#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QProcess>
#include <QSocketNotifier>
#include <myslide.h>

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
    void handleSr501Notification();
    void handleCameraFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void applyDashboardStyle();
    void restoreDashboardWindow();
    void refreshDashboardSnapshot();
    bool setupSr501Async();
    void teardownSr501Async();
    void updateSr501Label();

    Ui::MainWindow *ui;

    int led_fd, ap3216c_fd, dht11_fd, sr501_fd;

    QString led_drv = "/dev/led";
    QString ap3216c_drv = "/dev/ap3216c";
    QString dht11_drv = "/dev/dht11";
    QString sr501_drv = "/dev/sr501";

    QTimer *ap3216c_timer;
    QProcess *cameraProcess;
    QSocketNotifier *sr501Notifier;

    unsigned char buf[10];
    int sr501SignalFds[2];
};
#endif // MAINWINDOW_H
