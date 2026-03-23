#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QSocketNotifier>
#include <QListWidget>
#include <QListWidgetItem>
#include <QDialog>
#include <QGridLayout>
#include <QResizeEvent>
#include "v4l2_device.h"

class ImageViewerDialog : public QDialog {
    Q_OBJECT
public:
    ImageViewerDialog(const QStringList &paths, int currentIndex, QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void onPrevClicked();
    void onNextClicked();

private:
    void showImage(int index);
    void updateButtonState();

    QStringList m_paths;
    int m_currentIndex;
    QLabel *m_label;
    QPushButton *m_leftBtn;
    QPushButton *m_rightBtn;
    QPushButton *m_exitBtn;
    QPoint m_startPos;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateFrame();
    void captureImage();
    void handleSigInt();
    void onFileItemClicked(QListWidgetItem *item);
    void onExitButtonClicked();

private:
    QWidget *centralWidget;
    QLabel *videoLabel;
    QLabel *fpsLabel;
    QPushButton *captureButton;
    QPushButton *exitButton;
    QListWidget *fileListWidget;
    QHBoxLayout *mainLayout;

    V4L2Device *camera;
    QSocketNotifier *frameNotifier;
    QImage currentImage;
    QImage currentRawImage;
    QSocketNotifier *sigIntNotifier;

    QSize scaledSize;

    int frameCount;
    qint64 lastFpsUpdateTime;
    qint64 lastFrameTime;
    qint64 lastDisplayTime;

    struct LatencyStats {
        qint64 total_io;
        qint64 total_process;
        qint64 total_display;
        qint64 total_frames;
        int stat_frame_count;
    } latency_stats;
};

#endif // MAINWINDOW_H
