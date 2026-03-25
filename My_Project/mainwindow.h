#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSocketNotifier>
#include <QTextStream>
#include <QtGlobal>
#include <myslide.h>

#include "perf_event.h"
#include "linkage_logic.h"

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
    void evaluateAndApplyLinkage(bool allowCameraDecision, bool allowLedDecision);
    bool startCameraProcess(bool autoTriggered);
    void stopCameraProcess();
    bool syncLedStateFromDevice();
    bool setLedState(bool on);
    void logPerfEvent(const QString &event,
                      const QList<PerfKv> &fields = QList<PerfKv>());
    int reserveCameraSession(bool autoTriggered);

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
    LinkageState linkageState_{};
    SensorSnapshot sensorSnapshot_{};
    bool hasMotionSample_ = false;
    bool hasAlsSample_ = false;
    qint64 currentDarkTriggerTsMs_ = 0;
    int nextCameraSessionId_ = 0;
    int pendingCameraSessionId_ = 0;
    int activeCameraSessionId_ = 0;
};
#endif // MAINWINDOW_H
