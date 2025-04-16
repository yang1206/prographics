//
// Created by Yang1206 on 2025/2/23.
//
#pragma once
#include <QDebug>
#include <QVector4D>
#include <algorithm>
#include <cmath>
#include <deque>
#include <utility>
#include <optional>

namespace ProGraphics {
    // 颜色转换工具
    inline QVector4D hsvToRgb(float h, float s, float v, float a) {
        if (s <= 0.0f) {
            return QVector4D(v, v, v, a);
        }

        h = std::fmod(h, 360.0f);
        if (h < 0.0f)
            h += 360.0f;

        h /= 60.0f;
        int i = static_cast<int>(h);
        float f = h - i;
        float p = v * (1.0f - s);
        float q = v * (1.0f - (s * f));
        float t = v * (1.0f - (s * (1.0f - f)));

        float r, g, b;
        switch (i) {
            case 0:
                r = v;
                g = t;
                b = p;
                break;
            case 1:
                r = q;
                g = v;
                b = p;
                break;
            case 2:
                r = p;
                g = v;
                b = t;
                break;
            case 3:
                r = p;
                g = q;
                b = v;
                break;
            case 4:
                r = t;
                g = p;
                b = v;
                break;
            default:
                r = v;
                g = p;
                b = q;
                break;
        }

        return QVector4D(r, g, b, a);
    }

    inline QVector4D calculateColor(float intensity) {
        // 从蓝色(240°)渐变到红色(0°)
        float hue = 240.0f - intensity * 240.0f;
        float saturation = 1.0f;
        float value = 1.0f;
        float alpha = 1.0f;

        // 低强度点更透明
        if (intensity < 0.3f) {
            alpha = intensity / 0.3f * 0.7f + 0.3f;
        }

        return hsvToRgb(hue, saturation, value, alpha);
    }

    // 优化的刻度计算函数
    inline float calculateNiceTickStep(float range, int targetTicks = 6) {
        // 确保范围和目标刻度数有效
        if (range <= 0 || targetTicks <= 0) {
            return 1.0f;
        }

        // 计算粗略的步长
        float roughStep = range / (float) targetTicks;

        // 计算步长的数量级
        float magnitude = std::pow(10.0f, std::floor(std::log10(roughStep)));

        // 计算标准化步长
        float normalizedStep = roughStep / magnitude;

        // 选择美观的步长
        float niceStep;
        if (normalizedStep < 1.5f) {
            niceStep = 1.0f;
        } else if (normalizedStep < 3.0f) {
            niceStep = 2.0f;
        } else if (normalizedStep < 7.0f) {
            niceStep = 5.0f;
        } else {
            niceStep = 10.0f;
        }

        // 最终步长
        return niceStep * magnitude;
    }

    // 查找接近值的美观数字（避免小数）
    inline float findNiceNumber(float value, bool ceiling = false) {
        // 确定数量级
        float sign = (value >= 0) ? 1.0f : -1.0f;
        float absValue = std::abs(value);

        if (absValue < 1e-6f) {
            return 0.0f; // 处理接近零的情况
        }

        float magnitude = std::pow(10.0f, std::floor(std::log10(absValue)));
        float normalized = absValue / magnitude; // 1.0 到 9.999...

        // 美观数字序列
        const float niceNumbers[] = {1.0f, 2.0f, 2.5f, 5.0f, 10.0f};

        if (ceiling) {
            // 向上取美观值
            for (float nice: niceNumbers) {
                if (normalized <= nice) {
                    return sign * nice * magnitude;
                }
            }
            return sign * 10.0f * magnitude; // 如果超过所有值
        } else {
            // 向下取美观值
            for (int i = sizeof(niceNumbers) / sizeof(niceNumbers[0]) - 1; i >= 0; i--) {
                if (normalized >= niceNumbers[i]) {
                    return sign * niceNumbers[i] * magnitude;
                }
            }
            return sign * 1.0f * magnitude; // 如果小于所有值
        }
    }

