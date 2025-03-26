#pragma once
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QElapsedTimer>
#include "prographics/charts/prpd/prpd.h"
#include "prographics/charts/prps/prps.h"

class DataProcessor;

class PerformanceTest : public QWidget {
    Q_OBJECT

public:
    explicit PerformanceTest(QWidget *parent = nullptr);
    ~PerformanceTest() override;

private:
    struct ChartPair {
        ProGraphics::PRPSChart *prps;
        ProGraphics::PRPDChart *prpd;
        QGroupBox *container;
    };

    std::vector<ChartPair> m_chartPairs;
    QTimer m_fpsTimer;
    QLabel *m_fpsLabel;
    int m_frameCount = 0;

    // 性能监控
    QElapsedTimer m_performanceTimer;
    double m_lastDataProcessTime = 0.0;
    double m_lastRenderTime = 0.0;

    // 数据处理器
    DataProcessor *m_dataProcessor = nullptr;

private slots:
    void updateFPS();
    void updateCharts(const std::vector<float> &data);
    void validateSharedContext();

private:
    void setupUI();
};
