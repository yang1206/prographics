#include "mainwindow.h"
#include <QApplication>
#include <QMainWindow>
#include <QtGui/QSurfaceFormat>

int main(int argc, char *argv[]) {
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
 QSurfaceFormat format;
  format.setVersion(4, 1);
  format.setProfile(QSurfaceFormat::CoreProfile);
  format.setDepthBufferSize(24);
  format.setStencilBufferSize(8);
  format.setSamples(4);
  format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
  format.setSwapInterval(1);
  QSurfaceFormat::setDefaultFormat(format);

  QApplication a(argc, argv);

  MainWindow window;
  window.show();
  return QApplication::exec();
}