    // 优化的范围计算函数
    inline std::pair<float, float>
    calculateNiceRange(float min, float max, int targetTickCount = 6, bool enforcePositiveRange = true) {
        // 处理特殊情况
        if (min > max) {
            std::swap(min, max);
        }

        float range = max - min;

        // 对于非常小的范围，使用更精确的处理
        if (range < 0.001f) {
            range = 0.001f;
            max = min + range;
        }

        // 计算刻度步长
        float niceStep = calculateNiceTickStep(range, targetTickCount);

        // 计算美观的最小值（向下取整到最接近的步长倍数）
        float niceMin = std::floor(min / niceStep) * niceStep;

        // 强制全正数据有正下限
        if (enforcePositiveRange && min >= 0) {
            niceMin = std::max(0.0f, niceMin);
        }

        // 计算基础的美观最大值
        float niceMax = niceMin + std::ceil((max - niceMin) / niceStep) * niceStep;

        // 确保至少有targetTickCount-1个刻度
        int steps = std::round((niceMax - niceMin) / niceStep);
        if (steps < targetTickCount - 1) {
            niceMax = niceMin + (targetTickCount - 1) * niceStep;
        }

        return {niceMin, niceMax};
    }

    // 动态量程配置类
    class DynamicRange {
    private:
        int m_frameCounter = 0;
        // 当前显示范围
        float m_currentMin = 0.0f;
        float m_currentMax = 0.0f;

        // 目标范围
        float m_targetMin = 0.0f;
        float m_targetMax = 0.0f;

        // 状态标志
        bool m_initialized = false;

        // 数据平滑相关
        std::deque<std::pair<float, float> > m_recentDataRanges; // 记录最近的数据范围
        int m_stableRangeCounter = 0; // 稳定范围计数器
        std::pair<float, float> m_lastSignificantRange = {0, 0}; // 上次显著更新的数据范围
        int m_rangeLockCooldown = 0; // 范围锁定冷却时间

        // 新增：应用绝对范围限制并处理潜在冲突的辅助函数
        void applyAbsoluteLimits(float& minVal, float& maxVal) const {
            bool minClamped = false;
            bool maxClamped = false;

            // 应用下限
            if (m_config.absoluteMinLimit.has_value()) {
                float limit = m_config.absoluteMinLimit.value();
                if (minVal < limit) {
                    minVal = limit;
                    minClamped = true;
                }
            }

            // 应用上限
            if (m_config.absoluteMaxLimit.has_value()) {
                float limit = m_config.absoluteMaxLimit.value();
                if (maxVal > limit) {
                    maxVal = limit;
                    maxClamped = true;
                }
            }

            // 检查钳位后是否导致 maxVal <= minVal
            if (maxVal <= minVal) {
                // 尝试恢复一个有效的最小范围
                const float MINIMAL_RANGE_STEP = 0.001f; // 或者更智能的计算

                if (minClamped && maxClamped) {
                    // 如果同时被上下限钳位导致冲突，通常意味着 absoluteMinLimit >= absoluteMaxLimit
                    // 这种配置本身有问题，这里我们选择让 max 等于 min (优先下限)
                    qWarning() << "DynamicRange: absoluteMinLimit >= absoluteMaxLimit, range collapsed.";
                    maxVal = minVal;
                } else if (minClamped) {
                    // 被下限钳位导致冲突，尝试在下限基础上增加一个最小步长
                    maxVal = minVal + MINIMAL_RANGE_STEP;
                    // 再次检查是否超过上限（如果存在）
                    if (m_config.absoluteMaxLimit.has_value()) {
                        maxVal = std::min(maxVal, m_config.absoluteMaxLimit.value());
                    }
                    // 如果调整后仍然 max <= min (因为上限太接近下限)，则让 max 等于 min
                    if (maxVal <= minVal) maxVal = minVal;
                } else if (maxClamped) {
                    // 被上限钳位导致冲突，尝试在上限基础上减去一个最小步长
                    minVal = maxVal - MINIMAL_RANGE_STEP;
                    // 再次检查是否低于下限（如果存在）
                    if (m_config.absoluteMinLimit.has_value()) {
                        minVal = std::max(minVal, m_config.absoluteMinLimit.value());
                    }
                     // 如果调整后仍然 max <= min (因为下限太接近上限)，则让 min 等于 max
                    if (maxVal <= minVal) minVal = maxVal;
                } else {
                     // 既没有被钳位，但 max <= min （例如原始范围为0）
                     // 在 minVal 基础上创建一个最小范围
                     maxVal = minVal + MINIMAL_RANGE_STEP;
                      // 仍需检查上限
                     if (m_config.absoluteMaxLimit.has_value()) {
                         maxVal = std::min(maxVal, m_config.absoluteMaxLimit.value());
                     }
                     if (maxVal <= minVal) maxVal = minVal; // 最后保障
                }
            }
        }

