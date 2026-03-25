#include "mainwindow.h"
#include <QMessageBox>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QMouseEvent>
#include <QApplication>
#include <QScreen>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <QtGlobal>

ImageViewerDialog::ImageViewerDialog(const QStringList &paths, int currentIndex, QWidget *parent)
    : QDialog(parent), m_paths(paths), m_currentIndex(currentIndex)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_AcceptTouchEvents);
    setStyleSheet("background-color: black;");

    QGridLayout *mainLayout = new QGridLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_label, 0, 0);

    QWidget *controlPage = new QWidget(this);
    controlPage->setStyleSheet("background: transparent;");

    QVBoxLayout *controlLayout = new QVBoxLayout(controlPage);

    QHBoxLayout *arrowLayout = new QHBoxLayout();
    m_leftBtn = new QPushButton("<", this);
    m_rightBtn = new QPushButton(">", this);

    QString arrowStyle = "QPushButton { background-color: rgba(255, 255, 255, 150); color: black; border-radius: 30px; font-size: 30px; font-weight: bold; } QPushButton:pressed { background-color: rgba(255, 255, 255, 220); }";
    m_leftBtn->setFixedSize(60, 60);
    m_leftBtn->setStyleSheet(arrowStyle);
    m_rightBtn->setFixedSize(60, 60);
    m_rightBtn->setStyleSheet(arrowStyle);

    arrowLayout->addWidget(m_leftBtn);
    arrowLayout->addStretch();
    arrowLayout->addWidget(m_rightBtn);

    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    m_exitBtn = new QPushButton("退出", this);
    m_exitBtn->setFixedSize(100, 50);
    m_exitBtn->setStyleSheet("QPushButton { background-color: #d9534f; color: white; font-size: 18px; border-radius: 10px; border: 2px solid white; } QPushButton:pressed { background-color: #c9302c; }");
    bottomLayout->addWidget(m_exitBtn);

    controlLayout->addStretch(1);
    controlLayout->addLayout(arrowLayout);
    controlLayout->addStretch(1);
    controlLayout->addLayout(bottomLayout);
    controlLayout->setContentsMargins(20, 20, 20, 20);

    mainLayout->addWidget(controlPage, 0, 0);

    connect(m_leftBtn, &QPushButton::clicked, this, &ImageViewerDialog::onPrevClicked);
    connect(m_rightBtn, &QPushButton::clicked, this, &ImageViewerDialog::onNextClicked);
    connect(m_exitBtn, &QPushButton::clicked, this, &QDialog::accept);

    showImage(m_currentIndex);
}

