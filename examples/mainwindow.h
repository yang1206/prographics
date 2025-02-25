#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "prographics/charts/prpd/prpd.h"
#include "prographics/charts/prps/prps.h"
#include <QMainWindow>
#include <QTimer>

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);

private slots:
  void generateTestData();

private:
  QTabWidget *m_tabWidget;
  ProGraphics::PRPSChart *m_prpsChart;
  ProGraphics::PRPDChart *m_prpdChart;
  QTimer m_dataTimer;

  std::vector<float> generateStandardPDPattern() const;
};

#endif // MAINWINDOW_H
