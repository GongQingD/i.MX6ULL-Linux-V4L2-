#include "sensor_worker.h"

#include <QDateTime>
#include <QTimer>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

namespace {
constexpr int kPollIntervalMs = 1000;
constexpr int kDht11PollEveryNTicks = 3;
}

SensorWorker::SensorWorker(const QString &ap3216cDevice,
                           const QString &dht11Device,
                           const QString &sr501Device,
                           QObject *parent)
    : QObject(parent),
      ap3216cDevice_(ap3216cDevice),
      dht11Device_(dht11Device),
      sr501Device_(sr501Device) {}

SensorWorker::~SensorWorker() {
    stop();
}

void SensorWorker::start() {
    if (pollTimer_) {
        return;
    }

    dht11Counter_ = 0;
    cachedTemperature_ = 0.0f;
    cachedHumidity_ = 0.0f;
    hasDht11Sample_ = false;
    dht11Status_ = SensorReadStatus::Unavailable;
    latestSnapshot_ = SensorSnapshot{};
    hasAlsSample_ = false;
    hasMotionSample_ = false;
    linkageState_ = LinkageState{};

    openDevices();

    pollTimer_ = new QTimer(this);
    pollTimer_->setInterval(kPollIntervalMs);
    connect(pollTimer_, &QTimer::timeout, this, &SensorWorker::pollSensors);
    pollTimer_->start();

    pollSensors();
}

void SensorWorker::stop() {
    if (pollTimer_) {
        pollTimer_->stop();
        delete pollTimer_;
        pollTimer_ = nullptr;
    }

    closeDevices();

    dht11Counter_ = 0;
    hasDht11Sample_ = false;
    dht11Status_ = SensorReadStatus::Unavailable;
    latestSnapshot_ = SensorSnapshot{};
    hasAlsSample_ = false;
    hasMotionSample_ = false;
    linkageState_ = LinkageState{};
}

void SensorWorker::applyManualLedState(bool ledOn, long long nowMs) {
    recordManualLedToggle(linkageState_, ledOn, nowMs);
}

void SensorWorker::syncLedState(bool ledOn) {
    linkageState_.ledOn = ledOn;
}

void SensorWorker::recordAutoCameraStarted() {
    linkageState_.cameraRunning = true;
    linkageState_.cameraOwner = CameraOwner::Auto;
}

void SensorWorker::recordManualCameraStarted() {
    recordManualCameraStart(linkageState_);
}

void SensorWorker::recordCameraStopped() {
    if (linkageState_.cameraOwner == CameraOwner::Manual) {
        recordManualCameraStop(linkageState_);
        return;
    }

    linkageState_.cameraRunning = false;
    linkageState_.cameraOwner = CameraOwner::None;
    linkageState_.autoCameraDeadlineMs = 0;
}

void SensorWorker::pollSensors() {
    SensorUiState state;
    state.dht11Status = dht11Status_;
    if (hasDht11Sample_) {
        state.temperature = cachedTemperature_;
        state.humidity = cachedHumidity_;
    }

    const bool apOk = readAp3216c(state);
    const bool motionOk = readSr501(state);
    maybeReadDht11(state);

    if (apOk) {
        latestSnapshot_.als = state.als;
        hasAlsSample_ = true;
    }
    if (motionOk) {
        latestSnapshot_.motionDetected = state.motionDetected;
        hasMotionSample_ = true;
    }

    LinkageDecision decision;
    if (hasAlsSample_ && hasMotionSample_) {
        const long long nowMs = QDateTime::currentMSecsSinceEpoch();
        decision = evaluateLinkage(linkageState_, latestSnapshot_, nowMs);
    }

    emit sensorUiStateReady(state);
    emit decisionReady(decision);
}