void ImageViewerDialog::showImage(int index)
{
    if (index < 0 || index >= m_paths.size()) return;

    QPixmap pix(m_paths[index]);
    if (!pix.isNull()) {
        m_label->setPixmap(pix.scaled(QApplication::primaryScreen()->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        m_label->setText("无法加载图片");
        m_label->setStyleSheet("color: white; font-size: 24px;");
    }
    updateButtonState();
}

void ImageViewerDialog::updateButtonState()
{
    m_leftBtn->setVisible(m_currentIndex > 0);
    m_rightBtn->setVisible(m_currentIndex < m_paths.size() - 1);
}

void ImageViewerDialog::onPrevClicked()
{
    if (m_currentIndex > 0) {
        m_currentIndex--;
        showImage(m_currentIndex);
    }
}

void ImageViewerDialog::onNextClicked()
{
    if (m_currentIndex < m_paths.size() - 1) {
        m_currentIndex++;
        showImage(m_currentIndex);
    }
}

void ImageViewerDialog::mousePressEvent(QMouseEvent *event)
{
    m_startPos = event->pos();
}

void ImageViewerDialog::mouseReleaseEvent(QMouseEvent *event)
{
    int dx = event->pos().x() - m_startPos.x();
    int dy = event->pos().y() - m_startPos.y();
    int swipeThreshold = 50;

    if (abs(dx) > swipeThreshold && abs(dx) > abs(dy)) {
        if (dx > 0) onPrevClicked();
        else onNextClicked();
    }
}

static int sigIntFd[2];

void sigIntHandler(int)
{
    char a = 1;
    ::write(sigIntFd[0], &a, sizeof(a));
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      frameCount(0),
      lastFpsUpdateTime(0),
      lastFrameTime(0),
      lastDisplayTime(0),
      latency_stats({0, 0, 0, 0, 0}),
      scaledSize(0, 0),
      sessionId_(qEnvironmentVariable("PERF_SESSION_ID")),
      firstFrameLogged_(false)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    centralWidget = new QWidget(this);
    centralWidget->setAutoFillBackground(true);
    QPalette pal = centralWidget->palette();
    pal.setColor(QPalette::Window, Qt::black);
    centralWidget->setPalette(pal);

    setAttribute(Qt::WA_OpaquePaintEvent);
    setCentralWidget(centralWidget);

    QVBoxLayout *leftLayout = new QVBoxLayout();

    QWidget *videoContainer = new QWidget(this);
    videoContainer->setGeometry(0, 0, 768, 450);

    videoLabel = new QLabel("Camera Feed", videoContainer);
    videoLabel->setGeometry(0, 0, 768, 450);
    videoLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    videoLabel->setStyleSheet("border: none; background-color: #333;");
    videoLabel->setAttribute(Qt::WA_OpaquePaintEvent, true);

    fpsLabel = new QLabel("FPS: --", videoContainer);
    fpsLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    fpsLabel->setStyleSheet("background-color: rgba(0, 0, 0, 180); color: #00FF00; font-size: 16px; font-weight: bold; padding: 5px; border-radius: 5px;");
    fpsLabel->setAttribute(Qt::WA_TranslucentBackground, false);
    fpsLabel->setGeometry(10, 10, 140, 100);
    fpsLabel->raise();

    leftLayout->addWidget(videoContainer, 3);

    fileListWidget = new QListWidget(this);
    fileListWidget->setStyleSheet("font-size: 16px;");
    connect(fileListWidget, &QListWidget::itemClicked, this, &MainWindow::onFileItemClicked);
    leftLayout->addWidget(fileListWidget, 1);

    QVBoxLayout *rightLayout = new QVBoxLayout();

    captureButton = new QPushButton("拍照", this);
    captureButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    captureButton->setStyleSheet("font-size: 32px; font-weight: bold; background-color: #FFD700; color: black; border: 5px solid black; border-radius: 15px; padding: 10px;");

    exitButton = new QPushButton("退出", this);
    exitButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    exitButton->setStyleSheet("font-size: 32px; font-weight: bold; background-color: #d9534f; color: white; border: 5px solid white; border-radius: 15px; padding: 10px; margin-top: 10px;");

    rightLayout->addWidget(captureButton, 3);
    rightLayout->addWidget(exitButton, 1);

    mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->addLayout(leftLayout, 3);
    mainLayout->addLayout(rightLayout, 1);

    camera = new V4L2Device();

    if (!camera->openDevice("/dev/video1")) {
        QMessageBox::critical(this, "Error", "Cannot open /dev/video1");
    } else {
        if (!camera->initDevice(640, 480)) {
            QMessageBox::critical(this, "Error", "Cannot init device");
        } else {
            camera->startCapturing();
            logPerfEvent("camera_project_started");
            int fd = camera->getFileDescriptor();
            frameNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
            connect(frameNotifier, &QSocketNotifier::activated, this, &MainWindow::updateFrame);
        }
    }

    connect(captureButton, &QPushButton::clicked, this, &MainWindow::captureImage);
    connect(exitButton, &QPushButton::clicked, this, &MainWindow::onExitButtonClicked);

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigIntFd) == 0) {
        sigIntNotifier = new QSocketNotifier(sigIntFd[1], QSocketNotifier::Read, this);
        connect(sigIntNotifier, &QSocketNotifier::activated, this, &MainWindow::handleSigInt);

        struct sigaction sig;
        sig.sa_handler = sigIntHandler;
        sigemptyset(&sig.sa_mask);
        sig.sa_flags = 0;
        sigaction(SIGINT, &sig, nullptr);
    }

    showFullScreen();

    if (camera && videoLabel) {
        QSize labelSize = videoLabel->size();
        QSize imageSize(camera->getWidth(), camera->getHeight());
        scaledSize = imageSize.scaled(labelSize, Qt::KeepAspectRatio);
    }
}

MainWindow::~MainWindow()
{
    if (frameNotifier) {
        frameNotifier->setEnabled(false);
        delete frameNotifier;
    }
    delete camera;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    QSize labelSize = videoLabel->size();
    if (camera) {
        QSize imageSize(camera->getWidth(), camera->getHeight());
        scaledSize = imageSize.scaled(labelSize, Qt::KeepAspectRatio);
    }
}

void MainWindow::handleSigInt()
{
    sigIntNotifier->setEnabled(false);
    char tmp;
    ::read(sigIntFd[1], &tmp, sizeof(tmp));
    QApplication::quit();
}

