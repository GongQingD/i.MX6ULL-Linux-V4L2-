#include "mainwindow.h"
#include <QMessageBox>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QMouseEvent>
#include <QApplication> // 必须包含：修复 incomplete type 'QApplication' 错误
#include <QScreen>      // 必须包含：用于 primaryScreen()
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>

// --- ImageViewerDialog 实现 ---

ImageViewerDialog::ImageViewerDialog(const QStringList &paths, int currentIndex, QWidget *parent)
    : QDialog(parent), m_paths(paths), m_currentIndex(currentIndex)
{
    // 全屏、无边框、置顶、接受触摸
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_AcceptTouchEvents);
    setStyleSheet("background-color: black;");

    // 使用 QGridLayout 将控件层覆盖在图片层之上
    QGridLayout *mainLayout = new QGridLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // 层 0: 图片显示 (位于底层)
    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    // 将 label 放入 (0,0)
    mainLayout->addWidget(m_label, 0, 0);

    // 层 1: 控制按钮 (位于顶层)
    QWidget *controlPage = new QWidget(this);
    controlPage->setStyleSheet("background: transparent;"); // 背景透明
    
    QVBoxLayout *controlLayout = new QVBoxLayout(controlPage);
    
    // 中间部分：左右箭头
    QHBoxLayout *arrowLayout = new QHBoxLayout();
    m_leftBtn = new QPushButton("<", this);
    m_rightBtn = new QPushButton(">", this);
    
    // 设置箭头样式：半透明白色圆底，黑色箭头
    QString arrowStyle = "QPushButton { background-color: rgba(255, 255, 255, 150); color: black; border-radius: 30px; font-size: 30px; font-weight: bold; } QPushButton:pressed { background-color: rgba(255, 255, 255, 220); }";
    m_leftBtn->setFixedSize(60, 60);
    m_leftBtn->setStyleSheet(arrowStyle);
    m_rightBtn->setFixedSize(60, 60);
    m_rightBtn->setStyleSheet(arrowStyle);

    arrowLayout->addWidget(m_leftBtn);
    arrowLayout->addStretch(); // 中间弹簧，把按钮推向两边
    arrowLayout->addWidget(m_rightBtn);

    // 底部部分：退出按钮
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch(); // 左侧弹簧，把按钮推向右边
    m_exitBtn = new QPushButton("退出", this);
    m_exitBtn->setFixedSize(100, 50);
    m_exitBtn->setStyleSheet("QPushButton { background-color: #d9534f; color: white; font-size: 18px; border-radius: 10px; border: 2px solid white; } QPushButton:pressed { background-color: #c9302c; }");
    bottomLayout->addWidget(m_exitBtn);

    controlLayout->addStretch(1); // 顶部弹簧
    controlLayout->addLayout(arrowLayout); // 中间箭头区域
    controlLayout->addStretch(1); // 底部弹簧
    controlLayout->addLayout(bottomLayout); // 底部退出按钮
    controlLayout->setContentsMargins(20, 20, 20, 20); // 设置边距

    // 将 controlPage 也放入 (0,0)，这样它会覆盖在 m_label 上
    mainLayout->addWidget(controlPage, 0, 0);

    // 连接信号
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
    // 到顶时隐藏左箭头，到底时隐藏右箭头
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

    // 保留滑动功能
    if (abs(dx) > swipeThreshold && abs(dx) > abs(dy)) {
        if (dx > 0) onPrevClicked(); // 向右滑 -> 上一张
        else onNextClicked();        // 向左滑 -> 下一张
    }
    // 移除了“点击任意位置关闭”的功能，现在必须点击退出按钮
}

// --- MainWindow 实现 ---
#include "mainwindow.h"
#include <QMessageBox>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QMouseEvent>
#include <QApplication> // 必须包含：修复 incomplete type 'QApplication' 错误
#include <QScreen>      // 必须包含：用于 primaryScreen()
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>

// 用于信号处理的 socket pair
static int sigIntFd[2];

