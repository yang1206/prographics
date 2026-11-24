#include "prographics/charts/prpd/prpd.h"

#include <QApplication>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <cmath>
#include <cstdlib>
#include <iostream>

using ProGraphics::Coordinate2D;
using ProGraphics::PRPDChart;
using Mode = Coordinate2D::SelectionMode;

#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << "line " << __LINE__ << ": " << #expression << '\n'; std::exit(EXIT_FAILURE); \
} } while (false)

namespace {
void near(double actual, double expected) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > 0.0001) {
        std::cerr << "Expected " << expected << ", got " << actual << '\n';
        std::exit(EXIT_FAILURE);
    }
}

class TrackingChart : public Coordinate2D {
public:
    int presses = 0, moves = 0, releases = 0, wheels = 0, returns = 0;

protected:
    void mousePressEvent(QMouseEvent*) override { ++presses; }
    void mouseMoveEvent(QMouseEvent*) override { ++moves; }
    void mouseReleaseEvent(QMouseEvent*) override { ++releases; }
    void wheelEvent(QWheelEvent*) override { ++wheels; }
    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Return) ++returns;
    }
};

void configurePlot(Coordinate2D& chart) {
    chart.resize(700, 700);
    auto config = chart.config();
    config.displayMode = Coordinate2D::DisplayMode::Stretch;
    config.margin.left = config.margin.right = config.margin.top = config.margin.bottom = 1;
    chart.setConfig(config);
}

void mouse(Coordinate2D& chart, QEvent::Type type, const QPointF& position,
           Qt::MouseButton button, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QMouseEvent event(type, position, chart.mapToGlobal(position), button, buttons, modifiers);
    QApplication::sendEvent(&chart, &event);
}

void press(Coordinate2D& chart, const QPointF& at, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    mouse(chart, QEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton, modifiers);
}

void move(Coordinate2D& chart, const QPointF& at) {
    mouse(chart, QEvent::MouseMove, at, Qt::NoButton, Qt::LeftButton);
}

void release(Coordinate2D& chart, const QPointF& at) {
    mouse(chart, QEvent::MouseButtonRelease, at, Qt::LeftButton, Qt::NoButton);
}

void checkRectangle(const Coordinate2D& chart, double x1, double y1, double x2, double y2) {
    const auto region = chart.selection();
    CHECK(region && std::holds_alternative<QRectF>(*region));
    const auto& rectangle = std::get<QRectF>(*region);
    near(rectangle.left(), x1);
    near(rectangle.top(), y1);
    near(rectangle.right(), x2);
    near(rectangle.bottom(), y2);
}

void rectangleSelectionAndEvents() {
    TrackingChart chart;
    configurePlot(chart);
    CHECK(chart.selectionMode() == Mode::Disabled);
    int finished = 0, canceled = 0, cleared = 0;
    QObject::connect(&chart, &Coordinate2D::selectionFinished, [&](const auto& result) {
        CHECK(chart.selection() == result);
        ++finished;
    });
    QObject::connect(&chart, &Coordinate2D::selectionCanceled, [&] { ++canceled; });
    QObject::connect(&chart, &Coordinate2D::selectionCleared, [&] { ++cleared; });

    press(chart, {475, 225});
    move(chart, {225, 475});
    release(chart, {225, 475});
    CHECK(chart.presses == 1 && chart.moves == 1 && chart.releases == 1 && finished == 0);
    CHECK(!chart.selection());

    chart.setSelectionMode(Mode::Rectangle);
    const auto plot = chart.plotRect();
    near(plot.left(), 100);
    near(plot.top(), 100);
    near(plot.width(), 500);
    near(plot.height(), 500);

    // Outside and modified gestures keep reaching the underlying chart.
    press(chart, {40, 45});
    move(chart, {52, 65});
    release(chart, {52, 65});
    press(chart, {475, 225}, Qt::ShiftModifier);
    release(chart, {225, 475});
    CHECK(chart.presses == 3 && chart.moves == 2 && chart.releases == 3);

    press(chart, {475, 225});
    move(chart, {225, 475});
    CHECK(!chart.selection() && finished == 0);
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(&chart, &enter);
    CHECK(chart.returns == 1 && finished == 0);
    QWheelEvent wheel({350, 350}, {350, 350}, {}, {0, 120}, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&chart, &wheel);
    CHECK(chart.wheels == 1);
    release(chart, {225, 475});
    CHECK(chart.presses == 3 && chart.moves == 2 && chart.releases == 3 && finished == 1);
    checkRectangle(chart, 90, -63.75, 270, -41.25);

    press(chart, {225, 475});
    move(chart, {450, 500});
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&chart, &escape);
    release(chart, {450, 500});
    CHECK(escape.isAccepted() && canceled == 1 && finished == 1 && chart.releases == 3);
    checkRectangle(chart, 90, -63.75, 270, -41.25);

    // Programmatic finish uses the current draft. The following release is consumed once.
    press(chart, {350, 350});
    move(chart, {690, 30});
    CHECK(chart.finishSelection());
    release(chart, {690, 30});
    CHECK(finished == 2 && chart.releases == 3);
    checkRectangle(chart, 180, -52.5, 360, -30);

    // Geometry and ranges can change after a selection. The stored data stays fixed.
    auto config = chart.config();
    config.margin.left = config.margin.right = config.margin.top = config.margin.bottom = 0;
    chart.setConfig(config);
    chart.setTicksRange('y', -100, 0, 10);
    const auto projected = chart.mapFromChart({180, -52.5});
    CHECK(projected);
    near(projected->x(), 350);
    near(projected->y(), 367.5);
    checkRectangle(chart, 180, -52.5, 360, -30);

    press(chart, {340, 345});
    release(chart, {340, 345});
    CHECK(canceled == 2 && finished == 2);
    press(chart, {340, 345});
    chart.setSelectionMode(Mode::Disabled);
    release(chart, {340, 345});
    CHECK(canceled == 3 && chart.releases == 3);
    chart.clearSelection();
    chart.clearSelection();
    CHECK(cleared == 1 && !chart.selection());

    // Focus loss can route that button release elsewhere. A new modified
    // press must still reach the chart and own its full sequence.
    chart.setSelectionMode(Mode::Rectangle);
    press(chart, {340, 345});
    QFocusEvent lostFocus(QEvent::FocusOut);
    QApplication::sendEvent(&chart, &lostFocus);
    press(chart, {340, 345}, Qt::ShiftModifier);
    move(chart, {480, 455});
    release(chart, {480, 455});
    CHECK(canceled == 4 && chart.presses == 4 && chart.moves == 3 && chart.releases == 4);
}

