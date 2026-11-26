#include "prographics/charts/line/line_chart.h"
#include <QApplication>
#include <QMouseEvent>
#include <QImage>
#include <QWheelEvent>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using ProGraphics::LineChart;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (false)
void near(double a, double b, double tolerance = 1e-6) { CHECK(std::isfinite(a) && std::abs(a-b) < tolerance); }
void mouse(LineChart& c, QEvent::Type type, QPointF p, Qt::MouseButton button,
           Qt::MouseButtons buttons, Qt::KeyboardModifiers mods = {}) {
    QMouseEvent e(type, p, c.mapToGlobal(p), button, buttons, mods);
    QApplication::sendEvent(&c, &e);
}
void wheel(LineChart& c, QPointF p, Qt::KeyboardModifiers mods = {}) {
    QWheelEvent e(p, c.mapToGlobal(p), {}, {0,120}, {}, mods, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&c, &e);
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    LineChart manual;
    CHECK(manual.setViewRange({100,200},{-5,5}));
    LineChart::Series initial; initial.id="initial"; initial.y={0,1};
    CHECK(manual.setSeries({initial})); near(manual.xRange().min,100);
    LineChart c;
    c.resize(1000, 600);
    CHECK(c.setViewRange({-1.56,20.29},{-1.56,20.29}));
    CHECK(c.config().ticks.x.customTicks && c.config().ticks.y.customTicks);
    const auto& regular=*c.config().ticks.y.customTicks;
    CHECK(regular.size()==5 && regular[0].text=="0" && regular[4].text=="20");
    near(regular[0].position,1.56/21.85);
    QVector<LineChart::Series> original;
    for(int channel=0;channel<8;++channel) {
        LineChart::Series raw; raw.id=QString::number(channel); raw.xStep=0.002;
        for(int i=0;i<2048;++i) raw.y.append(i==97?12345.6789:double(i%43)-channel);
        original.append(raw);
    }
    CHECK(c.setSeries(original)); CHECK(c.series().size()==8);
    for(int channel=0;channel<8;++channel) CHECK(c.series()[channel].y==original[channel].y);
    LineChart::Series uniform;
    uniform.id = "pulse"; uniform.xStart = 10; uniform.xStep = 2;
    uniform.y = {0, 1, 9, -2, 0};
    CHECK(c.setSeries({uniform}));
    CHECK(c.setViewRange({10,18}, {-10,10}));
    const auto pixel = c.mapFromChart({14,9}); CHECK(pixel);
    const auto data = c.mapToChart(*pixel); CHECK(data);
    near(data->x(),14); near(data->y(),9);
    const auto spike = c.sampleAt(*pixel, 3); CHECK(spike);
    CHECK(spike->seriesId == "pulse" && spike->index == 2);
    near(spike->x,14); near(spike->y,9);
    CHECK(!c.sampleAt(c.mapFromChart({14,0}).value(),3));
    const auto overlay=c.findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly).first();
    overlay->resize(c.size());
    const auto overlayImage=[&] {
        QImage image(c.size(),QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
        overlay->render(&image,QPoint(),QRegion(),QWidget::DrawChildren); return image;
    };
    const auto noHover=overlayImage();
    mouse(c,QEvent::MouseMove,*pixel,Qt::NoButton,{});
    CHECK(overlayImage()!=noHover);
    QEvent leave(QEvent::Leave); QApplication::sendEvent(&c,&leave);
    CHECK(c.setSeries({uniform}));
    CHECK(overlayImage()==noHover); // A new frame must not resurrect the old tooltip.

    // Zoom must preserve the data under the mouse, and the other axis.
    const QPointF anchor = c.mapFromChart({12,4}).value();
    wheel(c, anchor, Qt::ShiftModifier);
    near(c.mapToChart(anchor)->x(),12);
    near(c.yRange().min,-10); near(c.yRange().max,10);
    CHECK(c.xRange().max-c.xRange().min < 8);
    const auto x = c.xRange();
    wheel(c, anchor, Qt::ControlModifier);
    near(c.xRange().min,x.min); near(c.xRange().max,x.max);
    near(c.mapToChart(anchor)->y(),4);
    const auto before = c.xRange();
    const QPointF center = c.plotRect().center();
    mouse(c,QEvent::MouseButtonPress,center,Qt::LeftButton,Qt::LeftButton);
    mouse(c,QEvent::MouseMove,center+QPointF(50,20),Qt::NoButton,Qt::LeftButton);
    mouse(c,QEvent::MouseButtonRelease,center+QPointF(50,20),Qt::LeftButton,{});
    CHECK(c.xRange().min < before.min);
    auto panX=c.xRange(); auto panY=c.yRange();
    mouse(c,QEvent::MouseButtonPress,center,Qt::LeftButton,Qt::LeftButton,Qt::ControlModifier);
    mouse(c,QEvent::MouseMove,center+QPointF(50,20),Qt::NoButton,Qt::LeftButton,Qt::ControlModifier);
    mouse(c,QEvent::MouseButtonRelease,center+QPointF(50,20),Qt::LeftButton,{});
    near(c.xRange().min,panX.min); CHECK(c.yRange().min>panY.min);
    c.setInteractionAxes(Qt::Horizontal);
    panY=c.yRange(); wheel(c,c.plotRect().center()); near(c.yRange().min,panY.min);
    c.setInteractionAxes(Qt::Horizontal|Qt::Vertical);
    const auto preservedX = c.xRange(), preservedY = c.yRange();
    uniform.y[2] = 12;
    CHECK(c.setSeries({uniform}));
    near(c.xRange().min,preservedX.min); near(c.yRange().max,preservedY.max);

    // The irregular path uses actual sorted X values, including duplicate X.
    LineChart::Series irregular;
    irregular.id="irregular"; irregular.x={1,1.1,1.1,4,8}; irregular.y={1,2,7,-3,0};
    CHECK(c.setSeries({irregular}));
    CHECK(c.setViewRange({0,10},{-10,10}));
    auto hit = c.sampleAt(c.mapFromChart({1.1,7}).value(),2);
    CHECK(hit && hit->index == 2); near(hit->x,1.1);
    c.setCursorsEnabled(true);
    CHECK(c.setCursor(0,"irregular",2)); CHECK(c.setCursor(1,"irregular",3));
    near(c.cursor(0)->y,7); near(c.cursor(1)->x-c.cursor(0)->x,2.9);
    CHECK(c.setSeriesVisible("irregular",false));
    CHECK(!c.sampleAt(c.mapFromChart({1.1,7}).value(),2)); CHECK(!c.cursor(0));
    CHECK(c.setSeriesVisible("irregular",true));
    c.fitToData(); near(c.xRange().min,1); near(c.xRange().max,8);
    const auto validRange=c.xRange();
    CHECK(!c.setXRange({2,2})); near(c.xRange().min,validRange.min);
    irregular.x[1]=0; CHECK(!c.setSeries({irregular}));
    CHECK(c.series().first().x[1]==1.1); // Invalid batches are atomic.
    CHECK(!c.setSeries({uniform,uniform}));
    uniform.y[0]=std::numeric_limits<double>::quiet_NaN(); CHECK(!c.setSeries({uniform}));
    uniform.y[0]=0; uniform.xStep=0; CHECK(!c.setSeries({uniform}));
    uniform.xStart=1e12; uniform.xStep=0.001; uniform.y={-2,0,2};
    CHECK(c.setSeries({uniform})); c.fitToData();
    auto precise=c.sampleAt(c.mapFromChart({1e12+0.001,0}).value(),1);
    CHECK(precise && precise->index==1); near(precise->x,1e12+0.001);
    uniform.xStart=1e16; uniform.xStep=0.1; uniform.y={0,1,2,3,4,5,6,7,8,9};
    CHECK(c.setSeries({uniform})); CHECK(c.setViewRange({1e16-10,1e16+10},{-1,10}));
    auto repeated=c.sampleAt(c.mapFromChart({1e16,9}).value(),1);
    CHECK(repeated && repeated->index==9);
    uniform.xStart=0; uniform.xStep=1; uniform.y={-8.5e307,8.5e307};
    CHECK(c.setSeries({uniform})); c.fitToData();
    CHECK(c.yRange().min<=-8.5e307 && c.yRange().max>=8.5e307);
    CHECK(std::isfinite(c.yRange().max-c.yRange().min));
    c.clear(); CHECK(c.series().isEmpty()); CHECK(!c.cursor(0));
    c.fitToData(); CHECK(std::isfinite(c.xRange().min));
    std::cout << "LineChart data, mapping, zoom, pan, snapshot, cursors and empty-data checks passed\n";
}