    public:
        struct DynamicRangeConfig {
            //==================== 核心参数 ====================
            float expandThreshold; // 数据超出范围多少比例时立即扩展 (0.0表示任何超出都立即扩展)
            float shrinkThreshold; // 数据使用范围比例低于此值时收缩 (0.3表示使用不足70%时收缩)
            float significantShrinkRatio; // 数据范围显著缩小的比例阈值 (0.25表示缩小到1/4或更少时为显著)
            float bufferRatio; // 缓冲区比例 (0.1表示添加10%缓冲区)
            int updateInterval; // 多少帧检查一次量程 (5表示每5帧检查一次)
            int targetTickCount; // 期望的刻度数量 (6-8通常较好)

            //==================== 平滑控制 ====================
            float expandSmoothFactor; // 范围扩大时的平滑系数 (0.2表示每次向目标移动20%)
            float shrinkSmoothFactor; // 范围收缩时的平滑系数 (0.3表示每次向目标移动30%)
            float fastShrinkFactor; // 显著缩小时的加速系数 (0.6表示加速到60%每次)

            //==================== 范围控制 ====================
            float maxRangeExtensionRatio; // 最大范围扩展比例
            float dataUsageThreshold; // 数据使用率阈值，低于此值视为低使用率
            float maxDataRatio; // 数据最大值比例，低于此值视为显著缩小
            float significantChangeThreshold; // 显著变化的阈值
            bool enforcePositiveRangeForPositiveData; // 是否强制正数据使用正范围

            //==================== 范围限制 (新增) ====================
            std::optional<float> absoluteMinLimit; // 绝对最小量程限制
            std::optional<float> absoluteMaxLimit; // 绝对最大量程限制

            //==================== 波动控制 ====================
            int dataHistoryLength; // 保留最近数据范围的数量
            int stabilityThreshold; // 范围保持稳定的计数阈值
            int changeConfirmationCount; // 确认范围变化的持续帧数
            int rangeLockTime; // 范围锁定时间(帧数)
            bool useHistoricalRange; // 是否使用历史数据平均范围
            float outlierThreshold; // 异常值检测阈值（倍数）

            DynamicRangeConfig()
                : expandThreshold(0.0f), // 任何超出都立即扩展
                  shrinkThreshold(0.1f), // 使用不足70%时收缩
                  significantShrinkRatio(0.20f), // 缩小到1/4视为显著
                  bufferRatio(0.01f), // 添加10%的缓冲区
                  updateInterval(5), // 每5帧检查一次
                  targetTickCount(6), // 6个刻度
                  expandSmoothFactor(0.1f), // 扩大时平滑移动20%
                  shrinkSmoothFactor(0.3f), // 收缩时平滑移动30%
                  fastShrinkFactor(0.6f), // 显著缩小时加速到60%
                  maxRangeExtensionRatio(1.3f), // 最大扩展范围为数据范围的1.3倍
                  dataUsageThreshold(0.4f), // 数据使用率低于40%视为低使用
                  maxDataRatio(0.7f), // 数据最大值低于当前最大值70%视为显著缩小
                  significantChangeThreshold(0.03f), // 3%的变化视为显著
                  enforcePositiveRangeForPositiveData(true), // 默认强制正数据使用正范围
                  absoluteMinLimit(std::nullopt), // 新增：默认无绝对下限
                  absoluteMaxLimit(std::nullopt), // 新增：默认无绝对上限
                  dataHistoryLength(20), // 保留最近50帧的数据范围
                  stabilityThreshold(10), // 15帧内范围变化不大视为稳定
                  changeConfirmationCount(5), // 需要连续5帧确认范围变化
                  rangeLockTime(20), // 范围锁定30帧
                  useHistoricalRange(true), // 默认使用历史平均范围
                  outlierThreshold(1.5f) // 超过平均值2.5倍视为异常值
            {
            }
        };