void polygonSelectionAndDegeneracy() {
    TrackingChart chart;
    configurePlot(chart);
    chart.setSelectionMode(Mode::Polygon);
    const QPointF start(200, 500);
    press(chart, start);
    move(chart, {201, 501});
    move(chart, {500, 500});
    move(chart, {500, 200});
    release(chart, start);
    const auto result = chart.selection();
    CHECK(result && std::holds_alternative<QPolygonF>(*result));
    const auto& polygon = std::get<QPolygonF>(*result);
    CHECK(polygon.size() == 3);
    near(polygon.first().x(), 72);
    near(polygon.first().y(), -66);
    CHECK(polygon.containsPoint({240, -60}, Qt::OddEvenFill));
    CHECK(!polygon.containsPoint({72, -40}, Qt::OddEvenFill));

    press(chart, start);
    move(chart, {300, 400});
    move(chart, {400, 300});
    release(chart, {500, 200});
    CHECK(chart.selection() == result);

    // A two-lobed path has nonzero odd-even area even when signed area cancels.
    press(chart, {200, 200});
    move(chart, {500, 500});
    move(chart, {500, 200});
    release(chart, {200, 500});
    CHECK(chart.selection() != result);
    CHECK(std::get<QPolygonF>(*chart.selection()).size() == 4);
}

void staticPrpdUsesSnapshotDataCoordinates() {
    PRPDChart chart;
    configurePlot(chart);
    ProGraphics::PRPDHistogramData data;
    data.phaseBins = data.amplitudeBins = 2;
    data.phaseEdges = {11.250000123, 80.5, 352.75};
    data.amplitudeEdges = {-82.125, -62.5, -12.75};
    data.counts = {1, 2, 3, 4};
    const auto snapshot = ProGraphics::PRPDHistogramSnapshot::prepare(data);
    CHECK(snapshot && chart.setStaticHistogram(snapshot));

    const auto bottomLeft = chart.mapToChart({100, 600});
    const auto topRight = chart.mapToChart({600, 100});
    CHECK(bottomLeft && topRight);
    near(bottomLeft->x(), 11.25);
    near(bottomLeft->y(), -82.125);
    near(topRight->x(), 352.75);
    near(topRight->y(), -12.75);
    const auto exactEdge = chart.mapToChart(chart.plotRect().bottomLeft());
    CHECK(exactEdge && std::abs(exactEdge->x() - 11.250000123) < 1e-12);
    const auto cell = chart.histogramCellAt({350, 350});
    CHECK(cell && cell->phaseBin == 1 && cell->amplitudeBin == 1 && cell->count == 4);

    chart.setSelectionMode(Mode::Rectangle);
    press(chart, {225, 475});
    release(chart, {475, 225});
    checkRectangle(chart, 96.625, -64.78125, 267.375, -30.09375);
    CHECK(chart.setStaticHistogram(snapshot) && chart.selection());

    const auto replacement = ProGraphics::PRPDHistogramSnapshot::prepare(data);
    CHECK(replacement && chart.setStaticHistogram(replacement) && !chart.selection());
    press(chart, {225, 475});
    release(chart, {475, 225});
    CHECK(chart.selection());
    chart.clearStaticHistogram();
    CHECK(!chart.selection());
    press(chart, {225, 475});
    release(chart, {475, 225});
    CHECK(!chart.selection());
    CHECK(chart.setStaticHistogram(snapshot));
    press(chart, {225, 475});
    release(chart, {475, 225});
    chart.useRealtimeData();
    CHECK(!chart.selection());
    const auto realtime = chart.mapToChart({350, 350});
    CHECK(realtime);
    near(realtime->x(), 180);
    near(realtime->y(), -52.5);
}
} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    rectangleSelectionAndEvents();
    polygonSelectionAndDegeneracy();
    staticPrpdUsesSnapshotDataCoordinates();
}