// Unix 信号处理函数
void sigIntHandler(int)
{
    char a = 1;
    // 在信号处理函数中只能调用异步信号安全的函数，write 是安全的
    ::write(sigIntFd[0], &a, sizeof(a));
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      frameCount(0),
      lastFpsUpdateTime(0),
      lastFrameTime(0),
      lastDisplayTime(0),
      latency_stats({0, 0, 0, 0, 0})
{
    // 新增：设置窗口标志，无边框且置顶，这有助于防止点击穿透
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    
    // 1. 设置 UI
    centralWidget = new QWidget(this);
    
    // 开启自动填充背景，并设置为黑色
    centralWidget->setAutoFillBackground(true);
    QPalette pal = centralWidget->palette();
    pal.setColor(QPalette::Window, Qt::black);
    centralWidget->setPalette(pal);
    
    // 关键：设置属性，告诉系统该窗口完全不透明
    setAttribute(Qt::WA_OpaquePaintEvent);
    
    // --- 修改开始 ---
    // 移除或注释掉以下两行，允许 Qt 将触摸事件自动转换为鼠标事件，以便 QPushButton 正常工作
    // setAttribute(Qt::WA_AcceptTouchEvents);
    // this->setAttribute(Qt::WA_AcceptTouchEvents);
    // --- 修改结束 ---

    setCentralWidget(centralWidget);

    // --- 左侧布局 ---
    QVBoxLayout *leftLayout = new QVBoxLayout();

    // 1.1 摄像头显示区域 (左上)
    // 创建容器Widget来放置摄像头图像和FPS叠加
    QWidget *videoContainer = new QWidget(this);
    QVBoxLayout *videoContainerLayout = new QVBoxLayout(videoContainer);
    videoContainerLayout->setContentsMargins(0, 0, 0, 0);

    videoLabel = new QLabel("Camera Feed", videoContainer);
    // 修改：将 SizePolicy 设置为 Ignored。
    // 否则 setPixmap 会更新 label 的 sizeHint，导致布局不断尝试扩大 label 以适应图片，形成死循环。
    videoLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    videoLabel->setAlignment(Qt::AlignCenter);
    videoLabel->setStyleSheet("border: 1px solid gray; background-color: #333;");
    videoContainerLayout->addWidget(videoLabel);

    // 创建FPS显示标签，叠加在摄像头图像上方
    fpsLabel = new QLabel("FPS: --", videoContainer);
    fpsLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    fpsLabel->setStyleSheet("background-color: rgba(0, 0, 0, 180); color: #00FF00; font-size: 16px; font-weight: bold; padding: 5px; border-radius: 5px;");
    fpsLabel->setAttribute(Qt::WA_TranslucentBackground, false);
    // 设置为绝对定位，不受布局影响
    fpsLabel->setGeometry(10, 10, 140, 100); // 固定位置和大小（增大以显示多行延迟信息）
    fpsLabel->raise(); // 确保在最上层

    leftLayout->addWidget(videoContainer, 3); // 占据左侧 3/4 高度

    // 1.2 照片列表区域 (左下)
    fileListWidget = new QListWidget(this);
    fileListWidget->setStyleSheet("font-size: 16px;");
    connect(fileListWidget, &QListWidget::itemClicked, this, &MainWindow::onFileItemClicked);
    leftLayout->addWidget(fileListWidget, 1); // 占据左侧 1/4 高度

    // --- 右侧布局 ---
    QVBoxLayout *rightLayout = new QVBoxLayout();

    // 1.3 拍照按钮 (右侧上部)
    captureButton = new QPushButton("拍照", this);
    captureButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); // 填满右侧
    // 使用更明显的颜色对比：黄色背景，黑色边框，黑色文字
    captureButton->setStyleSheet("font-size: 32px; font-weight: bold; background-color: #FFD700; color: black; border: 5px solid black; border-radius: 15px; padding: 10px;");

    // 1.4 退出按钮 (右侧下部)
    exitButton = new QPushButton("退出", this);
    exitButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // 红色背景，白色文字
    exitButton->setStyleSheet("font-size: 32px; font-weight: bold; background-color: #d9534f; color: white; border: 5px solid white; border-radius: 15px; padding: 10px; margin-top: 10px;");

    // 将按钮添加到右侧布局
    rightLayout->addWidget(captureButton, 3); // 拍照按钮占3/4高度
    rightLayout->addWidget(exitButton, 1); // 退出按钮占1/4高度

    // --- 主布局 ---
    mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->addLayout(leftLayout, 3); // 左侧布局占 3/4 宽度
    mainLayout->addLayout(rightLayout, 1); // 右侧布局占 1/4 宽度

    // 2. 初始化摄像头
    camera = new V4L2Device();
    if (!camera->openDevice("/dev/video1")) {
        QMessageBox::critical(this, "Error", "Cannot open /dev/video1");
    } else {
        if (!camera->initDevice(640, 480)) {
            QMessageBox::critical(this, "Error", "Cannot init device");
        } else {
            camera->startCapturing();
            
            // ===== 关键改动：使用 QSocketNotifier 替代 QTimer =====
            int fd = camera->getFileDescriptor();
            frameNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
            connect(frameNotifier, &QSocketNotifier::activated, 
                    this, &MainWindow::updateFrame);
            // ===== 不再需要 timer->start(30) =====
        }
    }


    // 4. 连接按钮信号
    connect(captureButton, &QPushButton::clicked, this, &MainWindow::captureImage);
    connect(exitButton, &QPushButton::clicked, this, &MainWindow::onExitButtonClicked);

    // 新增：配置 Ctrl+C 信号处理
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigIntFd) == 0) {
        // 创建监听器，监听 sigIntFd[1] 的可读事件
        sigIntNotifier = new QSocketNotifier(sigIntFd[1], QSocketNotifier::Read, this);
        connect(sigIntNotifier, &QSocketNotifier::activated, this, &MainWindow::handleSigInt);

        // 安装信号处理器
        struct sigaction sig;
        sig.sa_handler = sigIntHandler;
        sigemptyset(&sig.sa_mask);
        sig.sa_flags = 0;
        sigaction(SIGINT, &sig, nullptr);
    }

    // 新增：强制全屏显示
    showFullScreen();
}