        // 构造函数
        DynamicRange(float initialMin = 0.0f,
                     float initialMax = 0.0f,
                     const DynamicRangeConfig &config = DynamicRangeConfig())
            : m_currentMin(initialMin),
              m_currentMax(initialMax),
              m_targetMin(initialMin),
              m_targetMax(initialMax),
              m_config(config),
              m_initialized(false) {
        }

        DynamicRangeConfig getConfig() const { return m_config; }
        // 更新范围，返回是否需要重建绘图数据
        bool updateRange(const std::vector<float> &newData) {
            if (newData.empty())
                return false;

            // 1. 计算当前数据范围
            auto [minIt, maxIt] = std::minmax_element(newData.begin(), newData.end());
            float dataMin = *minIt;
            float dataMax = *maxIt;

            // 记录初始显示范围用于判断变化
            float oldMin = m_currentMin;
            float oldMax = m_currentMax;

            // 添加当前数据范围到历史记录
            m_recentDataRanges.push_back({dataMin, dataMax});
            if (m_recentDataRanges.size() > m_config.dataHistoryLength) {
                m_recentDataRanges.pop_front();
            }

            // 2. 检查范围锁定冷却时间
            if (m_rangeLockCooldown > 0) {
                m_rangeLockCooldown--;
                // 在冷却期间只处理范围扩展，不处理范围收缩
                if (dataMin < m_currentMin || dataMax > m_currentMax) {
                    updateTargetRange(dataMin, dataMax);
                    smoothUpdateCurrentRange(m_config.expandSmoothFactor);
                    return isSignificantChange(oldMin, oldMax);
                }
                return false;
            }

            // 3. 首次初始化
            if (!m_initialized) {
                initializeRange(dataMin, dataMax);
                m_initialized = true;
                m_lastSignificantRange = {dataMin, dataMax};
                return true;
            }

            // 4. 计算历史数据的平均范围和标准差
            std::pair<float, float> avgRange = calculateAverageRange();
            std::pair<float, float> stdevRange = calculateRangeStandardDeviation(avgRange);

            // 5. 检测当前数据是否为异常值
            bool isOutlier = isDataOutlier(dataMin, dataMax, avgRange, stdevRange);

            // 如果是异常值且范围已经稳定，则可能暂时忽略
            if (isOutlier && m_stableRangeCounter > m_config.stabilityThreshold) {
                // 处理异常值：仅当异常值扩大范围时才立即响应
                if (dataMin < m_currentMin || dataMax > m_currentMax) {
                    updateTargetRange(dataMin, dataMax);
                    smoothUpdateCurrentRange(m_config.expandSmoothFactor * 0.5f); // 使用较小的平滑因子
                    return isSignificantChange(oldMin, oldMax);
                }
                // 否则暂时忽略异常值
                return false;
            }

            // 6. 数据超出当前范围时
            if (dataMin < m_currentMin || dataMax > m_currentMax) {
                // 检查是否为持续性变化或者突发峰值
                if (isConsistentChange(dataMin, dataMax)) {
                    // 持续性变化，正常更新
                    updateTargetRange(m_config.useHistoricalRange ? avgRange.first : dataMin,
                                      m_config.useHistoricalRange ? avgRange.second : dataMax);
                    smoothUpdateCurrentRange(m_config.expandSmoothFactor);
                    m_lastSignificantRange = {dataMin, dataMax};
                    m_stableRangeCounter = 0;
                    return isSignificantChange(oldMin, oldMax);
                } else {
                    // 可能是突发峰值，使用较小的平滑因子
                    updateTargetRange(dataMin, dataMax);
                    smoothUpdateCurrentRange(m_config.expandSmoothFactor * 0.3f);
                    return isSignificantChange(oldMin, oldMax);
                }
            }

            // 7. 周期性检查量程是否需要更新
            if (++m_frameCounter % m_config.updateInterval != 0) {
                // 即使不是周期检查点，也增加稳定计数器
                if (isRangeStable(avgRange)) {
                    m_stableRangeCounter++;
                } else {
                    m_stableRangeCounter = 0;
                }
                return false;
            }
            m_frameCounter = 0;

            // 8. 检查数据范围与当前量程的差距
            float dataRange = m_config.useHistoricalRange ? (avgRange.second - avgRange.first) : (dataMax - dataMin);
            float currentRange = m_currentMax - m_currentMin;
            float usageRatio = dataRange / currentRange;

            // 9. 稳定性检查：如果范围一直稳定，考虑收缩
            bool canShrink = m_stableRangeCounter >= m_config.stabilityThreshold;

            // 数据显著小于当前范围且已经稳定一段时间，才考虑收缩
            if (canShrink && usageRatio < m_config.dataUsageThreshold &&
                (m_config.useHistoricalRange ? avgRange.second : dataMax) < m_currentMax * m_config.maxDataRatio) {
                // 确认是持续性变化，而不是临时波动
                if (isConsistentChange(m_config.useHistoricalRange ? avgRange.first : dataMin,
                                       m_config.useHistoricalRange ? avgRange.second : dataMax)) {
                    // 向目标迅速收缩
                    updateTargetRange(m_config.useHistoricalRange ? avgRange.first : dataMin,
                                      m_config.useHistoricalRange ? avgRange.second : dataMax);

                    // 使用适当的收缩因子
                    float shrinkFactor = m_config.shrinkSmoothFactor;
                    smoothUpdateCurrentRange(shrinkFactor);

                    // 设置范围锁定冷却时间，避免频繁切换
                    m_rangeLockCooldown = m_config.rangeLockTime;

                    // 记录显著变化的范围
                    m_lastSignificantRange = {
                        m_config.useHistoricalRange ? avgRange.first : dataMin,
                        m_config.useHistoricalRange ? avgRange.second : dataMax
                    };

                    m_stableRangeCounter = 0;

                    // 检查范围变化并返回
                    bool changed = isSignificantChange(oldMin, oldMax);
                    return changed;
                }
            }

            // 增加稳定计数器
            if (isRangeStable(avgRange)) {
                m_stableRangeCounter++;
            } else {
                m_stableRangeCounter = 0;
            }

            return false;
        }

