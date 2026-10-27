#pragma once

#include "range_window.h"
#include <chrono>

namespace ProGraphics {

// 基础拟合使用 LinearScale；此类只负责模式、硬限制和按实际时间运行的滞回。
class DynamicRange {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Range = LinearScale::Range;

    struct DynamicRangeConfig {
        float bufferRatio = 0.3f;          // 数据上方留白，相对于数据跨度
        float responseSpeed = 0.7f;        // 兼容旧源码；新算法不使用逐周期插值
        bool smartAdjustment = true;       // 兼容旧源码；新算法始终使用时间滞回
        int targetTickCount = 6;           // 近似刻度数，不保证精确等于该数量
        bool enableRangeRecovery = true;   // 允许自动收缩；关闭后只扩展
        int recoveryFrameThreshold = 20;   // 兼容旧源码；请改用 shrinkDelayMs
        float recoveryRangeRatio = 0.8f;   // 候选跨度不大于当前跨度的该比例才允许收缩
        float sameValueRangeRatio = 0.1f;  // 恒定数据围绕其值扩展的比例
        bool enableHardLimits = false;
        float hardLimitMin = -1000.0f;
        float hardLimitMax = 1000.0f;
        int shrinkDelayMs = 3000;          // 收缩条件需要持续满足的实际时间
        float minimumSpan = 1.0f;          // 自动拟合的最小跨度；硬限制优先
        float lowerBufferRatio = 0.0f;     // 数据下方留白
    };

    DynamicRange(float initialMin = 0.0f, float initialMax = 0.0f)
        : DynamicRange(initialMin, initialMax, DynamicRangeConfig{}) {}

    DynamicRange(float initialMin, float initialMax, const DynamicRangeConfig& config) {
        LinearScale::normalize(initialMin, initialMax, m_initial, 0.0);
        m_current = m_initial;
        setConfig(config);
    }

    void setConfig(const DynamicRangeConfig& config) {
        auto next = config;
        next.bufferRatio = validRatio(next.bufferRatio, 0.3f, 10.0f);
        next.lowerBufferRatio = validRatio(next.lowerBufferRatio, 0.0f, 10.0f);
        next.sameValueRangeRatio = validRatio(next.sameValueRangeRatio, 0.1f, 1.0f);
        next.recoveryRangeRatio = std::clamp(validRatio(next.recoveryRangeRatio, 0.8f, 1.0f), 0.01f, 1.0f);
        next.targetTickCount = std::clamp(next.targetTickCount, 2, 1000);
        next.shrinkDelayMs = std::clamp(next.shrinkDelayMs, 0, 3600000);
        if (!std::isfinite(next.minimumSpan) || next.minimumSpan <= 0) next.minimumSpan = 1.0f;
        if (!validLimits(next.hardLimitMin, next.hardLimitMax)) {
            next.hardLimitMin = m_config.hardLimitMin;
            next.hardLimitMax = m_config.hardLimitMax;
        }
        if (next.hardLimitMin > next.hardLimitMax) std::swap(next.hardLimitMin, next.hardLimitMax);
        m_config = next;
        m_pending = false;
        m_candidateValid = false;
        m_current = constrain(m_current);
    }

    DynamicRangeConfig getConfig() const { return m_config; }

    // Auto 只拟合图表保留的数据；Adaptive 将初始区间作为最小显示区间。
    void setUseInitialRange(bool enabled) { m_useInitial = enabled; reset(); }

    bool updateRange(const std::vector<float>& data) {
        Range bounds;
        return RangeWindow::finiteBounds(data, bounds) && updateBounds(bounds.first, bounds.second);
    }

    // 显式时间参数便于确定性回归；图表默认使用单调时钟，频率不影响等待时长。
    bool updateBounds(float min, float max, TimePoint now = Clock::now()) {
        if (!std::isfinite(min) || !std::isfinite(max)) return false;
        if (min > max) std::swap(min, max);
        const Range data{min, max};
        if (!m_candidateValid || data != m_lastData) {
            m_candidate = fitted(data);
            m_lastData = data;
            m_candidateValid = true;
        }
        // 窗口极值不变时复用拟合结果，时间滞回仍正常推进。
        const Range candidate = m_candidate;
        if (!m_hasData) {
            m_hasData = true;
            m_pending = false;
            return commit(candidate);
        }
        if (!LinearScale::contains(m_current, candidate)) {
            // 扩展时只移动越界的一端，不能顺带收缩另一端。
            m_pending = false;
            return commit(constrain(LinearScale::united(m_current, candidate)));
        }
        if (!m_config.enableRangeRecovery || candidate == m_current ||
            LinearScale::span(candidate) > LinearScale::span(m_current) * m_config.recoveryRangeRatio) {
            m_pending = false;
            return false;
        }
        if (!m_pending || now < m_pendingSince) {
            m_pendingSince = now;
            m_pendingRange = candidate;
            m_pending = true;
        } else {
            // 等待期间出现过的数据都要覆盖，不能只拟合最后一个低值周期。
            m_pendingRange = LinearScale::united(m_pendingRange, candidate);
        }
        if (now - m_pendingSince < std::chrono::milliseconds(m_config.shrinkDelayMs)) return false;
        const Range target = m_pendingRange;
        m_pending = false;
        if (LinearScale::span(target) > LinearScale::span(m_current) * m_config.recoveryRangeRatio) return false;
        return commit(target);
    }

