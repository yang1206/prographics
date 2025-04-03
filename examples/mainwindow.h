#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QComboBox>
#include <QVBoxLayout>
#include <QLabel>
#include "prographics/charts/prpd/prpd.h"
#include "prographics/charts/prps/prps.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void generateTestData();

private:
    // 数据生成模式
    enum class DataGenerationMode {
        RANDOM_AMPLITUDE, // 随机幅值模式
        SMOOTH_CHANGING, // 平滑变化模式
        STANDARD_PD // 标准放电模式
    };

    QTabWidget *m_tabWidget;
    ProGraphics::PRPSChart *m_prpsChart;
    ProGraphics::PRPDChart *m_prpdChart;
    QTimer m_dataTimer;
    DataGenerationMode m_dataGenerationMode = DataGenerationMode::RANDOM_AMPLITUDE;

    std::vector<float> generateStandardPDPattern() const;

    std::vector<float> generateRandomAmplitudePattern() const;

    std::vector<float> generateSmoothlyChangingPattern() const;
};

#endif // MAINWINDOW_H
