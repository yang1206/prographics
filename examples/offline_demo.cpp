#include "offline_demo.h"
#include "prographics/charts/prpd/prpd.h"
#include "prographics/charts/prps/prps.h"
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>

using namespace ProGraphics;

QWidget* createOfflineDemo() {
    auto* window = new QWidget;
    window->setWindowTitle(QStringLiteral("PRPD／PRPS 静态快照示例（演示数据）"));
    window->resize(1280, 700);
    auto* layout = new QVBoxLayout(window);
    auto* controls = new QHBoxLayout;
    auto* coarse = new QPushButton(QStringLiteral("替换为 36×32"), window);
    auto* fine = new QPushButton(QStringLiteral("替换为 200×128"), window);
    auto* clear = new QPushButton(QStringLiteral("清空快照"), window);
    controls->addWidget(coarse); controls->addWidget(fine); controls->addWidget(clear);
    layout->addLayout(controls);
    auto* charts = new QHBoxLayout;
    auto* prpd = new PRPDChart(window);
    auto* prps = new PRPSChart(window);
    prpd->setStaticPointSize(5.0f);
    auto* tooltipCheck = new QCheckBox(QStringLiteral("悬浮提示"), window);
    auto* legendCheck = new QCheckBox(QStringLiteral("颜色分度条"), window);
    tooltipCheck->setChecked(prpd->isStaticTooltipEnabled());
    legendCheck->setChecked(prpd->isStaticColorLegendEnabled());
    controls->addWidget(tooltipCheck);
    controls->addWidget(legendCheck);
    controls->addStretch();
    QObject::connect(tooltipCheck, &QCheckBox::toggled, prpd, &PRPDChart::setStaticTooltipEnabled);
    QObject::connect(legendCheck, &QCheckBox::toggled, prpd, &PRPDChart::setStaticColorLegendEnabled);
    charts->addWidget(prpd); charts->addWidget(prps);
    layout->addLayout(charts, 1);
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
    auto load = [=](std::size_t phaseBins, std::size_t amplitudeBins) {
        const unsigned requested = ++*generation;
        status->setText(QStringLiteral("正在后台准备快照…"));
        auto result = std::make_shared<Result>();
        auto* worker = QThread::create([result, phaseBins, amplitudeBins] {
            PRPDHistogramData histogram;
            histogram.phaseBins = phaseBins;
            histogram.amplitudeBins = amplitudeBins;
            for (std::size_t i = 0; i <= phaseBins; ++i) histogram.phaseEdges.push_back(360.0 * i / phaseBins);
            for (std::size_t i = 0; i <= amplitudeBins; ++i) histogram.amplitudeEdges.push_back(-70.0 + 70.0 * i / amplitudeBins);
            histogram.counts.resize(phaseBins * amplitudeBins);
            for (std::size_t p = 0; p < phaseBins; ++p) {
                for (std::size_t a = 0; a < amplitudeBins; ++a) {
                    if ((p + a) % 5 == 0) histogram.counts[p * amplitudeBins + a] = (p + 1) * (a + 1);
                }
            }
            CountColorOptions colors;
            colors.normalization = CountNormalization::Log1p;
            result->histogram = PRPDHistogramSnapshot::prepare(std::move(histogram), colors, &result->error);
            PRPSEventSequenceData sequence;
            sequence.sequence = {0.0, 127.0, QStringLiteral("事件序号"), {}};
            for (std::uint64_t i = 0; i < 256; ++i) {
                // 同一序号、同一相位保留两条不同幅值的原始事件，源 ID 分别关联。
                sequence.events.push_back({5.0 + (i / 2 % 36) * 10.0,
                    i % 2 == 0 ? -20.0 : -55.0, static_cast<double>(i / 2), 10000 + i});
            }
            if (result->histogram) result->events = PRPSEventSnapshot::prepare(std::move(sequence), &result->error);
        });
        QObject::connect(worker, &QThread::finished, window, [=] {
            if (requested != *generation) return; // 后发请求或清空使旧结果失效
            if (!result->histogram || !result->events) { status->setText(result->error); return; }
            prpd->setStaticHistogram(result->histogram);
            prps->setStaticEvents(result->events);
            status->setText(QStringLiteral("%1×%2 原始网格；256 条事件；悬停查询计数，单击 PRPS 竖线查看源 ID。")
                .arg(static_cast<qulonglong>(phaseBins)).arg(static_cast<qulonglong>(amplitudeBins)));
        }, Qt::QueuedConnection);
        QObject::connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        worker->start();
    };
    QObject::connect(coarse, &QPushButton::clicked, window, [=] { load(36, 32); });
    QObject::connect(fine, &QPushButton::clicked, window, [=] { load(200, 128); });
    QObject::connect(clear, &QPushButton::clicked, window, [=] {
        ++*generation;
        prpd->clearStaticHistogram(); prps->clearStaticEvents();
        status->setText(QStringLiteral("已清空快照"));
    });
    QObject::connect(prps, &PRPSChart::staticEventSelected, window, [=](quint64 id, quint64 index) {
        status->setText(QStringLiteral("源事件 ID：%1；快照索引：%2").arg(id).arg(index));
    });
    load(36, 32);
    return window;
}
