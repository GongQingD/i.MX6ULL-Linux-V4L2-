#include "mainwindow.h"
#include "sr501_async.h"
#include "ui_mainwindow.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <QDebug>
#include <QMouseEvent>
#include <QTimer>
#include <QProcess>
#include <QApplication>
#include <QDateTime>
#include <QPalette>
#include <thread>
#include <errno.h>
#include <termios.h>
#include <stdlib.h>
#include <signal.h>
#include <linux/input.h>
#include <sys/ioctl.h>

namespace {

constexpr int kAlsDarkThreshold = 80;
constexpr int kAlsBrightThreshold = 120;
constexpr qint64 kMotionHoldMs = 5000;

int g_sr501SignalWriteFd = -1;

void sr501SigioHandler(int)
{
    if (g_sr501SignalWriteFd < 0) {
        return;
    }

    const char pending = 1;
    const ssize_t ret = ::write(g_sr501SignalWriteFd, &pending, sizeof(pending));
    (void)ret;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    applyDashboardStyle();
    setAttribute(Qt::WA_OpaquePaintEvent);
    ui->label_ir->setText("--");
    ui->label_light->setText("--");
    ui->label_dis->setText("--");
    ui->label_tmp->setText("--");
    ui->label_hum->setText("--");
    ui->label_people->setText("--");

    led_fd = open(led_drv.toStdString().c_str(), O_RDWR);
    if (led_fd < 0)
        printf("open led failed.");

    ap3216c_fd = open(ap3216c_drv.toStdString().c_str(), O_RDWR);
    if (ap3216c_fd < 0)
        printf("open ap3216c failed.");

    dht11_fd = open(dht11_drv.toStdString().c_str(), O_RDWR);
    if (dht11_fd < 0)
        printf("open dht11 failed.");

    sr501_fd = open(sr501_drv.toStdString().c_str(), O_RDWR);
    if (sr501_fd < 0) {
        printf("open sr501 failed.");
        ui->label_people->setText("--");
    } else {
        updateSr501Label();
    }

    ap3216c_timer = new QTimer();
    connect(ap3216c_timer, &QTimer::timeout, this, &MainWindow::ap3216c_timeout);
    ap3216c_timer->start(1000);

    cameraProcess = nullptr;
    sr501Notifier = nullptr;
    sr501SignalFds[0] = -1;
    sr501SignalFds[1] = -1;
    syncLedStateFromDevice();

    if (sr501_fd >= 0 && !setupSr501Async()) {
        qDebug() << "SR501 async notification setup failed, label will not auto-refresh";
    }

    refreshDashboardSnapshot();

    QTimer::singleShot(0, this, [this]() { refreshDashboardSnapshot(); });
}

MainWindow::~MainWindow()
{
    teardownSr501Async();

    if (led_fd >= 0)
        ::close(led_fd);

    if (ap3216c_fd >= 0)
        ::close(ap3216c_fd);

    if (dht11_fd >= 0)
        ::close(dht11_fd);

    if (sr501_fd >= 0)
        ::close(sr501_fd);

    if (cameraProcess) {
        if (cameraProcess->state() == QProcess::Running) {
            cameraProcess->terminate();
            cameraProcess->waitForFinished(3000);
        }
        delete cameraProcess;
        cameraProcess = nullptr;
    }

    delete ui;
}

void MainWindow::on_pushButton_clicked()
{
    if (led_fd < 0) {
        printf("LED device not opened, fd=%d\n", led_fd);
        qDebug() << "LED设备未打开，请检查 /dev/led 是否存在";
        return;
    }

    ssize_t ret = read(led_fd, buf, 1);
    if (ret < 0) {
        printf("read LED failed: %s\n", strerror(errno));
        return;
    }

    buf[0] = buf[0] == 1 ? 0 : 1;

    ret = write(led_fd, buf, 1);
    if (ret < 0) {
        printf("write LED failed: %s\n", strerror(errno));
        return;
    }

    hasLedAutoState = true;
    ledAutoState = (buf[0] == 1);
    printf("LED state changed to: %d\n", buf[0]);
}

void MainWindow::ap3216c_timeout()
{
    static int dht11_count = 0;
    unsigned short ir, als, ps;

    if (ap3216c_fd < 0) {
        ui->label_ir->setText("--");
        ui->label_light->setText("--");
        ui->label_dis->setText("--");
    } else {
        lseek(ap3216c_fd, 0, SEEK_SET);
        if (read(ap3216c_fd, buf, 6) != 6) {
            ui->label_ir->setText("ERR");
            ui->label_light->setText("ERR");
            ui->label_dis->setText("ERR");
        } else {
            int is_data_valid = ((buf[0] & 0x80) == 0) && ((buf[4] & 0x40) == 0);

            if (is_data_valid) {
                ir = (buf[1] << 2) | (buf[0] & 0x03);
                als = (buf[3] << 8) | buf[2];
                ps = ((buf[5] & 0x3F) << 4) | (buf[4] & 0x0F);
                printf("IR: %u, ALS: %u, PS: %u\n", ir, als, ps);
            } else {
                ir = 0;
                als = 0;
                ps = ((buf[5] & 0x3F) << 4) | (buf[4] & 0x0F);
                printf("valid: %d, IR: %u, ALS: %u, PS: %u\n", is_data_valid, ir, als, ps);
            }

            ui->label_ir->setNum(ir);
            ui->label_light->setNum(als);
            ui->label_dis->setNum(ps);
            lastAlsValue = als;
            hasAlsValue = true;
        }
    }

    processAutomaticActions();

    dht11_count++;
    if (dht11_count >= 3) {
        dht11_count = 0;

        if (dht11_fd < 0) {
            printf("DHT11 device not opened, fd=%d\n", dht11_fd);
            qDebug() << "DHT11设备未打开，请检查 /dev/dht11 是否存在";
            ui->label_tmp->setText("--");
            ui->label_hum->setText("--");
            return;
        }

        unsigned char dht11_buf[5] = {0};
        lseek(dht11_fd, 0, SEEK_SET);
        ssize_t ret = read(dht11_fd, dht11_buf, 5);

        if (ret < 0) {
            printf("read DHT11 failed: %s\n", strerror(errno));
            qDebug() << "DHT11读取失败";
            ui->label_tmp->setText("ERR");
            ui->label_hum->setText("ERR");
            return;
        }

        float humidity = dht11_buf[0] + dht11_buf[1] * 0.1;
        float temperature = dht11_buf[2] + dht11_buf[3] * 0.1;

        printf("Humidity: %.1f%%, Temperature: %.1f°C\n", humidity, temperature);

        ui->label_hum->setText(QString::number(humidity, 'f', 1));
        ui->label_tmp->setText(QString::number(temperature, 'f', 1));
    }
}

void MainWindow::applyDashboardStyle()
{
    if (ui->centralwidget) {
        ui->centralwidget->setAutoFillBackground(true);
        QPalette palette = ui->centralwidget->palette();
        palette.setColor(QPalette::Window, QColor(245, 245, 245));
        palette.setColor(QPalette::WindowText, QColor(20, 20, 20));
        ui->centralwidget->setPalette(palette);
    }

    setStyleSheet(
        "QWidget#centralwidget { background-color: rgb(245, 245, 245); color: rgb(20, 20, 20); }"
        "QLabel { color: rgb(20, 20, 20); background: transparent; }"
        "QPushButton { color: rgb(20, 20, 20); }"
        "QLabel#label { color: rgb(20, 20, 20); background-color: rgb(114, 159, 207); }");
}

void MainWindow::restoreDashboardWindow()
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    showFullScreen();
    raise();
    activateWindow();