        // 获取当前显示范围
        std::pair<float, float> getDisplayRange() const { return {m_currentMin, m_currentMax}; }

        // 设置显示范围
        void setDisplayRange(float min, float max) {
            m_currentMin = min;
            m_currentMax = max;
            m_targetMin = min;
            m_targetMax = max;
            m_initialized = true;
        }

        // 重置
        void reset() {
            m_initialized = false;
            m_currentMin = 0.0f;
            m_currentMax = 0.0f;
            m_targetMin = 0.0f;
            m_targetMax = 0.0f;
        }

        // 新增：用于设置绝对限制的公共接口
        void setAbsoluteLimits(std::optional<float> minLimit, std::optional<float> maxLimit) {
             if (minLimit.has_value() && maxLimit.has_value() && minLimit.value() > maxLimit.value()) {
                 qWarning() << "DynamicRange::setAbsoluteLimits - Invalid arguments: minLimit ("
                            << minLimit.value() << ") > maxLimit (" << maxLimit.value() << "). Ignoring limits.";
                 // 或者可以选择只设置一个，或者抛出错误等
                 return;
             }
             m_config.absoluteMinLimit = minLimit;
             m_config.absoluteMaxLimit = maxLimit;

             // 重要：设置限制后，需要立即重新应用这些限制到当前和目标范围
             // 并可能触发一次更新检查，以确保显示立即反映新限制

             // 1. 重新钳位当前目标值
             float targetMin = m_targetMin;
             float targetMax = m_targetMax;
             applyAbsoluteLimits(targetMin, targetMax);
             m_targetMin = targetMin;
             m_targetMax = targetMax;

             // 2. 重新钳位当前显示值
             float currentMin = m_currentMin;
             float currentMax = m_currentMax;
             applyAbsoluteLimits(currentMin, currentMax);
             m_currentMin = currentMin;
             m_currentMax = currentMax;

             // 3. (可选) 如果希望立即看到效果，可以强制一次显著变化判断
             // 这取决于外部代码如何响应 updateRange 的返回值
             // bool rangeSignificantlyChanged = isSignificantChange(oldMin, oldMax, true);
             // 或者让下一次 updateRange 自然处理
        }

