#include "mainwindow.h"
#include <QApplication>
#include <signal.h>
#include <QDebug>
#include <QFontDatabase>
#include <QFont>

// 信号处理函数
void handleSignal(int sig) {
    if (sig == SIGINT) {
        // 退出 Qt 事件循环，这将导致 main 函数中的 a.exec() 返回
        QApplication::quit();
    }
}

int main(int argc, char *argv[])
{
    // Qt渲染优化：启用硬件加速和性能优化属性
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QCoreApplication::setAttribute(Qt::AA_UseOpenGLES); // 嵌入式系统使用OpenGL ES
    QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL, false); // 禁用软件OpenGL

    QApplication a(argc, argv);

    // 设置字体路径
    QFontDatabase::addApplicationFont("/usr/share/fonts/ttf/SourceHanSansCN-Regular.otf");
    a.setFont(QFont("Source Han Sans CN", 20));

    // 注册 SIGINT (Ctrl+C) 信号
    signal(SIGINT, handleSignal);

    MainWindow w;
    w.setWindowTitle("V4L2 Camera Project");
    
    // 设置窗口标志：无边框 | 总是置顶
    // 这强制窗口位于最上层，拦截所有触摸事件
    w.setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    
    // 使用全屏显示代替 resize/show
    w.showFullScreen();
    
    // 当 a.exec() 返回时，w 超出作用域
    // w 的析构函数会被调用 -> delete camera -> V4L2Device 析构 -> 释放 mmap 和 fd
    return a.exec();
}
