#pragma once
#include "prographics/charts/line/line_chart.h"
#include <functional>

// 示例侧测量，不进入库组件的热路径。
class DemoLineChart final : public ProGraphics::LineChart {
public:
    using ProGraphics::LineChart::LineChart;
    std::function<void(double)> paintMeasured;
protected:
    void paintGL() override {
        QElapsedTimer clock; clock.start();
        ProGraphics::LineChart::paintGL();
        if (paintMeasured) paintMeasured(clock.nsecsElapsed()/1e6);
    }
};
QVector<ProGraphics::LineChart::Series> makeLineDemoSeries(bool irregular, double phase);
QWidget* createLineChartDemo();
QWidget* createLineChartDemo(quint32 randomSeed);
void startLineChartAcceptance(QWidget* demo, const QString& reportPath);
struct LineDemoFeedStats { quint64 submitted=0,displayed=0,rejected=0; bool pending=false; };
LineDemoFeedStats lineDemoFeedStats(QWidget* demo);