    private:
        // 初始化范围
        void initializeRange(float dataMin, float dataMax) {
            // 添加缓冲区
            float range = std::max(0.001f, dataMax - dataMin);
            float buffer = range * m_config.bufferRatio;

            // 对于正数范围，确保最小值不会为负
            float min = (dataMin >= 0 && m_config.enforcePositiveRangeForPositiveData)
                            ? std::max(0.0f, dataMin - buffer)
                            : dataMin - buffer;
            float max = dataMax + buffer;

            // 计算美观范围
            auto [niceMin, niceMax] =
                    calculateNiceRange(min, max, m_config.targetTickCount,
                                       m_config.enforcePositiveRangeForPositiveData);

            // 应用绝对限制 (新增)
            applyAbsoluteLimits(niceMin, niceMax);

            // 设置为目标和当前范围
            m_targetMin = niceMin;
            m_targetMax = niceMax;
            m_currentMin = niceMin;
            m_currentMax = niceMax;
        }

        // 更新目标范围
        void updateTargetRange(float dataMin, float dataMax) {
            // 添加缓冲区，但限制缓冲区大小
            float range = std::max(0.001f, dataMax - dataMin);
            float buffer = std::min(range * m_config.bufferRatio, range * 0.1f);

            // 对于正数范围，确保最小值不会为负
            float min = (dataMin >= 0 && m_config.enforcePositiveRangeForPositiveData)
                            ? std::max(0.0f, dataMin - buffer)
                            : dataMin - buffer;
            float max = dataMax + buffer;

            // 计算当前数据使用率
            float currentDataRange = dataMax - dataMin;
            float currentDisplayRange = m_currentMax - m_currentMin;
             // 检查除数是否有效
            float usageRatio = (currentDisplayRange > 1e-6f) ? (currentDataRange / currentDisplayRange) : 1.0f;

            // 如果数据使用率很低，加速缩小目标范围
            if (usageRatio < m_config.dataUsageThreshold && dataMax < m_currentMax * m_config.maxDataRatio) {
                // 直接使用较小的缓冲区计算新范围
                buffer = buffer * 0.5f; // 减小缓冲区
                max = dataMax + buffer;
                min = (dataMin >= 0 && m_config.enforcePositiveRangeForPositiveData)
                          ? std::max(0.0f, dataMin - buffer)
                          : dataMin - buffer;
            }

            // 计算美观范围
            auto [niceMin, niceMax] =
                    calculateNiceRange(min, max, m_config.targetTickCount,
                                       m_config.enforcePositiveRangeForPositiveData);

            // 应用绝对限制 (新增)
            applyAbsoluteLimits(niceMin, niceMax);

            // 检查新范围是否与当前范围接近，避免微小变化触发更新
            if (std::abs(niceMin - m_targetMin) < 0.0001f && std::abs(niceMax - m_targetMax) < 0.0001f) {
                return; // 范围几乎相同，不更新目标
            }

            m_targetMin = niceMin;
            m_targetMax = niceMax;
        }

