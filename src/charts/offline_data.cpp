#include "prographics/charts/offline_data.h"
#include "prographics/utils/utils.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>

namespace ProGraphics {
namespace {
bool validAxis(double low, double high) {
    return std::isfinite(low) && std::isfinite(high) && low < high && std::isfinite(high - low);
}
bool validEdges(const std::vector<double>& edges, std::size_t bins) {
    if (bins == 0 || bins == std::numeric_limits<std::size_t>::max() || edges.size() != bins + 1) return false;
    for (std::size_t i = 1; i < edges.size(); ++i) {
        if (!validAxis(edges[i - 1], edges[i])) return false;
    }
    return validAxis(edges.front(), edges.back());
}
bool validPhysicalAxis(double low, double high) {
    // 现有参考线及量程查询使用 float，拒绝不能表达的范围而不悄悄截断。
    const double limit = std::numeric_limits<float>::max();
    return validAxis(low, high) && low >= -limit && high <= limit &&
        static_cast<float>(low) < static_cast<float>(high);
}
void fail(QString* error, const QString& message) { if (error) *error = message; }
float position(double value, double low, double high) {
    return static_cast<float>((value - low) / (high - low));
}
QVector4D rgba(const QColor& c) { return QVector4D(c.redF(), c.greenF(), c.blueF(), c.alphaF()); }
}

double CountColorScale::normalized(std::uint64_t count) const {
    if (m_maximum == 0) return 0.0;
    const double value = static_cast<double>(std::min(count, m_maximum));
    const double maximum = static_cast<double>(m_maximum);
    return m_options.normalization == CountNormalization::Log1p
        ? std::log1p(value) / std::log1p(maximum) : value / maximum;
}

QColor CountColorScale::color(std::uint64_t count) const {
    return count == 0 ? QColor(Qt::transparent) : colorAt(normalized(count));
}

QColor CountColorScale::colorAt(double t) const {
    t = std::isfinite(t) ? std::clamp(t, 0.0, 1.0) : 0.0;
    const auto& table = m_options.colorTable;
    if (table.empty()) {
        return QColor::fromHsvF((240.0 - t * 240.0) / 360.0, 1.0, 0.8 + t * 0.2, 1.0);
    }
    const double index = t * static_cast<double>(table.size() - 1);
    const auto low = static_cast<std::size_t>(index);
    const auto high = std::min(low + 1, table.size() - 1);
    const double f = index - static_cast<double>(low);
    const QColor& a = table[low];
    const QColor& b = table[high];
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * f,
        a.greenF() + (b.greenF() - a.greenF()) * f,
        a.blueF() + (b.blueF() - a.blueF()) * f,
        a.alphaF() + (b.alphaF() - a.alphaF()) * f);
}

std::vector<CountColorTick> CountColorScale::ticks() const {
    std::vector<CountColorTick> result{{0, 0.0}};
    if (m_maximum == 0) return result;
    for (int i = 1; i < 4; ++i) {
        const long double t = static_cast<long double>(i) / 4;
        const long double raw = m_options.normalization == CountNormalization::Log1p
            ? std::expm1(t * std::log1p(static_cast<long double>(m_maximum))) : t * m_maximum;
        // 端点单独保留 uint64 原值，避免 double 对最大整数的舍入越界。
        const auto count = raw >= static_cast<long double>(m_maximum) ? m_maximum
            : static_cast<std::uint64_t>(raw);
        if (count != result.back().count && count < m_maximum) result.push_back({count, normalized(count)});
    }
    result.push_back({m_maximum, 1.0});
    return result;
}

