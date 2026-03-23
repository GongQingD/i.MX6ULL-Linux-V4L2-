#include "mainwindow.h"
#include <QApplication>
#include <signal.h>
#include <QDebug>
#include <QFontDatabase>
#include <QFont>

void handleSignal(int sig) {
    if (sig == SIGINT) {
        QApplication::quit();
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
    QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL, false);

    QApplication a(argc, argv);

    QFontDatabase::addApplicationFont("/usr/share/fonts/ttf/SourceHanSansCN-Regular.otf");
    a.setFont(QFont("Source Han Sans CN", 20));

    signal(SIGINT, handleSignal);

    MainWindow w;
    w.setWindowTitle("V4L2 Camera Project");
    w.setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    w.showFullScreen();
    return a.exec();
}