        // 平滑更新当前范围，接受平滑因子参数
        void smoothUpdateCurrentRange(float smoothFactor = -1.0f) {
            // qDebug() << "平滑更新当前范围:" << m_currentMin << "-" << m_currentMax;
            // 如果未指定平滑因子，使用默认值

            if (smoothFactor < 0.0f) {
                // 根据是扩大还是缩小选择不同的平滑因子
                bool isExpanding = (m_targetMax > m_currentMax) || (m_targetMin < m_currentMin);
                smoothFactor = isExpanding ? m_config.expandSmoothFactor : m_config.shrinkSmoothFactor;
            }

            // 使用平滑系数更新当前范围
            m_currentMin += (m_targetMin - m_currentMin) * smoothFactor;
            m_currentMax += (m_targetMax - m_currentMax) * smoothFactor;

#ifdef DEBUG
        // 保存调试信息
        float targetRange  = m_targetMax - m_targetMin;
        float currentRange = m_currentMax - m_currentMin;
        float rangeDiff    = std::abs(targetRange - currentRange) / std::max(0.001f, targetRange);

        当差距较大时输出调试信息
        if (rangeDiff > 0.1f) {
            qDebug() << "范围更新: 当前" << m_currentMin << "-" << m_currentMax << " 目标" << m_targetMin << "-"
                     << m_targetMax << " 平滑因子" << smoothFactor;
        }
#endif

            // 确保当前范围仍是美观范围
            // 注意：这里计算出的 niceMin/Max 不应再次应用 absolute limits，
            // 因为平滑的目标已经是被限制过的。这里的 calculateNiceRange 只是为了
            // 在平滑过程中保持刻度的美观性。最终 currentMin/Max 会自然趋近于
            // 被限制过的 targetMin/Max。
            auto [niceCurrentMin, niceCurrentMax] = calculateNiceRange(
                m_currentMin, m_currentMax, m_config.targetTickCount, m_config.enforcePositiveRangeForPositiveData);

            // 但是，为了绝对安全，可以在赋值前最后强制钳位一次当前值。
            // 这可以防止 calculateNiceRange 因某种原因产生略微超出限制的值。
            applyAbsoluteLimits(niceCurrentMin, niceCurrentMax);

            m_currentMin = niceCurrentMin;
            m_currentMax = niceCurrentMax;
        }

        // 判断范围是否发生显著变化
        bool isSignificantChange(float oldMin, float oldMax, bool forceUpdate = false) {
            if (forceUpdate) {
                return true;
            }

            // 检查是否完全相同（避免浮点误差的微小差异）
            const float EPSILON = 0.0001f;
            if (std::abs(m_currentMin - oldMin) < EPSILON && std::abs(m_currentMax - oldMax) < EPSILON) {
                return false;
            }

            // 计算变化比例
            float oldRange = std::max(0.001f, oldMax - oldMin);
            float minDiff = std::abs(m_currentMin - oldMin) / oldRange;
            float maxDiff = std::abs(m_currentMax - oldMax) / oldRange;

            // 使用配置的显著变化阈值
            return minDiff > m_config.significantChangeThreshold || maxDiff > m_config.significantChangeThreshold;
        }

        // 计算最近数据的平均范围
        std::pair<float, float> calculateAverageRange() {
            if (m_recentDataRanges.empty()) {
                return {0.0f, 0.0f};
            }

            float sumMin = 0.0f, sumMax = 0.0f;
            for (const auto &range: m_recentDataRanges) {
                sumMin += range.first;
                sumMax += range.second;
            }

            return {sumMin / m_recentDataRanges.size(), sumMax / m_recentDataRanges.size()};
        }