    Range getDisplayRange() const { return m_current; }

    void setDisplayRange(float min, float max) {
        Range range;
        if (!LinearScale::normalize(min, max, range, 0.0)) return;
        m_current = constrain(range);
        m_pending = false;
    }

    // 保留旧入口；直接指定量程时同时提交显示范围，避免目标与实际映射脱节。
    void setTargetRange(float min, float max) { setDisplayRange(min, max); }

    void setInitialRange(float min, float max) {
        Range range;
        if (!LinearScale::normalize(min, max, range, 0.0)) return;
        m_initial = range;
        reset();
    }

    Range getInitialRange() const { return m_initial; }

    void setHardLimits(float min, float max, bool enabled = true) {
        if (!validLimits(min, max)) return;
        if (min > max) std::swap(min, max);
        m_config.hardLimitMin = min;
        m_config.hardLimitMax = max;
        enableHardLimits(enabled);
    }

    Range getHardLimits() const { return {m_config.hardLimitMin, m_config.hardLimitMax}; }

    void enableHardLimits(bool enabled) {
        m_config.enableHardLimits = enabled;
        m_current = constrain(m_current);
        m_pending = false;
        m_candidateValid = false;
    }

    bool isHardLimitsEnabled() const { return m_config.enableHardLimits; }

    void reset() {
        m_current = constrain(m_initial);
        m_hasData = false;
        m_pending = false;
        m_candidateValid = false;
    }

    bool isDataExceedingRange(float min, float max) const {
        return std::isfinite(min) && std::isfinite(max) && (min < m_current.first || max > m_current.second);
    }

    Range clampData(float value, bool isMin) const {
        const float clamped = std::isfinite(value) ? std::clamp(value, m_current.first, m_current.second) : m_current.first;
        return isMin ? Range{m_current.first, clamped} : Range{clamped, m_current.second};
    }

private:
    static float validRatio(float value, float fallback, float max) {
        return std::isfinite(value) ? std::clamp(value, 0.0f, max) : fallback;
    }

    static bool validLimits(float min, float max) {
        return std::isfinite(min) && std::isfinite(max) && min != max;
    }

    Range constrain(const Range& range) const {
        return m_config.enableHardLimits ? LinearScale::constrained(range, getHardLimits()) : range;
    }

    Range fitted(Range data) const {
        if (m_config.enableHardLimits) {
            const auto limits = getHardLimits();
            data.first = std::clamp(data.first, limits.first, limits.second);
            data.second = std::clamp(data.second, limits.first, limits.second);
        }
        const Range baseline = constrain(m_initial);
        if (m_useInitial && LinearScale::contains(baseline, data)) return baseline;
        auto candidate = LinearScale::padded(data, m_config.lowerBufferRatio, m_config.bufferRatio,
            m_config.minimumSpan, m_config.sameValueRangeRatio, m_config.targetTickCount);
        if (m_useInitial) candidate = LinearScale::nice(LinearScale::united(candidate, baseline), m_config.targetTickCount);
        return constrain(candidate);
    }

    bool commit(const Range& range) {
        if (range == m_current) return false;
        m_current = range;
        return true;
    }

    DynamicRangeConfig m_config;
    Range m_initial{0.0f, 1.0f};
    Range m_current{0.0f, 1.0f};
    bool m_useInitial = true;
    bool m_hasData = false;
    bool m_pending = false;
    bool m_candidateValid = false;
    Range m_lastData{0.0f, 1.0f};
    Range m_candidate{0.0f, 1.0f};
    Range m_pendingRange{0.0f, 1.0f};
    TimePoint m_pendingSince{};
};

} // namespace ProGraphics
