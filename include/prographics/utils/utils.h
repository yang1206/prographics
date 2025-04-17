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
        float roughStep = range / static_cast<float>(targetTicks);

        // 计算步长的数量级
        float magnitude = std::pow(10.0f, std::floor(std::log10(roughStep)));

        // 计算标准化步长（相对于数量级）
        float normalizedStep = roughStep / magnitude;

        // 选择美观的步长倍数
        // 使用更多的可能值，减少大跳跃
        float niceStep;
        if (normalizedStep < 1.2f) {
            niceStep = 1.0f;
        } else if (normalizedStep < 2.5f) {
            niceStep = 2.0f;
        } else if (normalizedStep < 4.0f) {
            niceStep = 2.5f; // 添加2.5作为一个选项
        } else if (normalizedStep < 7.0f) {
            niceStep = 5.0f;
        } else {
            niceStep = 10.0f;
        }

        // 添加抗抽搐逻辑：
        // 检查是否非常接近下一个级别的步长
        // 如果非常接近（例如在边界的10%以内），选择更大的步长
        if (normalizedStep > 0.9f && normalizedStep < 1.0f) {
            niceStep = 1.0f; // 确保稳定在1.0
        } else if (normalizedStep > 1.8f && normalizedStep < 2.0f) {
            niceStep = 2.0f; // 确保稳定在2.0
        } else if (normalizedStep > 4.5f && normalizedStep < 5.0f) {
            niceStep = 5.0f; // 确保稳定在5.0
        } else if (normalizedStep > 9.0f && normalizedStep < 10.0f) {
            niceStep = 10.0f; // 确保稳定在10.0
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


    inline std::pair<float, float> calculateNiceRange(float min, float max, int targetTickCount = 6,
                                                      bool preserveBuffer = true,
                                                      float dataMax = 0.0f,
                                                      float bufferSize = 0.0f) {
        // 处理特殊情况
        if (min > max) {
            std::swap(min, max);
        }

        // 确保范围有最小宽度
        float minRangeWidth = 0.001f;
        if (max - min < minRangeWidth) {
            max = min + minRangeWidth;
        }

        // 记住原始数据值
        float originalMin = min;
        float originalMax = max;
        float originalRange = max - min;


        // 1. 确定适当的步长
        float niceStep = calculateNiceTickStep(originalRange, targetTickCount);

        // 2. 计算最小值 - 不再区分正负数据
        // 找到接近原始最小值的步长倍数
        float niceMin = std::floor(originalMin / niceStep) * niceStep;

        // 如果最小值偏离原始值太远，调整到下一个步长
        if (originalMin - niceMin > niceStep * 0.7f) {
            niceMin += niceStep;
        }

        // 3. 计算最大值
        // 首先计算需要的步长数量（至少覆盖原始范围）
        int minSteps = static_cast<int>(std::ceil((originalMax - niceMin) / niceStep));

        // 确保至少有目标刻度数-1个步长
        minSteps = std::max(minSteps, targetTickCount - 1);

        // 计算美观的最大值
        float niceMax = niceMin + minSteps * niceStep;

        // 4. 缓冲区处理
        if (preserveBuffer && dataMax > 0.0f) {
            // 计算当前缓冲区
            float currentBuffer = niceMax - dataMax;
            float targetBuffer = originalRange * bufferSize;

            // 如果缓冲区太小，增加一个步长
            if (currentBuffer < targetBuffer * 0.5f) {
                niceMax += niceStep;
            }
            // 如果缓冲区太大，减少一个步长（但确保仍然覆盖数据）
            else if (currentBuffer > targetBuffer * 2.0f && niceMax - niceStep > dataMax) {
                niceMax -= niceStep;
            }
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

    public:
        struct DynamicRangeConfig {
            //==================== 核心参数 ====================
            float bufferRatio; // 缓冲区比例，仅应用于上限，控制数据最大值之上的额外空间
            float responseSpeed; // 总体响应速度，控制量程变化的灵敏度
            bool smartAdjustment; // 智能调整模式（自动处理扩展和收缩的差异）

            //==================== 美观控制 ====================
            int targetTickCount; // 刻度数量

            //==================== 特殊功能 ====================
            bool enforceMinimumRange; // 是否使用最小量程
            float minimumRangeMax; // 最小量程上限值
            float minimumThreshold; // 使用最小量程的触发阈值
            bool useFixedRange; // 是否使用固定量程

            DynamicRangeConfig()
                : bufferRatio(0.3f), // 添加30%的上限缓冲区，使显示更美观
                  responseSpeed(0.7f), // 较高响应速度（范围0.1-1.0，越大响应越快）
                  smartAdjustment(true), // 启用智能调整
                  targetTickCount(6), // 6个刻度通常比较美观
                  enforceMinimumRange(true), // 默认启用最小量程
                  minimumRangeMax(5.0f), // 最小量程上限（0-5）
                  minimumThreshold(2.0f), // 数据小于2.0时使用最小量程
                  useFixedRange(false) // 默认不使用固定量程
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

        // 设置配置
        void setConfig(const DynamicRangeConfig &config) {
            m_config = config;
        }

        DynamicRangeConfig getConfig() const { return m_config; }

        // 更新范围，返回是否需要重建绘图数据
        bool updateRange(const std::vector<float> &newData) {
            if (newData.empty() || m_config.useFixedRange)
                return false;

            // 计算当前数据范围
            auto [minIt, maxIt] = std::minmax_element(newData.begin(), newData.end());
            float dataMin = *minIt;
            float dataMax = *maxIt;

            // 记录初始显示范围用于判断变化
            float oldMin = m_currentMin;
            float oldMax = m_currentMax;

            // 添加到历史数据
            m_recentDataRanges.push_back({dataMin, dataMax});
            if (m_recentDataRanges.size() > 10) {
                // 固定历史长度为20
                m_recentDataRanges.pop_front();
            }

            // 检查数据是否超出当前范围
            bool dataExceedsRange = (dataMin < m_currentMin || dataMax > m_currentMax);

            // 首次初始化
            if (!m_initialized) {
                initializeRange(dataMin, dataMax);
                m_initialized = true;
                return true;
            }

            // 超出范围时立即响应
            if (dataExceedsRange) {
                updateTargetRange(dataMin, dataMax);
                smoothUpdateCurrentRange(calculateSmoothFactor(true, false)); // 扩展平滑因子
                return true;
            }

            // 周期性检查量程是否需要收缩
            if (++m_frameCounter % 5 != 0) {
                // 每5帧检查一次
                return false;
            }
            m_frameCounter = 0;

            // 检查数据使用率
            float dataRange = dataMax - dataMin;
            float currentRange = m_currentMax - m_currentMin;
            float usageRatio = dataRange / currentRange;

            // 数据使用率过低时收缩范围
            if (usageRatio < 0.3f) {
                // 使用率低于30%时收缩
                float avgMax = calculateAverageMax();

                // 使用平均最大值来更新目标范围，避免因单帧波动导致量程不稳定
                updateTargetRange(dataMin, avgMax > dataMax ? avgMax : dataMax);
                smoothUpdateCurrentRange(calculateSmoothFactor(false, dataMax < m_currentMax * 0.5f));
                return isSignificantChange(oldMin, oldMax);
            }

            return false;
        }

        // 获取当前显示范围
        std::pair<float, float> getDisplayRange() const { return {m_currentMin, m_currentMax}; }

        // 设置显示范围
        void setDisplayRange(float min, float max, bool fixedRange = false) {
            // 直接设置目标范围
            m_targetMin = min;
            m_targetMax = max;

            // 直接应用，不使用平滑过渡
            m_currentMin = min;
            m_currentMax = max;

            // 如果设置为固定范围，禁用动态更新
            m_config.useFixedRange = fixedRange;

            // 如果是最小范围，也更新最小范围配置
            if (fixedRange && max <= 10.0f) {
                m_config.minimumRangeMax = max;
                m_config.enforceMinimumRange = true;
            }
        }

        // 直接设置目标范围
        void setTargetRange(float min, float max) {
            m_targetMin = min;
            m_targetMax = max;
        }

        // 重置
        void reset() {
            m_initialized = false;
            m_currentMin = 0.0f;
            m_currentMax = 0.0f;
            m_targetMin = 0.0f;
            m_targetMax = 0.0f;
            m_recentDataRanges.clear();
            m_stableRangeCounter = 0;
            m_lastSignificantRange = {0, 0};
            m_rangeLockCooldown = 0;
        }

    private:
        // 初始化范围
        void initializeRange(float dataMin, float dataMax) {
            float range = std::max(0.001f, dataMax - dataMin);
            float buffer = range * m_config.bufferRatio;

            // 最小值不添加缓冲区
            float min = dataMin;
            // 最大值添加缓冲区
            float max = dataMax + buffer;

            // 计算美观范围并设置为目标和当前范围
            auto [niceMin, niceMax] = calculateNiceRange(
                min, max, m_config.targetTickCount,
                true, dataMax, m_config.bufferRatio);

            m_targetMin = niceMin;
            m_targetMax = niceMax;
            m_currentMin = niceMin;
            m_currentMax = niceMax;

            // 记录调试信息
            qDebug() << "初始化范围: 数据[" << dataMin << "," << dataMax
                    << "] -> 显示[" << m_currentMin << "," << m_currentMax
                    << "] (缓冲区:" << (m_currentMax - dataMax) << ")";
        }

        // 更新目标范围
        void updateTargetRange(float dataMin, float dataMax) {
            float range = std::max(0.001f, dataMax - dataMin);
            float buffer = range * m_config.bufferRatio;

            // 最小值不添加缓冲区
            float rawMin = dataMin;
            // 最大值添加缓冲区
            float rawMax = dataMax + buffer;

            // 计算美观范围，移除enforcePositiveRange参数
            auto [niceMin, niceMax] = calculateNiceRange(
                rawMin, rawMax, m_config.targetTickCount,
                true, dataMax, m_config.bufferRatio);

            // 避免抽搐：如果目标范围与当前目标范围非常接近，保持不变
            if (m_initialized) {
                // 计算相对变化
                float currentRange = m_targetMax - m_targetMin;
                float minChange = std::abs(niceMin - m_targetMin) / currentRange;
                float maxChange = std::abs(niceMax - m_targetMax) / currentRange;

                // 如果变化非常小（小于5%），保持当前目标
                if (minChange < 0.05f && maxChange < 0.05f) {
                    return;
                }
            }

            m_targetMin = niceMin;
            m_targetMax = niceMax;

            // 输出调试信息
            qDebug() << "更新目标范围: 数据[" << dataMin << "," << dataMax
                    << "] -> 目标[" << m_targetMin << "," << m_targetMax
                    << "] (缓冲区:" << (m_targetMax - dataMax) << ")";
        }

        // 平滑更新当前范围，接受平滑因子参数
        void smoothUpdateCurrentRange(float smoothFactor = -1.0f) {
            // 如果未指定平滑因子，使用responseSpeed计算
            if (smoothFactor < 0.0f) {
                // 根据是扩大还是缩小选择不同的平滑因子
                bool isExpanding = (m_targetMax > m_currentMax) || (m_targetMin < m_currentMin);
                smoothFactor = calculateSmoothFactor(isExpanding, false);
            }

            // 限制平滑因子，避免过快变化
            smoothFactor = std::min(smoothFactor, 0.5f);

            // 使用平滑系数更新当前范围
            m_currentMin += (m_targetMin - m_currentMin) * smoothFactor;
            m_currentMax += (m_targetMax - m_currentMax) * smoothFactor;

            // 确保当前范围仍是美观范围，移除enforcePositiveRange参数
            auto [niceMin, niceMax] = calculateNiceRange(
                m_currentMin, m_currentMax, m_config.targetTickCount);
            m_currentMin = niceMin;
            m_currentMax = niceMax;

            // 输出调试信息
            qDebug() << "平滑更新: 当前[" << m_currentMin << "," << m_currentMax
                    << "] (平滑因子:" << smoothFactor << ")";
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

            // 使用响应速度作为变化阈值的基础
            float threshold = 0.1f * (1.0f - m_config.responseSpeed * 0.5f);

            // 返回是否发生显著变化
            bool changed = minDiff > threshold || maxDiff > threshold;
            // if (changed) {
            //     qDebug() << "显著变化: 旧[" << oldMin << "," << oldMax
            //             << "] -> 新[" << m_currentMin << "," << m_currentMax
            //             << "] (阈值:" << threshold << ")";
            // }
            return changed;
        }

        // 计算最近数据的平均最大值
        float calculateAverageMax() {
            if (m_recentDataRanges.empty()) {
                return 0.0f;
            }

            float sum = 0.0f;
            for (const auto &range: m_recentDataRanges) {
                sum += range.second;
            }

            return sum / m_recentDataRanges.size();
        }

        // 查找最近n帧的最大值
        float findRecentMaxValue(int n) {
            if (m_recentDataRanges.empty()) {
                return 0.0f;
            }

            float maxValue = 0.0f;
            int count = std::min(n, static_cast<int>(m_recentDataRanges.size()));

            for (int i = 0; i < count; i++) {
                size_t idx = m_recentDataRanges.size() - 1 - i;
                maxValue = std::max(maxValue, m_recentDataRanges[idx].second);
            }

            return maxValue;
        }

        // 检查两个范围是否基本相等
        bool isRangeEqual(float min1, float min2, float max1, float max2) {
            const float EPSILON = 0.001f;
            return std::abs(min1 - min2) < EPSILON && std::abs(max1 - max2) < EPSILON;
        }

        // 计算平滑因子
        float calculateSmoothFactor(bool isExpanding, bool isSignificant) {
            float baseFactor = m_config.responseSpeed; // 使用总体响应速度作为基础

            if (m_config.smartAdjustment) {
                if (isExpanding) {
                    // 扩展时快速响应
                    return baseFactor * 1.5f;
                } else if (isSignificant) {
                    // 显著收缩时也快速响应
                    return baseFactor * 1.2f;
                } else {
                    // 普通收缩时较慢响应
                    return baseFactor * 1.0f;
                }
            }

            return baseFactor; // 不使用智能调整时直接返回基础因子
        }

    private:
        // 配置参数
        DynamicRangeConfig m_config;
    };
} // namespace ProGraphics
