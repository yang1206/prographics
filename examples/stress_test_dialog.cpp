#include "stress_test_dialog.h"
#include "prographics/charts/prpd/prpd.h"
#include "prographics/charts/prps/prps.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QPushButton>
#include <QSaveFile>
#include <QScreen>
#include <QSpinBox>
#include <QSysInfo>
#include <QTableWidget>
#include <QThread>
#include <QTimer>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>
#include <vector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

namespace {
constexpr qint64 Second = 1000000000;
constexpr int BankSize = 512;

// 固定大小的分段直方图，长时间压测也不保存无限增长的逐样本数组。
struct DurationStats {
    std::array<quint64, 4601> bins{};
    quint64 count = 0;
    long double totalNs = 0;
    qint64 maximumNs = 0;

    void clear() {
        bins.fill(0);
        count = 0;
        totalNs = 0;
        maximumNs = 0;
    }
    void add(qint64 ns) {
        ns = std::max<qint64>(0, ns);
        const qint64 us = ns / 1000;
        size_t index;
        if (us < 1000) index = static_cast<size_t>(us);
        else if (us < 10000) index = 1000 + static_cast<size_t>((us - 1000) / 10);
        else if (us < 100000) index = 1900 + static_cast<size_t>((us - 10000) / 100);
        else if (us < 1000000) index = 2800 + static_cast<size_t>((us - 100000) / 1000);
        else if (us < 10000000) index = 3700 + static_cast<size_t>((us - 1000000) / 10000);
        else index = 4600;
        ++bins[index];
        ++count;
        totalNs += ns;
        maximumNs = std::max(maximumNs, ns);
    }
    double meanMs() const { return count ? static_cast<double>(totalNs / count / 1000000) : 0; }
    double maxMs() const { return static_cast<double>(maximumNs) / 1000000; }
    double percentile(double fraction) const {
        if (!count) return 0;
        const quint64 rank = static_cast<quint64>(std::ceil(static_cast<double>(count) * fraction));
        quint64 accumulated = 0;
        for (size_t i = 0; i < bins.size(); ++i) {
            accumulated += bins[i];
            if (accumulated < rank) continue;
            // 返回桶上界；超过 10 秒的末桶使用实际最大值作为保守上界。
            if (i < 1000) return static_cast<double>(i + 1) / 1000;
            if (i < 1900) return (1000.0 + (i - 999) * 10) / 1000;
            if (i < 2800) return (10000.0 + (i - 1899) * 100) / 1000;
            if (i < 3700) return (100000.0 + (i - 2799) * 1000) / 1000;
            if (i < 4600) return (1000000.0 + (i - 3699) * 10000) / 1000;
            return maxMs();
        }
        return maxMs();
    }
};

struct PaintProbe {
    virtual ~PaintProbe() = default;
    virtual void setProfiling(bool enabled) = 0;
    virtual bool graphicsReady() const = 0;
};

template<class Chart>
class MeasuredChart final : public Chart, public PaintProbe {
public:
    std::function<void(qint64, const ProGraphics::BaseGLWidget::FrameRenderMetrics&)> onPaint;
    explicit MeasuredChart(QWidget* parent) : Chart(parent) {}
    void setProfiling(bool enabled) override {
        m_profiling = enabled;
        this->setRenderProfilingEnabled(enabled);
    }
    bool graphicsReady() const override { return m_ready; }
protected:
    void initializeGL() override {
        const auto* context = QOpenGLContext::currentContext();
        m_ready = context && context->isValid() && context->format().version() >= qMakePair(4, 1);
        if (m_ready) Chart::initializeGL();
    }
    void resizeGL(int width, int height) override {
        if (m_ready) Chart::resizeGL(width, height);
    }
    void paintGL() override {
        if (!m_ready) return;
        if (!m_profiling) {
            Chart::paintGL();
            return;
        }
        QElapsedTimer timer;
        timer.start();
        Chart::paintGL();
        const qint64 elapsed = timer.nsecsElapsed();
        if (onPaint) onPaint(elapsed, this->lastFrameRenderMetrics());
    }
private:
    bool m_profiling = false;
    bool m_ready = false;
};

struct ProcessSample {
    qint64 cpuNs = -1;
    quint64 workingSet = 0;
};

ProcessSample processSample() {
    ProcessSample sample;
#ifdef Q_OS_WIN
    FILETIME created{}, exited{}, kernel{}, user{};
    if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) {
        const quint64 kernelTicks = (static_cast<quint64>(kernel.dwHighDateTime) << 32) | kernel.dwLowDateTime;
        const quint64 userTicks = (static_cast<quint64>(user.dwHighDateTime) << 32) | user.dwLowDateTime;
        sample.cpuNs = static_cast<qint64>((kernelTicks + userTicks) * 100);
    }
    PROCESS_MEMORY_COUNTERS memory{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory))) {
        sample.workingSet = static_cast<quint64>(memory.WorkingSetSize);
    }
#endif
    return sample;
}

