#include "mainwindow.h"
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
#include <thread>
#include <errno.h>
#include <termios.h>
#include <stdlib.h>
#include <linux/input.h>
#include <sys/ioctl.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowState(Qt::WindowFullScreen);
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    show();

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
    }

    ap3216c_timer = new QTimer();
    connect(ap3216c_timer, &QTimer::timeout, this, &MainWindow::ap3216c_timeout);
    ap3216c_timer->start(1000);

    cameraProcess = nullptr;
}

MainWindow::~MainWindow()
{
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

    printf("LED state changed to: %d\n", buf[0]);
}

void MainWindow::ap3216c_timeout()
{
    static int dht11_count = 0;
    unsigned short ir, als, ps;

    lseek(ap3216c_fd, 0, SEEK_SET);
    read(ap3216c_fd, buf, 6);

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

    if (sr501_fd >= 0) {
        char sr501_state = 0;
        lseek(sr501_fd, 0, SEEK_SET);
        if (read(sr501_fd, &sr501_state, 1) == 1) {
            ui->label_people->setNum(sr501_state != 0 ? 1 : 0);
        } else {
            ui->label_people->setText("ERR");
        }
    } else {
        ui->label_people->setText("--");
    }

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

void MainWindow::on_pushButton_4_clicked()
{
    QString cameraApp = "/lib/modules/4.1.15-g3dc0a4b/Camera_Project";

    qDebug() << "启动监控画面：" << cameraApp;

    if (cameraProcess && cameraProcess->state() == QProcess::Running) {
        cameraProcess->terminate();
        cameraProcess->waitForFinished(3000);
        delete cameraProcess;
        cameraProcess = nullptr;
    }

    cameraProcess = new QProcess(this);
    connect(cameraProcess, &QProcess::readyReadStandardError, [=]() {
        qDebug() << "Camera stderr:" << cameraProcess->readAllStandardError();
    });
    cameraProcess->start(cameraApp);

    if (cameraProcess->waitForStarted(3000)) {
        qDebug() << "Camera_Project启动成功";
        this->hide();

        connect(cameraProcess, SIGNAL(finished(int, QProcess::ExitStatus)),
                this, SLOT(handleCameraFinished(int, QProcess::ExitStatus)));
    } else {
        qDebug() << "Camera_Project启动失败：" << cameraProcess->errorString();
        QMessageBox::warning(this, "错误", "无法启动监控程序");
        delete cameraProcess;
        cameraProcess = nullptr;
    }
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

    this->show();
    this->raise();
    this->activateWindow();
}
