#include "mainwindow.h"
#include <QApplication>
#include <QMainWindow>
#include <QtGui/QSurfaceFormat>

int main(int argc, char *argv[]) {
  // 创建一个 QSurfaceFormat 对象
  QSurfaceFormat format;
  // 设置所需的 OpenGL 版本为 4.1
  format.setVersion(4, 1);
  // 设置使用核心模式（Core Profile）
  format.setProfile(QSurfaceFormat::CoreProfile);
  // 将该格式设置为默认格式，全局生效
  QSurfaceFormat::setDefaultFormat(format);

  QApplication a(argc, argv);
  MainWindow window;
  window.show();
  return QApplication::exec();
}