QString number(double value) { return QString::number(value, 'f', 3); }
QString csvStat(const DurationStats& stats, double value) { return stats.count ? number(value) : QString(); }
QString percentile(const DurationStats& stats, double p) {
    return stats.count ? number(stats.percentile(p)) : QStringLiteral("—");
}
QString csvRow(const QStringList& values) {
    QStringList escaped;
    for (QString value : values) {
        value.replace('"', "\"\"");
        escaped.push_back('"' + value + '"');
    }
    return escaped.join(',') + "\r\n";
}
} // namespace

struct StressTestDialog::Impl {
    enum class State { Idle, Warming, Measuring };
    struct ChartEntry {
        QString name;
        ProGraphics::BaseGLWidget* widget = nullptr;
        ProGraphics::PRPDChart* prpd = nullptr;
        ProGraphics::PRPSChart* prps = nullptr;
        PaintProbe* probe = nullptr;
        int channel = 0;
        DurationStats updates;
        DurationStats paints;
        DurationStats frameGaps;
        std::array<DurationStats, 4> stages;
        quint64 cachedTextFrames = 0, textCacheRebuilds = 0;
        qint64 lastPaintNs = -1;
        QSize measuredSize;
        double dpr = 1;
        bool sizeChanged = false;
    };

    StressTestDialog* owner;
    QWidget* configuration = nullptr;
    QWidget* chartHost = nullptr;
    QGridLayout* chartGrid = nullptr;
    QComboBox *preset, *kind, *shape, *range;
    QSpinBox *channels, *phases, *rate, *density, *warmup, *duration, *lines, *animation, *seed, *budget;
    QCheckBox *references, *hidden;
    QCheckBox *geometryLayer, *textLayer, *dataLayer, *textCache;
    QPushButton *startButton, *stopButton, *exportButton;
    QLabel *status, *summary, *resources, *device;
    QTableWidget* table;
    QTimer feedTimer, heartbeat, refreshTimer;
    QElapsedTimer clock;
    State state = State::Idle;
    std::vector<ChartEntry> charts;
    std::vector<std::vector<std::vector<float>>> bank;
    DurationStats eventLoopLag;
    qint64 phaseStartNs = 0, measurementStartNs = -1, finishNs = 0, lastHeartbeatNs = 0;
    quint64 delivered = 0, warmupDelivered = 0, maxBacklog = 0, sourceCursor = 0;
    quint64 finalExpected = 0;
    QString finalStatus, startedUtc, glVendor, glRenderer, glVersion;
    bool hiddenAtStart = false, hiddenAtFinish = false, visibilityChanged = false, autoClose = false;
    int runRate = 50, runWarmup = 10, runDuration = 60, runBudget = 4;
    QStringList runConfigCsv;
    ProGraphics::BaseGLWidget::RenderOptions runRenderOptions;
    quint64 bankPayloadBytes = 0;
    QString autoCsv;
    QSize previousMinimum, previousMaximum;
    ProcessSample initialProcess, previousProcess, currentProcess;
    qint64 previousProcessNs = 0;
    quint64 peakWorkingSet = 0;
    double peakCpuCores = 0, currentCpuCores = -1;

    explicit Impl(StressTestDialog* dialog) : owner(dialog) {
        buildUi();
        feedTimer.setInterval(5);
        feedTimer.setTimerType(Qt::PreciseTimer);
        heartbeat.setInterval(20);
        heartbeat.setTimerType(Qt::PreciseTimer);
        refreshTimer.setInterval(500);
        QObject::connect(&feedTimer, &QTimer::timeout, owner, [this] { pump(); });
        QObject::connect(&heartbeat, &QTimer::timeout, owner, [this] {
            if (state != State::Measuring) return;
            const qint64 now = clock.nsecsElapsed();
            eventLoopLag.add(std::max<qint64>(0, now - lastHeartbeatNs - 20000000));
            lastHeartbeatNs = now;
        });
        QObject::connect(&refreshTimer, &QTimer::timeout, owner, [this] {
            sampleResources();
            refresh();
        });
    }

    ~Impl() {
        feedTimer.stop();
        heartbeat.stop();
        refreshTimer.stop();
        for (auto& chart : charts) chart.probe->setProfiling(false);
    }

    QSpinBox* spin(int minimum, int maximum, int value) {
        auto* result = new QSpinBox(configuration);
        result->setRange(minimum, maximum);
        result->setValue(value);
        return result;
    }

