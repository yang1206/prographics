#pragma once

#include <QElapsedTimer>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include "prographics/charts/prpd/prpd.h"
#include "prographics/charts/prps/prps.h"

class DataProcessor;

class PerformanceTest : public QWidget {
    Q_OBJECT

public:
    explicit PerformanceTest(QWidget *parent = nullptr);

    ~PerformanceTest() override = default;

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

    QThread *m_dataThread;
    DataProcessor *m_dataProcessor;

private slots:
    void updateFPS();

    void updateCharts(const std::vector<float> &data);

private:
    void setupUI();

    void validateSharedContext();
};
