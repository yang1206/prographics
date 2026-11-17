#pragma once

#include "prographics/prographics_export.h"
#include "prographics/core/graphics/primitive2d.h"
#include <QColor>
#include <QString>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace ProGraphics {

enum class ChartDataMode { Realtime, StaticSnapshot };
enum class CountNormalization { Linear, Log1p };

/** 调用方定义的物理坐标；不做单位或工频换算。 */
struct StaticAxis {
    double minimum = 0.0;
    double maximum = 1.0;
    QString name;
    QString unit;
};

struct CountColorOptions {
    CountNormalization normalization = CountNormalization::Linear;
    std::vector<QColor> colorTable; ///< 空表使用蓝到红的默认颜色，非空表按归一化位置均匀采样
    std::optional<std::uint64_t> maximum; ///< 未指定时使用整份快照最大计数；指定后固定色域并饱和显示超出值
    QString label = QStringLiteral("次数");
};

struct CountColorTick {
    std::uint64_t count = 0;
    double position = 0.0;
};

/** 可独立供色标、提示框和其他视图复用；原始计数始终为 uint64。 */
class PROGRAPHICS_EXPORT CountColorScale {
public:
    CountColorScale() = default;
    std::uint64_t maximum() const { return m_maximum; }
    CountNormalization normalization() const { return m_options.normalization; }
    const QString& label() const { return m_options.label; }
    double normalized(std::uint64_t count) const;
    QColor color(std::uint64_t count) const;
    QColor colorAt(double position) const;
    std::vector<CountColorTick> ticks() const;
private:
    friend class PRPDHistogramSnapshot;
    std::uint64_t m_maximum = 0;
    CountColorOptions m_options;
};

struct PRPDHistogramData {
    std::size_t phaseBins = 0;
    std::size_t amplitudeBins = 0;
    std::vector<double> phaseEdges; ///< phaseBins + 1 个严格递增的边界
    std::vector<double> amplitudeEdges; ///< amplitudeBins + 1 个严格递增的边界
    std::vector<std::uint64_t> counts; ///< phase-major：counts[p * amplitudeBins + a]
    QString phaseName = QStringLiteral("相位");
    QString phaseUnit = QStringLiteral("°");
    QString amplitudeName = QStringLiteral("幅值");
    QString amplitudeUnit = QStringLiteral("dBm");
};

struct HistogramCell {
    std::size_t phaseBin = 0;
    std::size_t amplitudeBin = 0;
    double phaseMinimum = 0.0, phaseMaximum = 0.0;
    double amplitudeMinimum = 0.0, amplitudeMaximum = 0.0;
    std::uint64_t count = 0;
};

/** 可在工作线程准备，无 QWidget、OpenGL 或等待操作。失败返回空指针和中文原因。
 * 所有权转移到不可变快照；保留完整原始网格，零格不生成点，不插值、不重新分箱。
 */
class PROGRAPHICS_EXPORT PRPDHistogramSnapshot {
public:
    using Ptr = std::shared_ptr<const PRPDHistogramSnapshot>;
    PRPDHistogramSnapshot(const PRPDHistogramSnapshot&) = delete;
    PRPDHistogramSnapshot& operator=(const PRPDHistogramSnapshot&) = delete;
    ~PRPDHistogramSnapshot() = default;
    static Ptr prepare(PRPDHistogramData data, CountColorOptions colors = {}, QString* error = nullptr);
    const PRPDHistogramData& data() const { return m_data; }
    const CountColorScale& colorScale() const { return m_colors; }
    bool empty() const { return m_batches.empty(); }
    std::optional<HistogramCell> cell(std::size_t phaseBin, std::size_t amplitudeBin) const;
    std::optional<HistogramCell> cellAt(double phase, double amplitude) const;
private:
    friend class PRPDChart;
    PRPDHistogramSnapshot() = default;
    PRPDHistogramData m_data;
    CountColorScale m_colors;
    std::vector<std::vector<Transform2D>> m_batches;
};

struct PRPSStaticEvent {
    double phase = 0.0;
    double amplitude = 0.0;
    double sequence = 0.0;
    std::uint64_t sourceEventId = 0; ///< 调用方的稳定源 ID；库不生成、不替换
};

struct PRPSEventSequenceData {
    StaticAxis phase{0.0, 360.0, QStringLiteral("相位"), QStringLiteral("°")};
    StaticAxis amplitude{-70.0, 0.0, QStringLiteral("幅值"), QStringLiteral("dBm")};
    StaticAxis sequence{0.0, 1.0, QStringLiteral("事件序号"), {}};
    std::vector<PRPSStaticEvent> events; ///< 保留提交顺序，不按相位合并，不按 sequence 排序
};

struct StaticEventHit {
    std::size_t index = 0;
    PRPSStaticEvent event;
};

/** 所有事件必须处于调用方给定的闭区间内；任一非法事件会拒绝整份快照。 */
class PROGRAPHICS_EXPORT PRPSEventSnapshot {
public:
    using Ptr = std::shared_ptr<const PRPSEventSnapshot>;
    PRPSEventSnapshot(const PRPSEventSnapshot&) = delete;
    PRPSEventSnapshot& operator=(const PRPSEventSnapshot&) = delete;
    ~PRPSEventSnapshot() = default;
    static Ptr prepare(PRPSEventSequenceData data, QString* error = nullptr);
    const PRPSEventSequenceData& data() const { return m_data; }
    bool empty() const { return m_data.events.empty(); }
    std::optional<StaticEventHit> event(std::size_t index) const;
private:
    friend class PRPSChart;
    friend class PRPSRenderer;
    PRPSEventSnapshot() = default;
    struct Instance {
        float phaseHeight[2];
        float color[4];
        float sequence;
    };
    static_assert(sizeof(float) == 4 && sizeof(Instance) == 28);
    PRPSEventSequenceData m_data;
    std::vector<Instance> m_instances;
};

} // namespace ProGraphics