    void buildUi() {
        owner->setWindowTitle("性能压力测试");
        auto* root = new QVBoxLayout(owner);
        auto* hint = new QLabel("预热后计时；固定种子数据在计时前生成。目标速率追不上时显示缺口，不伪报吞吐量。", owner);
        hint->setWordWrap(true);
        root->addWidget(hint);

        configuration = new QGroupBox("负载配置", owner);
        auto* config = new QGridLayout(configuration);
        preset = new QComboBox(configuration);
        preset->addItems({"日常负载", "稠密高压", "稳态复用", "量程变动"});
        kind = new QComboBox(configuration);
        kind->addItems({"PRPD + PRPS", "仅 PRPD", "仅 PRPS"});
        shape = new QComboBox(configuration);
        shape->addItems({"变化数据", "重复同一周期", "高低幅值交替"});
        range = new QComboBox(configuration);
        range->addItems({"固定量程", "自适应量程"});
        channels = spin(1, 16, 4);
        phases = spin(1, 4096, 200);
        rate = spin(1, 10000, 50);
        density = spin(0, 100, 5);
        warmup = spin(1, 60, 10);
        duration = spin(1, 3600, 60);
        lines = spin(0, 4096, 50);
        lines->setSpecialValueText("全部采样点");
        animation = spin(5, 100, 20);
        seed = spin(0, std::numeric_limits<int>::max(), 20261023);
        budget = spin(1, 20, 4);
        references = new QCheckBox("绘制参考线", configuration);
        references->setChecked(true);
        const auto field = [&](int row, int column, const QString& text, QWidget* input) {
            config->addWidget(new QLabel(text, configuration), row, column * 2);
            config->addWidget(input, row, column * 2 + 1);
        };
        field(0, 0, "预设", preset);
        field(0, 1, "图表", kind);
        field(0, 2, "通道数", channels);
        field(0, 3, "相位点数", phases);
        field(0, 4, "周期/秒/通道", rate);
        field(1, 0, "数据类型", shape);
        field(1, 1, "量程", range);
        field(1, 2, "非底值概率 %", density);
        field(1, 3, "预热秒数", warmup);
        field(1, 4, "计时秒数", duration);
        field(2, 0, "PRPS 线数", lines);
        field(2, 1, "动画间隔 ms", animation);
        field(2, 2, "随机种子", seed);
        field(2, 3, "每次送入预算 ms", budget);
        config->addWidget(references, 2, 8, 1, 2);
        geometryLayer = new QCheckBox("绘制坐标网格", configuration);
        textLayer = new QCheckBox("绘制文字", configuration);
        dataLayer = new QCheckBox("绘制数据", configuration);
        textCache = new QCheckBox("启用文字缓存", configuration);
        for (auto* option : {geometryLayer, textLayer, dataLayer, textCache}) option->setChecked(true);
        config->addWidget(geometryLayer, 3, 0, 1, 2);
        config->addWidget(textLayer, 3, 2, 1, 2);
        config->addWidget(dataLayer, 3, 4, 1, 2);
        config->addWidget(textCache, 3, 6, 1, 2);
        config->addWidget(new QLabel("关闭图层用于定位瓶颈", configuration), 3, 8, 1, 2);
        textCache->setToolTip("取消后使用原有文字绘制路径；保持其他配置相同，比较文字耗时、FPS 和界面延迟。");
        dataLayer->setToolTip("只关闭数据绘制，继续送入数据和更新统计；PRPD 延后到绘制时的统计重建仍执行。");
        rate->setToolTip("每通道每秒的周期数；组合模式下，同一个周期分别送入一张 PRPD 和一张 PRPS。");
        density->setToolTip("每个相位生成高于 -75 底值的概率；PRPD 仍统计底值，不能视为脉冲事件密度。");
        budget->setToolTip("GUI 线程每 5ms 尝试送入，单次最多 128 个周期。预算是软上限，一组通道更新不会中途打断。");
        root->addWidget(configuration);

        auto* actions = new QHBoxLayout;
        startButton = new QPushButton("开始测试", owner);
        stopButton = new QPushButton("停止", owner);
        exportButton = new QPushButton("导出 CSV", owner);
        hidden = new QCheckBox("隐藏绘图区，继续送入数据", owner);
        stopButton->setEnabled(false);
        exportButton->setEnabled(false);
        actions->addWidget(startButton);
        actions->addWidget(stopButton);
        actions->addWidget(exportButton);
        actions->addWidget(hidden);
        actions->addStretch();
        root->addLayout(actions);
        status = new QLabel("就绪：建议先跑日常负载，再逐步增加通道、速率和数据密度。", owner);
        summary = new QLabel(owner);
        resources = new QLabel(owner);
        device = new QLabel(owner);
        for (auto* label : {status, summary, resources, device}) {
            label->setWordWrap(true);
            root->addWidget(label);
        }
        chartHost = new QWidget(owner);
        chartGrid = new QGridLayout(chartHost);
        chartGrid->setContentsMargins(0, 0, 0, 0);
        chartGrid->setSpacing(4);
        root->addWidget(chartHost, 1);
        table = new QTableWidget(0, 13, owner);
        table->setHorizontalHeaderLabels({"图表", "更新/秒", "更新 P95 ms", "更新 P99 ms", "绘制 FPS",
                                         "绘制 P95 ms", "绘制 P99 ms", "帧间隔 P99 ms", "绘制最大 ms",
                                         "坐标平均 ms", "文字平均 ms", "数据平均 ms", "参考线平均 ms"});
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->verticalHeader()->hide();
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setMaximumHeight(180);
        root->addWidget(table);
        auto* note = new QLabel(
            "绘制 FPS 是 paintGL 调用率；绘制耗时为 CPU 提交时间，不含 GPU 完成等待。"
            "P95/P99 为有界直方图估计。稳态 PRPD 少重绘是正常现象；请用系统监视器另记 GPU 占用。", owner);
        note->setWordWrap(true);
        root->addWidget(note);
        QObject::connect(preset, QOverload<int>::of(&QComboBox::currentIndexChanged), owner,
                         [this](int index) { applyPreset(index); });
        QObject::connect(startButton, &QPushButton::clicked, owner, [this] { start(); });
        QObject::connect(stopButton, &QPushButton::clicked, owner, [this] { finish("手动停止"); });
        QObject::connect(exportButton, &QPushButton::clicked, owner, [this] {
            const QString filename = QFileDialog::getSaveFileName(owner, "导出压力测试报告",
                "prographics-stress-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".csv",
                "CSV 文件 (*.csv)");
            if (!filename.isEmpty() && !exportCsv(filename)) QMessageBox::warning(owner, "导出失败", "无法保存报告，请检查路径和文件占用。");
        });
        QObject::connect(hidden, &QCheckBox::toggled, owner, [this](bool checked) {
            chartHost->setVisible(!checked);
            if (state == State::Measuring) visibilityChanged = true;
        });
        const QRect available = owner->screen()->availableGeometry();
        owner->resize(std::min(1400, available.width() - 60), std::min(1000, available.height() - 60));
    }

