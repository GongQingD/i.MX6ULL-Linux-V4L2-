#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <myslide.h>
#include <QProcess>
#include <QMessageBox>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pushButton_clicked();
    void on_pushButton_4_clicked();
    void ap3216c_timeout();
    void handleCameraFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    Ui::MainWindow *ui;

    int led_fd, ap3216c_fd, dht11_fd;

    QString led_drv = "/dev/led";
    QString ap3216c_drv = "/dev/ap3216c";
    QString dht11_drv = "/dev/dht11";

    QTimer *ap3216c_timer;
    QProcess *cameraProcess;

    unsigned char buf[10];
};
#endif // MAINWINDOW_H
