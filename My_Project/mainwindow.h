#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QProcess>
#include <QSocketNotifier>
#include <QtGlobal>
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
    void handleSr501State(int state);
    void processAutomaticActions();
    bool startCameraProcess(bool autoTriggered);
    void stopCameraProcess();
    bool syncLedStateFromDevice();
    bool setLedState(bool on);

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
    bool lastSr501State = false;
    bool hasSr501State = false;
    bool cameraAutoRunning = false;
    qint64 cameraAutoDeadlineMs = 0;
    bool ledAutoState = false;
    bool hasLedAutoState = false;
    int lastAlsValue = 0;
    bool hasAlsValue = false;
};
#endif // MAINWINDOW_H