    void applyPreset(int index) {
        kind->setCurrentIndex(0);
        channels->setValue(index == 1 ? 8 : 4);
        phases->setValue(index == 1 || index == 3 ? 512 : 200);
        rate->setValue(index == 0 ? 50 : index == 3 ? 100 : 200);
        density->setValue(index == 0 ? 5 : 100);
        shape->setCurrentIndex(index == 2 ? 1 : index == 3 ? 2 : 0);
        range->setCurrentIndex(index == 3 ? 1 : 0);
        lines->setValue(index == 1 ? 0 : 50);
        animation->setValue(20);
        warmup->setValue(10);
        duration->setValue(60);
    }

    void prepareBank() {
        bank.clear();
        bankPayloadBytes = 0;
        bank.resize(static_cast<size_t>(channels->value()));
        for (size_t channel = 0; channel < bank.size(); ++channel) {
            std::uint32_t random = static_cast<std::uint32_t>(seed->value()) ^
                (0x9e3779b9u * (static_cast<std::uint32_t>(channel) + 1));
            if (!random) random = 0x6d2b79f5u;
            const auto next = [&]() {
                random ^= random << 13;
                random ^= random >> 17;
                random ^= random << 5;
                return random;
            };
            auto& cycles = bank[channel];
            cycles.resize(shape->currentIndex() == 1 ? 1 : BankSize);
            for (size_t frame = 0; frame < cycles.size(); ++frame) {
                auto& values = cycles[frame];
                values.resize(static_cast<size_t>(phases->value()));
                bankPayloadBytes += values.size() * sizeof(float);
                for (float& value : values) {
                    if (next() % 100 >= static_cast<unsigned>(density->value())) {
                        value = -75.0f;
                        continue;
                    }
                    const float unit = static_cast<float>(next() >> 8) / 16777216.0f;
                    const bool shock = shape->currentIndex() == 2;
                    const float low = shock ? ((frame / 32) % 2 == 0 ? -74.0f : -43.0f) : -74.0f;
                    const float span = shock ? 9.0f : 43.0f;
                    value = low + unit * span;
                }
            }
        }
    }

    template<class Chart>
    void addChart(int channel, int columns) {
        const size_t index = charts.size();
        charts.emplace_back();
        auto& entry = charts.back();
        entry.channel = channel;
        auto* card = new QWidget(chartHost);
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(2, 2, 2, 2);
        layout->setSpacing(1);
        auto* chart = new MeasuredChart<Chart>(card);
        entry.widget = chart;
        entry.probe = chart;
        if constexpr (std::is_same_v<Chart, ProGraphics::PRPDChart>) {
            entry.prpd = chart;
            entry.name = QString("通道 %1 PRPD").arg(channel + 1);
            if (references->isChecked()) chart->setAmplitudeLines({{-50.0f}, {-35.0f}});
        } else {
            entry.prps = chart;
            entry.name = QString("通道 %1 PRPS").arg(channel + 1);
            chart->setDisplayLineCount(lines->value());
            chart->setUpdateInterval(animation->value());
            if (references->isChecked()) chart->setPhaseAmplitudePlaneLines({{-50.0f}, {-35.0f}});
        }
        chart->setPhasePoint(phases->value());
        if (range->currentIndex() == 0) chart->setFixedRange(-75.0f, -30.0f);
        else chart->setAdaptiveRange(-75.0f, -30.0f);
        chart->setMinimumSize(65, 55);
        chart->setRenderOptions(runRenderOptions);
        chart->onPaint = [this, index](qint64 ns, const ProGraphics::BaseGLWidget::FrameRenderMetrics& metrics) {
            if (state != State::Measuring) return;
            auto& measured = charts[index];
            const qint64 now = clock.nsecsElapsed() - ns;
            measured.paints.add(ns);
            for (size_t stage = 0; stage < measured.stages.size(); ++stage) {
                if (metrics.measured[stage]) measured.stages[stage].add(metrics.cpuNs[stage]);
            }
            if (metrics.textCacheUsed) ++measured.cachedTextFrames;
            if (metrics.textCacheRebuilt) ++measured.textCacheRebuilds;
            if (measured.lastPaintNs >= 0) measured.frameGaps.add(now - measured.lastPaintNs);
            measured.lastPaintNs = now;
            measured.sizeChanged = measured.sizeChanged || measured.measuredSize != measured.widget->size()
                || measured.dpr != measured.widget->devicePixelRatioF();
        };
        layout->addWidget(new QLabel(entry.name, card));
        layout->addWidget(chart, 1);
        chartGrid->addWidget(card, static_cast<int>(index) / columns, static_cast<int>(index) % columns);
    }

