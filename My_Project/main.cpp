#include "mainwindow.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QFontDatabase::addApplicationFont("/usr/share/fonts/ttf/SourceHanSansCN-Regular.otf");
    a.setFont(QFont("Source Han Sans CN", 20));

    MainWindow w;
    w.setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    w.showFullScreen();
    return a.exec();
}
