#ifndef SENSOR_WORKER_H
#define SENSOR_WORKER_H

#include <QObject>
#include <QMetaType>
#include <QString>

#include "linkage_logic.h"

enum class SensorReadStatus {
    Unavailable,
    Valid,
    Error,
};

struct SensorUiState {
    int ir = 0;
    int als = 0;
    int ps = 0;
    SensorReadStatus ap3216cStatus = SensorReadStatus::Unavailable;

    bool motionDetected = false;
    SensorReadStatus motionStatus = SensorReadStatus::Unavailable;

    float temperature = 0.0f;
    float humidity = 0.0f;
    SensorReadStatus dht11Status = SensorReadStatus::Unavailable;
};

Q_DECLARE_METATYPE(SensorUiState)
Q_DECLARE_METATYPE(LinkageDecision)

class QTimer;

class SensorWorker : public QObject
{
    Q_OBJECT
public:
    explicit SensorWorker(const QString &ap3216cDevice,
                          const QString &dht11Device,
                          const QString &sr501Device,
                          QObject *parent = nullptr);
    ~SensorWorker();

public slots:
    void start();
    void stop();
    void applyManualLedState(bool ledOn, long long nowMs);
    void syncLedState(bool ledOn);
    void recordAutoCameraStarted();
    void recordManualCameraStarted();
    void recordCameraStopped();

signals:
    void sensorUiStateReady(const SensorUiState &state);
    void decisionReady(const LinkageDecision &decision);

private slots:
    void pollSensors();

private:
    void openDevices();
    void closeDevices();

    bool readAp3216c(SensorUiState &state);
    bool readSr501(SensorUiState &state);
    void maybeReadDht11(SensorUiState &state);

    QString ap3216cDevice_;
    QString dht11Device_;
    QString sr501Device_;

    int ap3216cFd_ = -1;
    int dht11Fd_ = -1;
    int sr501Fd_ = -1;

    QTimer *pollTimer_ = nullptr;
    int dht11Counter_ = 0;

    float cachedTemperature_ = 0.0f;
    float cachedHumidity_ = 0.0f;
    bool hasDht11Sample_ = false;
    SensorReadStatus dht11Status_ = SensorReadStatus::Unavailable;

    SensorSnapshot latestSnapshot_{};
    bool hasAlsSample_ = false;
    bool hasMotionSample_ = false;
    LinkageState linkageState_{};
};

#endif // SENSOR_WORKER_H
