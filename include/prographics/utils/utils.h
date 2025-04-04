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
    inline std::pair<float, float> calculateNiceRange(float min, float max, int targetTickCount = 6,
                                                      bool enforcePositiveRange = true) {
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

            DynamicRangeConfig()
                : expandThreshold(0.0f), // 任何超出都立即扩展
                  shrinkThreshold(0.1f), // 使用不足70%时收缩
                  significantShrinkRatio(0.20f), // 缩小到1/4视为显著
                  bufferRatio(0.01f), // 添加10%的缓冲区
                  updateInterval(10), // 每5帧检查一次
                  targetTickCount(6), // 6个刻度
                  expandSmoothFactor(0.1f), // 扩大时平滑移动20%
                  shrinkSmoothFactor(0.3f), // 收缩时平滑移动30%
                  fastShrinkFactor(0.8f), // 显著缩小时加速到60%
                  maxRangeExtensionRatio(1.3f), // 最大扩展范围为数据范围的1.3倍
                  dataUsageThreshold(0.4f), // 数据使用率低于40%视为低使用
                  maxDataRatio(0.7f), // 数据最大值低于当前最大值70%视为显著缩小
                  significantChangeThreshold(0.03f), // 3%的变化视为显著
                  enforcePositiveRangeForPositiveData(true) // 默认强制正数据使用正范围

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


            // 2. 首次初始化
            if (!m_initialized) {
                initializeRange(dataMin, dataMax);
                m_initialized = true;
                return true;
            }

            // 3. 数据超出当前范围时立即更新
            if (dataMin < m_currentMin || dataMax > m_currentMax) {
                updateTargetRange(dataMin, dataMax);

                // 使用扩展平滑因子
                float smoothFactor = m_config.expandSmoothFactor;
                smoothUpdateCurrentRange(smoothFactor);

                return isSignificantChange(oldMin, oldMax);
            }

            // 4. 周期性检查量程是否需要更新
            if (++m_frameCounter % m_config.updateInterval != 0) {
                return false;
            }
            m_frameCounter = 0;

            // 5. 检查数据范围与当前量程的差距
            float dataRange = dataMax - dataMin;
            float currentRange = m_currentMax - m_currentMin;
            float usageRatio = dataRange / currentRange;

            // 数据显著小于当前范围时加速收缩判断
            if (usageRatio < m_config.dataUsageThreshold && dataMax < m_currentMax * m_config.maxDataRatio) {
                // 向目标迅速收缩
                updateTargetRange(dataMin, dataMax);

                // 使用加速收缩因子
                float acceleratedShrinkFactor = m_config.fastShrinkFactor * 1.5f;
                smoothUpdateCurrentRange(acceleratedShrinkFactor);

                // 检查范围变化并返回
                bool changed = isSignificantChange(oldMin, oldMax, true); // 强制检查
                if (changed) {
                    // qDebug() << "检测到数据显著缩小，加速响应: " << m_currentMin << "-" << m_currentMax;
                }
                return changed;
            }

            // 判断是否应该收缩量程
            bool needShrink = false;
            bool isSignificantShrink = false;
            bool forceUpdate = false;

            // 判断数据是否显著小于当前范围
            if (usageRatio < (m_config.dataUsageThreshold - m_config.shrinkThreshold)) {
                needShrink = true;

                // 检查是否是显著缩小（数据范围比当前显示范围小很多）
                if (usageRatio < m_config.significantShrinkRatio) {
                    isSignificantShrink = true;
                    forceUpdate = true; // 显著缩小时强制更新
                    // qDebug() << "检测到数据显著缩小: " << usageRatio << "，强制更新量程";
                }
            }

            // 6. 需要收缩时，计算新的目标范围
            if (needShrink) {
                updateTargetRange(dataMin, dataMax);

                // 根据收缩程度选择平滑因子
                float smoothFactor = isSignificantShrink ? m_config.fastShrinkFactor : m_config.shrinkSmoothFactor;

                // 显著缩小时输出日志
#ifdef DEBUG
            if (isSignificantShrink) {
                qDebug() << "检测到数据显著缩小: 数据范围" << dataMin << "-" << dataMax << " 占当前显示范围的"
                         << (usageRatio * 100.0f) << "%"
                         << " 使用加速收缩因子:" << smoothFactor;
            }
#endif

                // 使用选定的平滑因子更新当前范围
                smoothUpdateCurrentRange(smoothFactor);
            }

            // 7. 检查范围是否发生显著变化
            return isSignificantChange(oldMin, oldMax, forceUpdate);
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

            // 计算美观范围并设置为目标和当前范围
            auto [niceMin, niceMax] = calculateNiceRange(min, max, m_config.targetTickCount,
                                                         m_config.enforcePositiveRangeForPositiveData);
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
            float usageRatio = currentDataRange / currentDisplayRange;

            // 如果数据使用率很低，加速缩小目标范围
            if (usageRatio < m_config.dataUsageThreshold && dataMax < m_currentMax * m_config.maxDataRatio) {
                // 直接使用较小的缓冲区计算新范围
                buffer = buffer * 0.5f; // 减小缓冲区
                max = dataMax + buffer;
                min = (dataMin >= 0 && m_config.enforcePositiveRangeForPositiveData)
                          ? std::max(0.0f, dataMin - buffer)
                          : dataMin - buffer;
            }

            // 计算美观范围并设置为目标范围
            auto [niceMin, niceMax] = calculateNiceRange(min, max, m_config.targetTickCount,
                                                         m_config.enforcePositiveRangeForPositiveData);

            // 检查新范围是否与当前范围接近，避免微小变化触发更新
            if (std::abs(niceMin - m_targetMin) < 0.0001f &&
                std::abs(niceMax - m_targetMax) < 0.0001f) {
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
            auto [niceMin, niceMax] = calculateNiceRange(m_currentMin, m_currentMax,
                                                         m_config.targetTickCount,
                                                         m_config.enforcePositiveRangeForPositiveData);
            m_currentMin = niceMin;
            m_currentMax = niceMax;
        }

        // 判断范围是否发生显著变化
        bool isSignificantChange(float oldMin, float oldMax, bool forceUpdate = false) {
            if (forceUpdate) {
                return true;
            }

            // 检查是否完全相同（避免浮点误差的微小差异）
            const float EPSILON = 0.0001f;
            if (std::abs(m_currentMin - oldMin) < EPSILON &&
                std::abs(m_currentMax - oldMax) < EPSILON) {
                return false;
            }

            // 计算变化比例
            float oldRange = std::max(0.001f, oldMax - oldMin);
            float minDiff = std::abs(m_currentMin - oldMin) / oldRange;
            float maxDiff = std::abs(m_currentMax - oldMax) / oldRange;

            // 使用配置的显著变化阈值
            return minDiff > m_config.significantChangeThreshold ||
                   maxDiff > m_config.significantChangeThreshold;
        }

    private:
        // 配置参数
        DynamicRangeConfig m_config;
    };
} // namespace ProGraphics