MainWindow::~MainWindow()
{
    if (frameNotifier) {
        frameNotifier->setEnabled(false);
        delete frameNotifier;
    }
    delete camera; // 正常退出时这里会被调用，关闭摄像头
}

// 新增：响应 Ctrl+C 信号
void MainWindow::handleSigInt()
{
    sigIntNotifier->setEnabled(false);
    char tmp;
    ::read(sigIntFd[1], &tmp, sizeof(tmp));

    // 退出应用程序，这将触发析构函数
    QApplication::quit();
}

void MainWindow::updateFrame()
{
    unsigned char *data = nullptr;
    size_t length = 0;

    // T1: 用户接收到帧通知的时间（毫秒）
    qint64 T1 = QDateTime::currentMSecsSinceEpoch();

    // 获取当前时间戳（毫秒）用于FPS计算
    qint64 currentTime = T1;

    // 获取帧数据（忽略驱动时间戳，使用相对时间）
    if (camera->getFrame(&data, &length) != -1) {
        // 更新帧计数
        frameCount++;

        // 计算帧间延时（处理延时）
        if (lastFrameTime > 0) {
            // 帧间隔可用于后续调试，暂时不显示
            // qint64 frameInterval = currentTime - lastFrameTime;

            // 每秒更新一次FPS显示
            if (lastFpsUpdateTime == 0) {
                lastFpsUpdateTime = currentTime;
            } else if (currentTime - lastFpsUpdateTime >= 1000) {
                // 计算FPS
                double fps = frameCount * 1000.0 / (currentTime - lastFpsUpdateTime);
                frameCount = 0;
                lastFpsUpdateTime = currentTime;

                // 计算平均延迟（如果至少有一帧）
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
                }

                // 更新FPS显示
                fpsLabel->setText(latencyText);

                // 根据帧率改变颜色
                if (fps >= 25) {
                    fpsLabel->setStyleSheet("background-color: rgba(0, 100, 0, 180); color: #00FF00; font-size: 16px; font-weight: bold; padding: 5px; border-radius: 5px;");
                } else if (fps >= 15) {
                    fpsLabel->setStyleSheet("background-color: rgba(100, 100, 0, 180); color: #FFFF00; font-size: 16px; font-weight: bold; padding: 5px; border-radius: 5px;");
                } else {
                    fpsLabel->setStyleSheet("background-color: rgba(100, 0, 0, 180); color: #FF0000; font-size: 16px; font-weight: bold; padding: 5px; border-radius: 5px;");
                }

                // 重置延迟统计
                latency_stats.total_io = 0;
                latency_stats.total_process = 0;
                latency_stats.total_display = 0;
                latency_stats.total_frames = 0;
                latency_stats.stat_frame_count = 0;
            }
        }

        lastFrameTime = currentTime;

        // T2: 图像处理开始时间
        qint64 T2 = QDateTime::currentMSecsSinceEpoch();

        // RGB565 对应 QImage::Format_RGB16
        // 使用原始数据构造 QImage，注意这里不进行拷贝，只是引用数据
        QImage rawImg(data, camera->getWidth(), camera->getHeight(), QImage::Format_RGB16);

        if (!rawImg.isNull()) {
            // 必须调用 copy() 进行深拷贝，因为 data 指向的 V4L2 缓冲区即将被 releaseFrame 释放
            currentImage = rawImg.copy();
        }

        // T3: 图像处理结束时间
        qint64 T3 = QDateTime::currentMSecsSinceEpoch();

        // 显示图像
        if (!currentImage.isNull()) {
            videoLabel->setPixmap(QPixmap::fromImage(currentImage).scaled(videoLabel->size(), Qt::KeepAspectRatio));
        }

        // T4: 显示完成时间
        qint64 T4 = QDateTime::currentMSecsSinceEpoch();

        // 内存累加统计（开销最小）
        // 计算IO等待时间：从上一帧显示完成到收到新帧通知的时间
        qint64 io_wait = (lastDisplayTime > 0) ? (T1 - lastDisplayTime) : 0;
        latency_stats.total_io += io_wait;         // IO等待时间
        latency_stats.total_process += (T3 - T2);  // 处理延迟：图像拷贝时间
        latency_stats.total_display += (T4 - T3);  // 显示延迟：Qt 显示时间
        latency_stats.stat_frame_count++;
        latency_stats.total_frames++;  // 保留字段，可用于其他统计

        // 更新上一帧显示完成时间，用于下一帧的IO等待计算
        lastDisplayTime = T4;

        camera->releaseFrame();
    }
}

