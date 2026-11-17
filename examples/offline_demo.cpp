#include "offline_demo.h"
#include "prographics/charts/prpd/prpd.h"
#include "prographics/charts/prps/prps.h"
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QThread>
#include <QVBoxLayout>
#include <algorithm>

using namespace ProGraphics;

QWidget* createOfflineDemo() {
    auto* window = new QWidget;
    window->setWindowTitle(QStringLiteral("PRPD／PRPS 静态快照示例（演示数据）"));
    window->resize(1280, 700);
    auto* layout = new QVBoxLayout(window);
    auto* controls = new QHBoxLayout;
    auto* eventCount = new QSpinBox(window);
    eventCount->setRange(1, 1'000'000);
    eventCount->setValue(2514);
    eventCount->setSingleStep(1000);
    auto* regenerate = new QPushButton(QStringLiteral("生成数据"), window);
    auto* clear = new QPushButton(QStringLiteral("清空快照"), window);
    controls->addWidget(new QLabel(QStringLiteral("事件数量"), window));
    controls->addWidget(eventCount);
    controls->addWidget(regenerate);
    controls->addWidget(clear);
    layout->addLayout(controls);
    auto* charts = new QTabWidget(window);
    auto* prpd = new PRPDChart(charts);
    auto* prps = new PRPSChart(charts);
    auto* tooltipCheck = new QCheckBox(QStringLiteral("悬浮提示"), window);
    auto* legendCheck = new QCheckBox(QStringLiteral("颜色分度条"), window);
    tooltipCheck->setChecked(prpd->isStaticTooltipEnabled());
    legendCheck->setChecked(prpd->isStaticColorLegendEnabled());
    controls->addWidget(tooltipCheck);
    controls->addWidget(legendCheck);
    controls->addStretch();
    QObject::connect(tooltipCheck, &QCheckBox::toggled, prpd, &PRPDChart::setStaticTooltipEnabled);
    QObject::connect(legendCheck, &QCheckBox::toggled, prpd, &PRPDChart::setStaticColorLegendEnabled);
    charts->addTab(prpd, QStringLiteral("PRPD 相幅分布"));
    charts->addTab(prps, QStringLiteral("PRPS 事件序列"));
    charts->setCurrentIndex(1);
    layout->addWidget(charts, 1);
    auto* status = new QLabel(QStringLiteral("正在后台准备快照…"), window);
    status->setWordWrap(true);
    layout->addWidget(status);
    struct Result {
        PRPDHistogramSnapshot::Ptr histogram;
        PRPSEventSnapshot::Ptr events;
        QString error;
    };
    // 不等待 worker；窗口关闭会自动断开 finished 的 GUI 接收者，worker 自行完成及释放。
    auto generation = std::make_shared<unsigned>(0);
    auto load = [=] {
        const int count = eventCount->value();
        const unsigned requested = ++*generation;
        regenerate->setEnabled(false);
        eventCount->setEnabled(false);
        status->setText(QStringLiteral("正在后台准备快照…"));
        auto result = std::make_shared<Result>();
        auto* worker = QThread::create([result, count] {
            constexpr std::size_t phaseBins = PRPDConstants::PHASE_POINTS;
            constexpr std::size_t amplitudeBins = PRPDConstants::AMPLITUDE_BINS;
            PRPDHistogramData histogram;
            histogram.phaseBins = phaseBins;
            histogram.amplitudeBins = amplitudeBins;
            for (std::size_t i = 0; i <= phaseBins; ++i) histogram.phaseEdges.push_back(360.0 * i / phaseBins);
            for (std::size_t i = 0; i <= amplitudeBins; ++i) histogram.amplitudeEdges.push_back(-70.0 + 70.0 * i / amplitudeBins);
            histogram.counts.resize(phaseBins * amplitudeBins);
            PRPSEventSequenceData sequence;
            sequence.sequence = {0.0, static_cast<double>(std::max(1, count - 1)), QStringLiteral("事件序号"), {}};
            sequence.events.reserve(static_cast<std::size_t>(count));
            for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(count); ++i) {
                const auto phaseTenths = (i * 137) % 3600;
                const double phase = static_cast<double>(phaseTenths) / 10.0;
                const double amplitude = -65.0 + static_cast<double>((i * 19) % 60);
                sequence.events.push_back({phase, amplitude, static_cast<double>(i), 10000 + i});
                const auto p = static_cast<std::size_t>(phaseTenths * phaseBins / 3600);
                const auto a = static_cast<std::size_t>((amplitude + 70.0) / 70.0 * amplitudeBins);
                ++histogram.counts[p * amplitudeBins + a];
            }
            result->histogram = PRPDHistogramSnapshot::prepare(std::move(histogram), {}, &result->error);
            if (result->histogram) result->events = PRPSEventSnapshot::prepare(std::move(sequence), &result->error);
        });
        QObject::connect(worker, &QThread::finished, window, [=] {
            regenerate->setEnabled(true);
            eventCount->setEnabled(true);
            if (requested != *generation) return; // 后发请求或清空使旧结果失效
            if (!result->histogram || !result->events) { status->setText(result->error); return; }
            prpd->setStaticHistogram(result->histogram);
            prps->setStaticEvents(result->events);
            status->setText(QStringLiteral("%1×%2 原始网格；%3 条事件；PRPD 与 PRPS 使用同一批演示数据。单击 PRPS 竖线查看源 ID。")
                .arg(PRPDConstants::PHASE_POINTS).arg(PRPDConstants::AMPLITUDE_BINS).arg(count));
        }, Qt::QueuedConnection);
        QObject::connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        worker->start();
    };
    QObject::connect(regenerate, &QPushButton::clicked, window, load);
    QObject::connect(clear, &QPushButton::clicked, window, [=] {
        ++*generation;
        prpd->clearStaticHistogram(); prps->clearStaticEvents();
        status->setText(QStringLiteral("已清空快照"));
    });
    QObject::connect(prps, &PRPSChart::staticEventSelected, window, [=](quint64 id, quint64 index) {
        status->setText(QStringLiteral("源事件 ID：%1；快照索引：%2").arg(id).arg(index));
    });
    load();
    return window;
}
