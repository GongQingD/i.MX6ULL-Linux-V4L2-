#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMouseEvent> // 新增
#include <QEvent> // 新增
#include <QTouchEvent> // 新增
#include <QSocketNotifier> 
#include <QListWidget> 
#include <QListWidgetItem> 
#include <QDialog> 
#include <QGridLayout> // 新增
#include "v4l2_device.h"

// 新增：自定义图片查看器对话框类，支持滑动切换
class ImageViewerDialog : public QDialog {
    Q_OBJECT
public:
    ImageViewerDialog(const QStringList &paths, int currentIndex, QWidget *parent = nullptr);

protected:
    // 重写鼠标/触摸事件以检测滑动
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
    MainWindow(QWidget *parent = nullptr);// 构造函数
    ~MainWindow();// 析构函数

protected:

private slots:
    void updateFrame();
    void captureImage();
    void handleSigInt(); // 新增：处理 Ctrl+C 信号
    void onFileItemClicked(QListWidgetItem *item); // 新增：点击文件列表
    void onExitButtonClicked(); // 新增：退出按钮点击

private:
    QWidget *centralWidget;
    QLabel *videoLabel;
    QPushButton *captureButton;
    QPushButton *exitButton; // 新增：退出按钮
    QListWidget *fileListWidget; // 新增：文件列表
    // QVBoxLayout *rightLayout; // 删除：不再需要单独的右侧布局
    QHBoxLayout *mainLayout;

    V4L2Device *camera;
    QSocketNotifier *frameNotifier;  // 替代 QTimer *timer
    QImage currentImage;
    QSocketNotifier *sigIntNotifier; // 新增
};

#endif // MAINWINDOW_H