    void prepareCharts() {
        charts.clear();
        while (auto* item = chartGrid->takeAt(0)) {
            delete item->widget();
            delete item;
        }
        const int count = channels->value() * (kind->currentIndex() == 0 ? 2 : 1);
        const int columns = count <= 2 ? count : count <= 4 ? 2 : count <= 16 ? 4 : 8;
        charts.reserve(static_cast<size_t>(count));
        for (int channel = 0; channel < channels->value(); ++channel) {
            if (kind->currentIndex() != 2) addChart<ProGraphics::PRPDChart>(channel, columns);
            if (kind->currentIndex() != 1) addChart<ProGraphics::PRPSChart>(channel, columns);
        }
        table->setRowCount(count);
        for (int row = 0; row < count; ++row) {
            for (int column = 0; column < table->columnCount(); ++column) {
                if (!table->item(row, column)) table->setItem(row, column, new QTableWidgetItem);
            }
        }
        chartHost->setVisible(!hidden->isChecked());
    }

    void start() {
        if (state != State::Idle) return;
        runRate = rate->value();
        runWarmup = warmup->value();
        runDuration = duration->value();
        runBudget = budget->value();
        runRenderOptions.geometry = geometryLayer->isChecked();
        runRenderOptions.text = textLayer->isChecked();
        runRenderOptions.data = dataLayer->isChecked();
        runRenderOptions.referenceLines = references->isChecked();
        runRenderOptions.cachedText = textCache->isChecked();
        // 导出始终使用本轮快照，停止后修改下一轮配置不会改写已有结果。
        runConfigCsv = {QString::number(channels->value()), kind->currentText(), QString::number(phases->value()),
            QString::number(density->value()), shape->currentText(), range->currentText(), QString::number(seed->value()),
            QString::number(runRate), QString::number(runWarmup), QString::number(runDuration),
            QString::number(lines->value()), QString::number(animation->value()), QString::number(runBudget),
            references->isChecked() ? "是" : "否"};
        configuration->hide();
        prepareBank();
        prepareCharts();
        measurementStartNs = -1;
        finishNs = 0;
        delivered = warmupDelivered = maxBacklog = finalExpected = sourceCursor = 0;
        eventLoopLag.clear();
        visibilityChanged = false;
        finalStatus.clear();
        glVendor.clear(); glRenderer.clear(); glVersion.clear();
        currentCpuCores = -1;
        initialProcess = previousProcess = currentProcess = ProcessSample{};
        peakCpuCores = 0;
        peakWorkingSet = 0;
        configuration->setEnabled(false);
        startButton->setEnabled(false);
        stopButton->setEnabled(true);
        exportButton->setEnabled(false);
        previousMinimum = owner->minimumSize();
        previousMaximum = owner->maximumSize();
        owner->setFixedSize(owner->size());
        state = State::Warming;
        clock.start();
        phaseStartNs = 0;
        feedTimer.start();
        refreshTimer.start();
        refresh();
    }

    void beginMeasurement() {
        if (!hidden->isChecked()) {
            for (const auto& chart : charts) {
                if (!chart.widget->isValid() || !chart.probe->graphicsReady()) {
                    finish("失败：图表需要有效的 OpenGL 4.1 上下文");
                    return;
                }
            }
        }
        auto* widget = charts.front().widget;
        if (widget->isValid()) {
            widget->makeCurrent();
            const auto text = [&](GLenum name) {
                const auto* value = widget->context()->functions()->glGetString(name);
                return value ? QString::fromLatin1(reinterpret_cast<const char*>(value)) : QStringLiteral("不可用");
            };
            glVendor = text(GL_VENDOR);
            glRenderer = text(GL_RENDERER);
            glVersion = text(GL_VERSION);
            widget->doneCurrent();
        } else glRenderer = "未初始化（隐藏模式）";
        device->setText("渲染设备：" + glRenderer + "；OpenGL：" + glVersion);
        owner->layout()->activate();
        chartGrid->activate();
        for (auto& chart : charts) chart.widget->parentWidget()->layout()->activate();
        warmupDelivered = delivered;
        delivered = maxBacklog = 0;
        hiddenAtStart = hidden->isChecked();
        startedUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        for (auto& chart : charts) {
            chart.updates.clear(); chart.paints.clear(); chart.frameGaps.clear();
            for (auto& stage : chart.stages) stage.clear();
            chart.cachedTextFrames = chart.textCacheRebuilds = 0;
            chart.lastPaintNs = -1;
            chart.measuredSize = chart.widget->size();
            chart.dpr = chart.widget->devicePixelRatioF();
            chart.sizeChanged = false;
            chart.probe->setProfiling(true);
        }
        initialProcess = previousProcess = currentProcess = processSample();
        peakWorkingSet = initialProcess.workingSet;
        state = State::Measuring;
        measurementStartNs = phaseStartNs = clock.nsecsElapsed();
        previousProcessNs = lastHeartbeatNs = measurementStartNs;
        heartbeat.start();
        refresh();
    }