void MainWindow::updateFrame()
{
    unsigned char *data = nullptr;
    size_t length = 0;

    qint64 T1 = QDateTime::currentMSecsSinceEpoch();
    qint64 currentTime = T1;

    if (camera->getFrame(&data, &length) != -1) {
        frameCount++;

        if (lastFrameTime > 0) {
            if (lastFpsUpdateTime == 0) {
                lastFpsUpdateTime = currentTime;
            } else if (currentTime - lastFpsUpdateTime >= 1000) {
                double fps = frameCount * 1000.0 / (currentTime - lastFpsUpdateTime);
                frameCount = 0;
                lastFpsUpdateTime = currentTime;

                QString latencyText = QString("FPS:%1").arg(fps, 0, 'f', 1);
                if (latency_stats.stat_frame_count > 0) {
                    double avg_io = (double)latency_stats.total_io / latency_stats.stat_frame_count;
                    double avg_process = (double)latency_stats.total_process / latency_stats.stat_frame_count;
                    double avg_display = (double)latency_stats.total_display / latency_stats.stat_frame_count;
                    latencyText = QString("FPS:%1\nI/O:%2ms\n处理:%3ms\n显示:%4ms")
                        .arg(fps, 0, 'f', 1)
                        .arg(avg_io, 0, 'f', 1)
                        .arg(avg_process, 0, 'f', 1)
                        .arg(avg_display, 0, 'f', 1);
                    logPerfEvent("camera_fps_window", {
                        {"fps", QString::number(fps, 'f', 2)},
                        {"io_ms", QString::number(avg_io, 'f', 2)},
                        {"process_ms", QString::number(avg_process, 'f', 2)},
                        {"display_ms", QString::number(avg_display, 'f', 2)}
                    });
                }

                fpsLabel->setText(latencyText);

                if (fps >= 25) {
                    fpsLabel->setStyleSheet("background-color: rgba(0, 100, 0, 180); color: #00FF00; font-size: 16px; font-weight: bold; padding: 5px; border-radius: 5px;");
                } else if (fps >= 15) {
                    fpsLabel->setStyleSheet("background-color: rgba(100, 100, 0, 180); color: #FFFF00; font-size: 16px; font-weight: bold; padding: 5px; border-radius: 5px;");
                } else {
                    fpsLabel->setStyleSheet("background-color: rgba(100, 0, 0, 180); color: #FF0000; font-size: 16px; font-weight: bold; padding: 5px; border-radius: 5px;");
                }

                latency_stats.total_io = 0;
                latency_stats.total_process = 0;
                latency_stats.total_display = 0;
                latency_stats.total_frames = 0;
                latency_stats.stat_frame_count = 0;
            }
        }

        lastFrameTime = currentTime;

        qint64 T2 = QDateTime::currentMSecsSinceEpoch();
        currentRawImage = QImage(data, camera->getWidth(), camera->getHeight(), QImage::Format_RGB16);
        qint64 T3 = QDateTime::currentMSecsSinceEpoch();

        if (!currentRawImage.isNull()) {
            if (scaledSize.isValid()) {
                videoLabel->setPixmap(QPixmap::fromImage(currentRawImage).scaled(scaledSize, Qt::KeepAspectRatio, Qt::FastTransformation));
            } else {
                videoLabel->setPixmap(QPixmap::fromImage(currentRawImage).scaled(videoLabel->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
            }
        }

        if (!firstFrameLogged_ && !currentRawImage.isNull()) {
            firstFrameLogged_ = true;
            logPerfEvent("camera_first_frame_displayed", {
                {"width", QString::number(camera->getWidth())},
                {"height", QString::number(camera->getHeight())}
            });
        }

        qint64 T4 = QDateTime::currentMSecsSinceEpoch();
        qint64 io_wait = (lastDisplayTime > 0) ? (T1 - lastDisplayTime) : 0;
        latency_stats.total_io += io_wait;
        latency_stats.total_process += (T3 - T2);
        latency_stats.total_display += (T4 - T3);
        latency_stats.stat_frame_count++;
        latency_stats.total_frames++;

        lastDisplayTime = T4;
        camera->releaseFrame();
    }
}

void MainWindow::captureImage()
{
    logPerfEvent("camera_capture_clicked");

    if (!currentRawImage.isNull()) {
        currentImage = currentRawImage.copy();
    }

    if (!currentImage.isNull()) {
        QString savePath = "/media/figure/";
        QDir dir;
        if (!dir.exists(savePath)) {
            dir.mkpath(savePath);
        }

        QString fileNameOnly = QString("capture_%1.jpg").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
        QString fullPath = savePath + fileNameOnly;

        if (currentImage.save(fullPath, "JPG")) {
            QListWidgetItem *item = new QListWidgetItem(fileNameOnly);
            item->setData(Qt::UserRole, fullPath);
            fileListWidget->insertItem(0, item);

            while (fileListWidget->count() > 5) {
                delete fileListWidget->takeItem(fileListWidget->count() - 1);
            }
        } else {
            QMessageBox::warning(this, "Error", "Failed to save image");
        }
    } else {
        QMessageBox::warning(this, "Warning", "No image to capture");
    }
}

void MainWindow::onFileItemClicked(QListWidgetItem *item)
{
    QStringList paths;
    int currentIndex = 0;

    for (int i = 0; i < fileListWidget->count(); ++i) {
        QListWidgetItem *it = fileListWidget->item(i);
        paths.append(it->data(Qt::UserRole).toString());
        if (it == item) {
            currentIndex = i;
        }
    }

    if (paths.isEmpty()) return;

    ImageViewerDialog viewer(paths, currentIndex, this);
    viewer.showFullScreen();
    viewer.exec();
}

void MainWindow::onExitButtonClicked()
{
    qDebug() << "退出监控画面，返回智能家居界面";

    logPerfEvent("camera_exit_clicked");

    if (camera) {
        camera->stopCapturing();
        camera->closeDevice();
    }

    QApplication::quit();
}

void MainWindow::logPerfEvent(const QString &event, const QList<PerfField> &fields)
{
    QList<PerfField> finalFields = fields;
    if (!sessionId_.isEmpty()) {
        finalFields.prepend({QStringLiteral("session_id"), sessionId_});
    }
    QString line = buildPerfEventLine("Camera_Project", event, finalFields);
    qInfo().noquote() << line;
}