void SensorWorker::openDevices() {
    if (ap3216cFd_ < 0) {
        ap3216cFd_ = open(ap3216cDevice_.toStdString().c_str(), O_RDWR);
        if (ap3216cFd_ < 0) {
            printf("open ap3216c failed: %s\n", strerror(errno));
        }
    }

    if (dht11Fd_ < 0) {
        dht11Fd_ = open(dht11Device_.toStdString().c_str(), O_RDWR);
        if (dht11Fd_ < 0) {
            printf("open dht11 failed: %s\n", strerror(errno));
        }
    }

    if (sr501Fd_ < 0) {
        sr501Fd_ = open(sr501Device_.toStdString().c_str(), O_RDWR);
        if (sr501Fd_ < 0) {
            printf("open sr501 failed: %s\n", strerror(errno));
        }
    }
}

void SensorWorker::closeDevices() {
    if (ap3216cFd_ >= 0) {
        ::close(ap3216cFd_);
        ap3216cFd_ = -1;
    }

    if (dht11Fd_ >= 0) {
        ::close(dht11Fd_);
        dht11Fd_ = -1;
    }

    if (sr501Fd_ >= 0) {
        ::close(sr501Fd_);
        sr501Fd_ = -1;
    }
}

bool SensorWorker::readAp3216c(SensorUiState &state) {
    if (ap3216cFd_ < 0) {
        state.ap3216cStatus = SensorReadStatus::Unavailable;
        return false;
    }

    unsigned char buf[6] = {0};
    lseek(ap3216cFd_, 0, SEEK_SET);
    if (read(ap3216cFd_, buf, sizeof(buf)) != static_cast<ssize_t>(sizeof(buf))) {
        state.ap3216cStatus = SensorReadStatus::Error;
        return false;
    }

    const bool dataValid = ((buf[0] & 0x80) == 0) && ((buf[4] & 0x40) == 0);
    if (dataValid) {
        state.ir = (buf[1] << 2) | (buf[0] & 0x03);
        state.als = (buf[3] << 8) | buf[2];
        state.ps = ((buf[5] & 0x3F) << 4) | (buf[4] & 0x0F);
    } else {
        state.ir = 0;
        state.als = 0;
        state.ps = ((buf[5] & 0x3F) << 4) | (buf[4] & 0x0F);
    }

    state.ap3216cStatus = SensorReadStatus::Valid;
    return true;
}

bool SensorWorker::readSr501(SensorUiState &state) {
    if (sr501Fd_ < 0) {
        state.motionStatus = SensorReadStatus::Unavailable;
        return false;
    }

    char sr501State = 0;
    lseek(sr501Fd_, 0, SEEK_SET);
    if (read(sr501Fd_, &sr501State, 1) != 1) {
        state.motionStatus = SensorReadStatus::Error;
        return false;
    }

    state.motionDetected = (sr501State != 0);
    state.motionStatus = SensorReadStatus::Valid;
    return true;
}

void SensorWorker::maybeReadDht11(SensorUiState &state) {
    dht11Counter_++;
    if (dht11Counter_ < kDht11PollEveryNTicks) {
        return;
    }
    dht11Counter_ = 0;

    if (dht11Fd_ < 0) {
        dht11Status_ = SensorReadStatus::Unavailable;
        state.dht11Status = dht11Status_;
        return;
    }

    unsigned char dht11Buf[5] = {0};
    lseek(dht11Fd_, 0, SEEK_SET);
    if (read(dht11Fd_, dht11Buf, sizeof(dht11Buf)) != static_cast<ssize_t>(sizeof(dht11Buf))) {
        dht11Status_ = SensorReadStatus::Error;
        state.dht11Status = dht11Status_;
        return;
    }

    cachedHumidity_ = static_cast<float>(dht11Buf[0]) + dht11Buf[1] * 0.1f;
    cachedTemperature_ = static_cast<float>(dht11Buf[2]) + dht11Buf[3] * 0.1f;
    hasDht11Sample_ = true;
    dht11Status_ = SensorReadStatus::Valid;

    state.humidity = cachedHumidity_;
    state.temperature = cachedTemperature_;
    state.dht11Status = dht11Status_;
}