    quint64 expected(qint64 now) const {
        const qint64 limit = static_cast<qint64>(state == State::Warming ? runWarmup : runDuration) * Second;
        const qint64 elapsed = std::clamp<qint64>(now - phaseStartNs, 0, limit);
        return static_cast<quint64>(elapsed * runRate / Second);
    }

    void pump() {
        if (state == State::Idle) return;
        qint64 now = clock.nsecsElapsed();
        const qint64 limit = static_cast<qint64>(state == State::Warming ? runWarmup : runDuration) * Second;
        const bool ending = now - phaseStartNs >= limit;
        const quint64 due = expected(now);
        if (state == State::Measuring) maxBacklog = std::max(maxBacklog, due > delivered ? due - delivered : 0);
        QElapsedTimer slice;
        slice.start();
        int cycles = 0;
        while (delivered < due && cycles < 128) {
            for (auto& chart : charts) {
                const auto& source = bank[static_cast<size_t>(chart.channel)];
                const auto& data = source[static_cast<size_t>(sourceCursor % source.size())];
                if (state == State::Measuring) {
                    QElapsedTimer timer;
                    timer.start();
                    if (chart.prpd) chart.prpd->addCycleData(data);
                    else chart.prps->addCycleData(data);
                    chart.updates.add(timer.nsecsElapsed());
                } else {
                    if (chart.prpd) chart.prpd->addCycleData(data);
                    else chart.prps->addCycleData(data);
                }
            }
            ++delivered;
            ++sourceCursor;
            ++cycles;
            now = clock.nsecsElapsed();
            if ((!ending && now - phaseStartNs >= limit) || slice.nsecsElapsed() >= static_cast<qint64>(runBudget) * 1000000) break;
        }
        // 到时后只允许一次有界送入片，补齐正常定时器的末次请求；欠账不无限追赶。
        if (ending) {
            if (state == State::Warming) beginMeasurement();
            else finish("完成");
        }
    }

    double elapsedSeconds() const {
        if (measurementStartNs < 0) return 0;
        const qint64 end = state == State::Measuring ? clock.nsecsElapsed() : finishNs;
        return static_cast<double>(std::max<qint64>(0, end - measurementStartNs)) / Second;
    }

    void sampleResources() {
        if (measurementStartNs < 0) return;
        const qint64 now = state == State::Measuring ? clock.nsecsElapsed() : finishNs;
        currentProcess = processSample();
        peakWorkingSet = std::max(peakWorkingSet, currentProcess.workingSet);
        if (previousProcess.cpuNs >= 0 && currentProcess.cpuNs >= previousProcess.cpuNs && now > previousProcessNs) {
            currentCpuCores = static_cast<double>(currentProcess.cpuNs - previousProcess.cpuNs) / (now - previousProcessNs);
            peakCpuCores = std::max(peakCpuCores, currentCpuCores);
        }
        previousProcess = currentProcess;
        previousProcessNs = now;
    }

    void finish(const QString& reason) {
        if (state == State::Idle) return;
        finishNs = clock.nsecsElapsed();
        if (measurementStartNs >= 0 && finishNs - lastHeartbeatNs > 20000000) {
            // 收尾时补记尚未来得及触发的心跳超时，避免漏掉最后一次长阻塞。
            eventLoopLag.add(finishNs - lastHeartbeatNs - 20000000);
        }
        finalExpected = expected(finishNs);
        maxBacklog = std::max(maxBacklog, finalExpected > delivered ? finalExpected - delivered : 0);
        hiddenAtFinish = hidden->isChecked();
        finalStatus = measurementStartNs < 0 ? reason + "（未进入计时）" : reason;
        sampleResources();
        state = State::Idle;
        feedTimer.stop(); heartbeat.stop(); refreshTimer.stop();
        for (auto& chart : charts) {
            chart.probe->setProfiling(false);
            if (chart.prpd) chart.prpd->pause(true);
            else chart.prps->pause(true);
        }
        configuration->setEnabled(true);
        configuration->show();
        startButton->setEnabled(true);
        stopButton->setEnabled(false);
        exportButton->setEnabled(measurementStartNs >= 0);
        owner->setMaximumSize(previousMaximum);
        owner->setMinimumSize(previousMinimum);
        refresh();
        if (autoClose) {
            const bool ok = measurementStartNs >= 0 && exportCsv(autoCsv);
            qInfo().noquote() << (ok ? "压力测试报告已保存：" + autoCsv : "压力测试未生成有效报告");
            const int exitCode = ok && reason == "完成" ? 0 : 2;
            QTimer::singleShot(0, owner, [exitCode] { QApplication::exit(exitCode); });
        }
    }

