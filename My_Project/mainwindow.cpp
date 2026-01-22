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

    /* Set window to fullscreen and on top */
    setWindowState(Qt::WindowFullScreen);
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    show();

    /* open_led */
    led_fd = open(led_drv.toStdString().c_str(), O_RDWR);
    if(led_fd < 0)
        printf("open led failed.");

    /* open_ap3216c */
    ap3216c_fd = open(ap3216c_drv.toStdString().c_str(), O_RDWR);
    if(ap3216c_fd < 0)
        printf("open ap3216c failed.");

    /* open_dht11 */
    dht11_fd = open(dht11_drv.toStdString().c_str(), O_RDWR);
    if(dht11_fd < 0)
        printf("open dht11 failed.");

    /* timer_init */
    ap3216c_timer = new QTimer();
    connect(ap3216c_timer, &QTimer::timeout, this, &MainWindow::ap3216c_timeout);
    ap3216c_timer->start(1000);  // 1000ms

    /* camera process init */
    cameraProcess = nullptr;
}

MainWindow::~MainWindow()
{
    if(led_fd >= 0)
        ::close(led_fd);

    if(ap3216c_fd >= 0)
        ::close(ap3216c_fd);

    if(dht11_fd >= 0)
        ::close(dht11_fd);

    // 清理camera进程
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

/* led control */
void MainWindow::on_pushButton_clicked()
{
    // 检查LED设备是否成功打开
    if(led_fd < 0) {
        printf("LED device not opened, fd=%d\n", led_fd);
        qDebug() << "LED设备未打开，请检查 /dev/led 是否存在";
        return;
    }
    
    // 读取当前LED状态
    ssize_t ret = read(led_fd, buf, 1);
    if(ret < 0) {
        printf("read LED failed: %s\n", strerror(errno));
        return;
    }
    
    // 翻转LED状态
    buf[0] = buf[0] == 1 ? 0 : 1;
    
    // 写入新状态
    ret = write(led_fd, buf, 1);
    if(ret < 0) {
        printf("write LED failed: %s\n", strerror(errno));
        return;
    }
    
    printf("LED state changed to: %d\n", buf[0]);
}

/* ap3216c timeout */
void MainWindow::ap3216c_timeout()
{
    static int dht11_count = 0; // 静态变量用于计数
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

    // 每3次（3000ms）执行一次dht11相关操作
    dht11_count++;
    if (dht11_count >= 3) {
        dht11_count = 0;
        
        // 检查DHT11设备是否成功打开
        if(dht11_fd < 0) {
            printf("DHT11 device not opened, fd=%d\n", dht11_fd);
            qDebug() << "DHT11设备未打开，请检查 /dev/dht11 是否存在";
            ui->label_tmp->setText("--");
            ui->label_hum->setText("--");
            return;
        }
        
        // 读取DHT11数据（5字节：湿度整数、湿度小数、温度整数、温度小数、校验和）
        unsigned char dht11_buf[5] = {0};
        lseek(dht11_fd, 0, SEEK_SET);
        ssize_t ret = read(dht11_fd, dht11_buf, 5);
        
        if(ret < 0) {
            printf("read DHT11 failed: %s\n", strerror(errno));
            qDebug() << "DHT11读取失败";
            ui->label_tmp->setText("ERR");
            ui->label_hum->setText("ERR");
            return;
        }
        
        // 解析温湿度数据
        float humidity = dht11_buf[0] + dht11_buf[1] * 0.1;
        float temperature = dht11_buf[2] + dht11_buf[3] * 0.1;
        
        printf("Humidity: %.1f%%, Temperature: %.1f°C\n", humidity, temperature);
        
        // 更新UI显示
        ui->label_hum->setText(QString::number(humidity, 'f', 1));
        ui->label_tmp->setText(QString::number(temperature, 'f', 1));
    }
}

/* 监控画面按钮点击事件 - 启动Camera_Project */
void MainWindow::on_pushButton_4_clicked()
{
    QString cameraApp = "/lib/modules/4.1.15-g3dc0a4b/Camera_Project";

    qDebug() << "启动监控画面：" << cameraApp;

    // 如果已有进程在运行，先终止
    if (cameraProcess && cameraProcess->state() == QProcess::Running) {
        cameraProcess->terminate();
        cameraProcess->waitForFinished(3000);
        delete cameraProcess;
        cameraProcess = nullptr;
    }

    // 创建QProcess启动Camera_Project
    cameraProcess = new QProcess(this);
    // 连接输出信号
    connect(cameraProcess, &QProcess::readyReadStandardError, [=]() {
        qDebug() << "Camera stderr:" << cameraProcess->readAllStandardError();
    });
    cameraProcess->start(cameraApp);

    if (cameraProcess->waitForStarted(3000)) {
        qDebug() << "Camera_Project启动成功";
        // 隐藏当前窗口
        this->hide();
        
        // 连接进程结束信号，当Camera_Project退出时重新显示主窗口
        connect(cameraProcess, SIGNAL(finished(int, QProcess::ExitStatus)),
                this, SLOT(handleCameraFinished(int, QProcess::ExitStatus)));
    } else {
        qDebug() << "Camera_Project启动失败：" << cameraProcess->errorString();
        QMessageBox::warning(this, "错误", "无法启动监控程序");
        delete cameraProcess;
        cameraProcess = nullptr;
    }
}
/* Camera_Project进程结束处理 - 重新显示主窗口 */
void MainWindow::handleCameraFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);

    qDebug() << "Camera_Project进程结束，exitCode:" << exitCode << "exitStatus:" << exitStatus;
    qDebug() << "重新显示智能家居主窗口";

    // 清理进程对象
    if (cameraProcess) {
        cameraProcess->deleteLater();
        cameraProcess = nullptr;
    }

    // 重新显示主窗口
    this->show();
    this->raise();  // 确保窗口在最上层
    this->activateWindow();  // 激活窗口
}
