#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication a(argc, argv);

    MainWindow window;
    window.show();
    return QApplication::exec();
}