    void refresh() {
        const double seconds = elapsedSeconds();
        if (state == State::Warming) {
            status->setText(QString("预热中：%1 / %2 秒；已送入 %3 周期/通道，不计入结果。")
                .arg(clock.elapsed() / 1000.0, 0, 'f', 1).arg(runWarmup).arg(delivered));
        } else if (state == State::Measuring) {
            status->setText(QString("计时中：%1 / %2 秒；窗口尺寸已固定，可随时停止。")
                .arg(seconds, 0, 'f', 1).arg(runDuration));
        } else status->setText(finalStatus.isEmpty() ? "就绪" : finalStatus);
        const quint64 wanted = state == State::Measuring ? expected(clock.nsecsElapsed()) : finalExpected;
        const quint64 backlog = wanted > delivered ? wanted - delivered : 0;
        const double achieved = seconds > 0 ? static_cast<double>(delivered) / seconds : 0;
        summary->setText(QString("目标 %1 周期/秒/通道；实际 %2；已送入 %3；缺口 %4；最大积压 %5。界面延迟 P99 %6 ms，最大 %7 ms。")
            .arg(runRate).arg(achieved, 0, 'f', 1).arg(delivered).arg(backlog).arg(maxBacklog)
            .arg(percentile(eventLoopLag, .99)).arg(eventLoopLag.maxMs(), 0, 'f', 3));
        if (currentProcess.cpuNs >= 0) {
            resources->setText(QString("进程 CPU %1 核（整机约 %2%）；工作集 %3 MiB；计时阶段采样峰值 %4 MiB。")
                .arg(currentCpuCores < 0 ? "待采样" : QString::number(currentCpuCores, 'f', 2))
                .arg(currentCpuCores < 0 ? "待采样" : QString::number(currentCpuCores * 100 / std::max(1, QThread::idealThreadCount()), 'f', 1))
                .arg(currentProcess.workingSet / 1048576.0, 0, 'f', 1).arg(peakWorkingSet / 1048576.0, 0, 'f', 1));
        } else resources->setText("进程 CPU / 工作集：此平台未采集；请使用系统监视器记录 CPU、内存和 GPU。");
        device->setText(glRenderer.isEmpty() ? "设备信息将在预热结束后读取。" : "渲染设备：" + glRenderer + "；OpenGL：" + glVersion);
        for (size_t i = 0; i < charts.size(); ++i) {
            const auto& chart = charts[i];
            QStringList cells{chart.name, number(seconds > 0 ? chart.updates.count / seconds : 0),
                percentile(chart.updates, .95), percentile(chart.updates, .99),
                number(seconds > 0 ? chart.paints.count / seconds : 0), percentile(chart.paints, .95),
                percentile(chart.paints, .99), percentile(chart.frameGaps, .99), number(chart.paints.maxMs())};
            for (const auto& stage : chart.stages) cells.append(stage.count ? number(stage.meanMs()) : QStringLiteral("—"));
            for (int column = 0; column < cells.size(); ++column) table->item(static_cast<int>(i), column)->setText(cells[column]);
        }
    }