    if (centralWidget()) {
        centralWidget()->show();
        centralWidget()->update();
        centralWidget()->repaint();
    }

    update();
    repaint();
    QApplication::processEvents();
}

void MainWindow::refreshDashboardSnapshot()
{
    ap3216c_timeout();
    updateSr501Label();

    if (centralWidget()) {
        centralWidget()->update();
    }

    update();
}

bool MainWindow::setupSr501Async()
{
    if (sr501_fd < 0) {
        return false;
    }

    if (!Sr501Async::createSignalPipe(sr501SignalFds)) {
        qDebug() << "create sr501 signal pipe failed:" << strerror(errno);
        return false;
    }

    g_sr501SignalWriteFd = sr501SignalFds[0];

    sr501Notifier = new QSocketNotifier(sr501SignalFds[1], QSocketNotifier::Read, this);
    connect(sr501Notifier, &QSocketNotifier::activated, this, &MainWindow::handleSr501Notification);

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sr501SigioHandler;
    sigemptyset(&action.sa_mask);
    if (::sigaction(SIGIO, &action, nullptr) < 0) {
        qDebug() << "sigaction(SIGIO) failed:" << strerror(errno);
        teardownSr501Async();
        return false;
    }

    if (!Sr501Async::configureAsyncNotification(sr501_fd, ::getpid())) {
        qDebug() << "configure SR501 async notification failed:" << strerror(errno);
        teardownSr501Async();
        return false;
    }

    return true;
}

void MainWindow::teardownSr501Async()
{
    if (sr501_fd >= 0) {
        int flags = ::fcntl(sr501_fd, F_GETFL);
        if (flags >= 0) {
            ::fcntl(sr501_fd, F_SETFL, flags & ~O_ASYNC);
        }
    }

    ::signal(SIGIO, SIG_DFL);
    g_sr501SignalWriteFd = -1;

    if (sr501Notifier) {
        sr501Notifier->setEnabled(false);
        delete sr501Notifier;
        sr501Notifier = nullptr;
    }

    Sr501Async::closeSignalPipe(sr501SignalFds);
}

void MainWindow::updateSr501Label()
{
    if (sr501_fd < 0) {
        ui->label_people->setText("--");
        return;
    }

    int state = 0;
    if (Sr501Async::readDeviceState(sr501_fd, state)) {
        handleSr501State(state);
    } else {
        ui->label_people->setText("ERR");
    }
}