PRPDHistogramSnapshot::Ptr PRPDHistogramSnapshot::prepare(PRPDHistogramData data, CountColorOptions colors, QString* error) try {
    if (error) error->clear();
    if (!validEdges(data.phaseEdges, data.phaseBins) || !validEdges(data.amplitudeEdges, data.amplitudeBins) ||
        data.phaseBins > std::numeric_limits<std::size_t>::max() / data.amplitudeBins ||
        data.counts.size() != data.phaseBins * data.amplitudeBins) {
        fail(error, QStringLiteral("网格维度、严格递增边界或 phase-major 计数长度无效"));
        return {};
    }
    if (!validPhysicalAxis(data.phaseEdges.front(), data.phaseEdges.back()) ||
        !validPhysicalAxis(data.amplitudeEdges.front(), data.amplitudeEdges.back())) {
        fail(error, QStringLiteral("相位或幅值范围无法由现有坐标系统表达"));
        return {};
    }
    if ((colors.normalization != CountNormalization::Linear && colors.normalization != CountNormalization::Log1p) ||
        (colors.maximum && *colors.maximum == 0) ||
        std::any_of(colors.colorTable.begin(), colors.colorTable.end(), [](const QColor& c) { return !c.isValid(); })) {
        fail(error, QStringLiteral("计数归一化、固定色域上限或色表无效"));
        return {};
    }
    auto snapshot = std::shared_ptr<PRPDHistogramSnapshot>(new PRPDHistogramSnapshot);
    snapshot->m_colors.m_maximum = colors.maximum.value_or(*std::max_element(data.counts.begin(), data.counts.end()));
    snapshot->m_colors.m_options = std::move(colors);
    snapshot->m_data = std::move(data);
    const auto& d = snapshot->m_data;
    constexpr std::size_t batchSize = 65536;
    for (std::size_t p = 0; p < d.phaseBins; ++p) {
        const double phase = d.phaseEdges[p] + (d.phaseEdges[p + 1] - d.phaseEdges[p]) * 0.5;
        for (std::size_t a = 0; a < d.amplitudeBins; ++a) {
            const auto count = d.counts[p * d.amplitudeBins + a];
            if (count == 0) continue;
            if (snapshot->m_batches.empty() || snapshot->m_batches.back().size() == batchSize) snapshot->m_batches.emplace_back();
            const double amplitude = d.amplitudeEdges[a] + (d.amplitudeEdges[a + 1] - d.amplitudeEdges[a]) * 0.5;
            Transform2D transform;
            transform.position = QVector2D(PRPDConstants::GL_AXIS_LENGTH * position(phase, d.phaseEdges.front(), d.phaseEdges.back()),
                PRPDConstants::GL_AXIS_LENGTH * position(amplitude, d.amplitudeEdges.front(), d.amplitudeEdges.back()));
            transform.color = rgba(snapshot->m_colors.color(count));
            snapshot->m_batches.back().push_back(transform);
        }
    }
    return snapshot;
} catch (const std::bad_alloc&) {
    fail(error, QStringLiteral("准备分布快照所需内存不足"));
    return {};
} catch (const std::length_error&) {
    fail(error, QStringLiteral("分布快照超过容器可表示的容量"));
    return {};
}

std::optional<HistogramCell> PRPDHistogramSnapshot::cell(std::size_t p, std::size_t a) const {
    if (p >= m_data.phaseBins || a >= m_data.amplitudeBins) return {};
    return HistogramCell{p, a, m_data.phaseEdges[p], m_data.phaseEdges[p + 1],
        m_data.amplitudeEdges[a], m_data.amplitudeEdges[a + 1], m_data.counts[p * m_data.amplitudeBins + a]};
}

std::optional<HistogramCell> PRPDHistogramSnapshot::cellAt(double phase, double amplitude) const {
    if (!std::isfinite(phase) || !std::isfinite(amplitude) || phase < m_data.phaseEdges.front() ||
        phase >= m_data.phaseEdges.back() || amplitude < m_data.amplitudeEdges.front() || amplitude > m_data.amplitudeEdges.back()) return {};
    const auto p = static_cast<std::size_t>(std::upper_bound(m_data.phaseEdges.begin(), m_data.phaseEdges.end(), phase) - m_data.phaseEdges.begin() - 1);
    const auto a = std::min(m_data.amplitudeBins - 1, static_cast<std::size_t>(
        std::upper_bound(m_data.amplitudeEdges.begin(), m_data.amplitudeEdges.end(), amplitude) - m_data.amplitudeEdges.begin() - 1));
    return cell(p, a);
}

PRPSEventSnapshot::Ptr PRPSEventSnapshot::prepare(PRPSEventSequenceData data, QString* error) try {
    if (error) error->clear();
    if (!validPhysicalAxis(data.phase.minimum, data.phase.maximum) ||
        !validPhysicalAxis(data.amplitude.minimum, data.amplitude.maximum) ||
        !validAxis(data.sequence.minimum, data.sequence.maximum) || data.sequence.name.trimmed().isEmpty()) {
        fail(error, QStringLiteral("相位、幅值或序列范围无效；序列轴必须明确命名"));
        return {};
    }
    auto snapshot = std::shared_ptr<PRPSEventSnapshot>(new PRPSEventSnapshot);
    snapshot->m_instances.reserve(data.events.size());
    for (const auto& event : data.events) {
        auto inside = [](double v, const StaticAxis& axis) { return std::isfinite(v) && v >= axis.minimum && v <= axis.maximum; };
        if (!inside(event.phase, data.phase) || !inside(event.amplitude, data.amplitude) || !inside(event.sequence, data.sequence)) {
            fail(error, QStringLiteral("事件含非有限坐标或超出显式范围；整份快照已拒绝"));
            return {};
        }
        const float height = position(event.amplitude, data.amplitude.minimum, data.amplitude.maximum);
        const QVector4D color = calculateColor(height);
        snapshot->m_instances.push_back({{position(event.phase, data.phase.minimum, data.phase.maximum), height},
            {color.x(), color.y(), color.z(), color.w()}, position(event.sequence, data.sequence.minimum, data.sequence.maximum)});
    }
    snapshot->m_data = std::move(data);
    return snapshot;
} catch (const std::bad_alloc&) {
    fail(error, QStringLiteral("准备事件快照所需内存不足"));
    return {};
} catch (const std::length_error&) {
    fail(error, QStringLiteral("事件快照超过容器可表示的容量"));
    return {};
}

std::optional<StaticEventHit> PRPSEventSnapshot::event(std::size_t index) const {
    if (index >= m_data.events.size()) return {};
    return StaticEventHit{index, m_data.events[index]};
}
} // namespace ProGraphics