    bool exportCsv(const QString& filename) const {
        if (measurementStartNs < 0) return false;
        QSaveFile file(filename);
        if (!file.open(QIODevice::WriteOnly)) return false;
        const double seconds = elapsedSeconds();
        const double cpuCores = initialProcess.cpuNs >= 0 && currentProcess.cpuNs >= initialProcess.cpuNs && seconds > 0
            ? (currentProcess.cpuNs - initialProcess.cpuNs) / (seconds * Second) : -1;
        QString compiler;
#if defined(_MSC_VER)
        compiler = QString("MSVC %1").arg(_MSC_VER);
#elif defined(__clang__)
        compiler = "Clang " __clang_version__;
#elif defined(__GNUC__)
        compiler = "GCC " __VERSION__;
#endif
        QStringList common{startedUtc, finalStatus, PROGRAPHICS_EXAMPLE_BUILD_TYPE, qVersion(), compiler,
            QSysInfo::prettyProductName(), glVendor, glRenderer, glVersion};
        common.append(runConfigCsv);
        common.append({number(seconds), hiddenAtStart ? "是" : "否", hiddenAtFinish ? "是" : "否",
            visibilityChanged ? "是" : "否", QString::number(warmupDelivered), QString::number(finalExpected),
            QString::number(delivered), QString::number(finalExpected > delivered ? finalExpected - delivered : 0),
            QString::number(maxBacklog), QString::number(eventLoopLag.count),
            csvStat(eventLoopLag, eventLoopLag.percentile(.95)), csvStat(eventLoopLag, eventLoopLag.percentile(.99)),
            csvStat(eventLoopLag, eventLoopLag.maxMs()), cpuCores >= 0 ? number(cpuCores) : QString(),
            cpuCores >= 0 ? number(peakCpuCores) : QString(),
            initialProcess.workingSet ? number(initialProcess.workingSet / 1048576.0) : QString(),
            currentProcess.workingSet ? number(currentProcess.workingSet / 1048576.0) : QString(),
            peakWorkingSet ? number(peakWorkingSet / 1048576.0) : QString(), number(bankPayloadBytes / 1048576.0),
            runRenderOptions.geometry ? "是" : "否", runRenderOptions.text ? "是" : "否",
            runRenderOptions.data ? "是" : "否", runRenderOptions.cachedText ? "是" : "否"});
        QStringList header{"计时开始UTC", "结束状态", "构建类型", "Qt版本", "编译器", "系统", "GPU厂商", "渲染设备", "OpenGL版本",
            "通道数", "图表组合", "相位点数", "非底值概率百分比", "数据类型", "量程模式", "种子", "目标周期每秒每通道",
            "预热秒数", "设定计时秒数", "PRPS线数设置", "动画间隔ms", "送入预算ms", "参考线", "实际计时秒数",
            "开始隐藏", "结束隐藏", "计时中切换隐藏", "预热送入周期每通道", "目标周期每通道", "实际送入周期每通道",
            "最终缺口每通道", "最大积压每通道", "界面延迟样本数", "界面延迟P95ms估计", "界面延迟P99ms估计", "界面延迟最大ms",
            "进程平均CPU核数", "进程采样峰值CPU核数", "开始工作集MiB", "结束工作集MiB", "采样峰值工作集MiB", "样本库幅值数据MiB",
            "绘制坐标网格", "绘制文字", "绘制数据", "启用文字缓存",
            "图表", "逻辑宽", "逻辑高", "DPR", "更新次数", "实际更新每秒", "更新平均ms", "更新P95ms估计", "更新P99ms估计", "更新最大ms",
            "绘制次数", "绘制FPS", "绘制CPU提交平均ms", "绘制P95ms估计", "绘制P99ms估计", "绘制最大ms", "帧间隔样本数", "帧间隔P99ms估计", "帧间隔最大ms", "计时中图表尺寸变化",
            "文字缓存绘制帧数", "文字缓存重建帧数"};
        const QStringList stageNames{"坐标网格", "文字", "数据绘制", "参考线绘制"};
        for (const auto& stage : stageNames) {
            header.append({stage + "样本数", stage + "平均ms", stage + "P95ms估计", stage + "P99ms估计", stage + "最大ms"});
        }
        QByteArray csv = "\xEF\xBB\xBF";
        csv += csvRow(header).toUtf8();
        for (const auto& chart : charts) {
            QStringList row = common;
            row.append({chart.name, QString::number(chart.measuredSize.width()), QString::number(chart.measuredSize.height()), number(chart.dpr),
                QString::number(chart.updates.count), number(seconds > 0 ? chart.updates.count / seconds : 0), csvStat(chart.updates, chart.updates.meanMs()),
                csvStat(chart.updates, chart.updates.percentile(.95)), csvStat(chart.updates, chart.updates.percentile(.99)), csvStat(chart.updates, chart.updates.maxMs()),
                QString::number(chart.paints.count), number(seconds > 0 ? chart.paints.count / seconds : 0), csvStat(chart.paints, chart.paints.meanMs()),
                csvStat(chart.paints, chart.paints.percentile(.95)), csvStat(chart.paints, chart.paints.percentile(.99)), csvStat(chart.paints, chart.paints.maxMs()),
                QString::number(chart.frameGaps.count), csvStat(chart.frameGaps, chart.frameGaps.percentile(.99)), csvStat(chart.frameGaps, chart.frameGaps.maxMs()),
                chart.sizeChanged ? "是" : "否", QString::number(chart.cachedTextFrames), QString::number(chart.textCacheRebuilds)});
            for (const auto& stage : chart.stages) {
                row.append({QString::number(stage.count), csvStat(stage, stage.meanMs()), csvStat(stage, stage.percentile(.95)),
                    csvStat(stage, stage.percentile(.99)), csvStat(stage, stage.maxMs())});
            }
            csv += csvRow(row).toUtf8();
        }
        return file.write(csv) == csv.size() && file.commit();
    }
};

StressTestDialog::StressTestDialog(QWidget* parent) : QDialog(parent), m_impl(std::make_unique<Impl>(this)) {}
StressTestDialog::~StressTestDialog() = default;

void StressTestDialog::reject() {
    if (m_impl->autoClose && m_impl->state != Impl::State::Idle) {
        m_impl->finish("取消");
        return;
    }
    QDialog::reject();
}

void StressTestDialog::startAutomatically(int warmupSeconds, int durationSeconds, const QString& csvPath, int presetIndex) {
    m_impl->preset->setCurrentIndex(presetIndex);
    m_impl->warmup->setValue(warmupSeconds);
    m_impl->duration->setValue(durationSeconds);
    m_impl->autoClose = true;
    m_impl->autoCsv = csvPath;
    m_impl->start();
}