void MainWindow::handleSr501Notification()
{
    if (!sr501Notifier) {
        return;
    }

    sr501Notifier->setEnabled(false);
    Sr501Async::drainSignalPipe(sr501SignalFds[1]);
    updateSr501Label();
    sr501Notifier->setEnabled(true);
}

void MainWindow::handleSr501State(int state)
{
    hasSr501State = true;
    lastSr501State = (state != 0);
    ui->label_people->setNum(state);

    if (!lastSr501State) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    cameraAutoDeadlineMs = nowMs + kMotionHoldMs;

    if (cameraProcess && cameraProcess->state() == QProcess::Running) {
        return;
    }

    startCameraProcess(true);
}

void MainWindow::processAutomaticActions()
{
    if (hasAlsValue && led_fd >= 0) {
        if (!hasLedAutoState) {
            syncLedStateFromDevice();
        }

        if (hasLedAutoState) {
            if (!ledAutoState && lastAlsValue <= kAlsDarkThreshold) {
                setLedState(true);
            } else if (ledAutoState && lastAlsValue >= kAlsBrightThreshold) {
                setLedState(false);
            }
        }
    }

    if (cameraAutoRunning &&
        (!cameraProcess || cameraProcess->state() != QProcess::Running)) {
        cameraAutoRunning = false;
        cameraAutoDeadlineMs = 0;
    }

    if (cameraAutoRunning &&
        hasSr501State &&
        !lastSr501State &&
        QDateTime::currentMSecsSinceEpoch() >= cameraAutoDeadlineMs) {
        stopCameraProcess();
    }
}

bool MainWindow::startCameraProcess(bool autoTriggered)
{
    QString cameraApp = "/lib/modules/4.1.15-g3dc0a4b/Camera_Project";

    qDebug() << (autoTriggered ? "自动启动监控画面：" : "手动启动监控画面：") << cameraApp;

    if (cameraProcess && cameraProcess->state() == QProcess::Running) {
        return true;
    }

    if (cameraProcess) {
        cameraProcess->deleteLater();
        cameraProcess = nullptr;
    }

    cameraProcess = new QProcess(this);
    connect(cameraProcess, &QProcess::readyReadStandardError, [=]() {
        qDebug() << "Camera stderr:" << cameraProcess->readAllStandardError();
    });
    connect(cameraProcess, SIGNAL(finished(int, QProcess::ExitStatus)),
            this, SLOT(handleCameraFinished(int, QProcess::ExitStatus)));
    cameraProcess->start(cameraApp);

    if (cameraProcess->waitForStarted(3000)) {
        qDebug() << "Camera_Project启动成功";
        cameraAutoRunning = autoTriggered;
        if (!autoTriggered) {
            cameraAutoDeadlineMs = 0;
        }
        this->hide();
        return true;
    }

    qDebug() << "Camera_Project启动失败：" << cameraProcess->errorString();
    if (!autoTriggered) {
        QMessageBox::warning(this, "错误", "无法启动监控程序");
    }
    cameraAutoRunning = false;
    cameraAutoDeadlineMs = 0;
    delete cameraProcess;
    cameraProcess = nullptr;
    return false;
}

void MainWindow::stopCameraProcess()
{
    cameraAutoRunning = false;
    cameraAutoDeadlineMs = 0;

    if (!cameraProcess || cameraProcess->state() != QProcess::Running) {
        return;
    }

    cameraProcess->terminate();
    cameraProcess->waitForFinished(3000);
}

bool MainWindow::syncLedStateFromDevice()
{
    if (led_fd < 0) {
        return false;
    }

    ssize_t ret = read(led_fd, buf, 1);
    if (ret < 0) {
        qDebug() << "read LED state failed:" << strerror(errno);
        return false;
    }

    hasLedAutoState = true;
    ledAutoState = (buf[0] == 1);
    return true;
}

bool MainWindow::setLedState(bool on)
{
    if (led_fd < 0) {
        return false;
    }

    buf[0] = on ? 1 : 0;
    ssize_t ret = write(led_fd, buf, 1);
    if (ret < 0) {
        qDebug() << "write LED state failed:" << strerror(errno);
        return false;
    }

    hasLedAutoState = true;
    ledAutoState = on;
    qDebug() << "auto LED state changed to:" << (on ? 1 : 0);
    return true;
}

void MainWindow::on_pushButton_4_clicked()
{
    startCameraProcess(false);
}

void MainWindow::handleCameraFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);

    qDebug() << "Camera_Project进程结束，exitCode:" << exitCode << "exitStatus:" << exitStatus;
    qDebug() << "重新显示智能家居主窗口";

    if (cameraProcess) {
        cameraProcess->deleteLater();
        cameraProcess = nullptr;
    }

    cameraAutoRunning = false;
    cameraAutoDeadlineMs = 0;

    restoreDashboardWindow();
    refreshDashboardSnapshot();

    QTimer::singleShot(0, this, [this]() {
        restoreDashboardWindow();
        refreshDashboardSnapshot();
    });

    QTimer::singleShot(100, this, [this]() {
        restoreDashboardWindow();
        refreshDashboardSnapshot();
    });
}