void MainWindow::captureImage()
{
    if (!currentImage.isNull()) {
        QString savePath = "/media/figure/";
        QDir dir;
        // 如果目录不存在，则创建
        if (!dir.exists(savePath)) {
            dir.mkpath(savePath);
        }

        QString fileNameOnly = QString("capture_%1.jpg").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
        QString fullPath = savePath + fileNameOnly;
        
        if (currentImage.save(fullPath, "JPG")) {
            
            // 添加到列表
            QListWidgetItem *item = new QListWidgetItem(fileNameOnly);
            item->setData(Qt::UserRole, fullPath); // 存储完整路径以便打开
            fileListWidget->insertItem(0, item); // 插入到最前面

            // 保持只有5个
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
    // 1. 收集当前列表中所有图片的路径
    QStringList paths;
    int currentIndex = 0;
    
    for(int i = 0; i < fileListWidget->count(); ++i) {
        QListWidgetItem *it = fileListWidget->item(i);
        paths.append(it->data(Qt::UserRole).toString());
        // 找到用户点击的那张图片的索引
        if (it == item) {
            currentIndex = i;
        }
    }

    if (paths.isEmpty()) return;

    // 2. 启动自定义查看器
    ImageViewerDialog viewer(paths, currentIndex, this);
    viewer.showFullScreen();// 请求显示
    viewer.exec();// 模态运行（阻塞在这里直到关闭）
}

/* 退出按钮点击事件 - 关闭摄像头应用，返回智能家居界面 */
void MainWindow::onExitButtonClicked()
{
    qDebug() << "退出监控画面，返回智能家居界面";


    // 关闭摄像头
    if (camera) {
        camera->stopCapturing();
        camera->closeDevice();
    }

    // 关闭应用程序
    QApplication::quit();
}