        // 计算范围的标准差
        std::pair<float, float> calculateRangeStandardDeviation(const std::pair<float, float> &avgRange) {
            if (m_recentDataRanges.size() < 2) {
                return {0.0f, 0.0f};
            }

            float sumSqDiffMin = 0.0f, sumSqDiffMax = 0.0f;
            for (const auto &range: m_recentDataRanges) {
                float diffMin = range.first - avgRange.first;
                float diffMax = range.second - avgRange.second;
                sumSqDiffMin += diffMin * diffMin;
                sumSqDiffMax += diffMax * diffMax;
            }

            float varianceMin = sumSqDiffMin / (m_recentDataRanges.size() - 1);
            float varianceMax = sumSqDiffMax / (m_recentDataRanges.size() - 1);

            return {std::sqrt(varianceMin), std::sqrt(varianceMax)};
        }

        // 检测数据是否为异常值
        bool isDataOutlier(float dataMin,
                           float dataMax,
                           const std::pair<float, float> &avgRange,
                           const std::pair<float, float> &stdevRange) {
            // 如果历史数据不够，无法判断
            if (m_recentDataRanges.size() < 10) {
                return false;
            }

            // 计算Z分数 (考虑使用绝对差值代替标准差以处理极端值)
            float zScoreMin = std::abs(dataMin - avgRange.first) / std::max(0.001f, stdevRange.first);
            float zScoreMax = std::abs(dataMax - avgRange.second) / std::max(0.001f, stdevRange.second);

            // 使用配置的异常值阈值
            return zScoreMin > m_config.outlierThreshold || zScoreMax > m_config.outlierThreshold;
        }

        // 检查范围是否稳定
        bool isRangeStable(const std::pair<float, float> &avgRange) {
            if (m_recentDataRanges.size() < 10) {
                return false;
            }

            // 计算最近几帧范围的变化程度
            float recentVariation = 0.0f;
            int sampleCount = 0;

            // 安全获取最近10帧数据的索引范围
            size_t startIdx = m_recentDataRanges.size() > 10 ? m_recentDataRanges.size() - 10 : 0;

            // 从最新的数据向前遍历，避免使用可能导致溢出的自减循环
            for (size_t i = m_recentDataRanges.size(); i > startIdx; i--) {
                size_t idx = i - 1; // 当前处理的索引

                float minDiff = std::abs(m_recentDataRanges[idx].first - avgRange.first);
                float maxDiff = std::abs(m_recentDataRanges[idx].second - avgRange.second);
                float totalRange = std::max(0.001f, avgRange.second - avgRange.first);

                recentVariation += (minDiff + maxDiff) / totalRange;
                sampleCount++;
            }

            // 使用实际处理的样本数计算平均变化
            recentVariation /= sampleCount;

            return recentVariation < 0.1f; // 变化小于10%视为稳定
        }

        // 检查是否为持续性变化
        bool isConsistentChange(float dataMin, float dataMax) {
            if (m_recentDataRanges.size() < m_config.changeConfirmationCount) {
                return true; // 数据不足时默认为持续变化
            }

            // 检查最近几帧数据是否都指向同一变化方向
            int expandCount = 0;
            int shrinkCount = 0;
            int processedFrames = 0;

            // 安全计算需要处理的帧数
            int framesToCheck = std::min(static_cast<int>(m_recentDataRanges.size()), m_config.changeConfirmationCount);

            // 从最新的数据开始向前检查
            for (int i = 1; i <= framesToCheck; i++) {
                size_t idx = m_recentDataRanges.size() - i;

                if (m_recentDataRanges[idx].first < m_currentMin || m_recentDataRanges[idx].second > m_currentMax) {
                    expandCount++;
                }

                if ((m_recentDataRanges[idx].second - m_recentDataRanges[idx].first) <
                    (m_currentMax - m_currentMin) * m_config.dataUsageThreshold) {
                    shrinkCount++;
                }

                processedFrames++;
            }

            // 如果是扩大变化，只需要有一定比例的帧超出范围
            if (dataMin < m_currentMin || dataMax > m_currentMax) {
                return expandCount >= processedFrames * 0.5f;
            }

            // 如果是缩小变化，需要大部分帧都表现出缩小趋势
            return shrinkCount >= processedFrames * 0.8f;
        }

    private:
        // 配置参数
        DynamicRangeConfig m_config;
    };
} // namespace ProGraphics
