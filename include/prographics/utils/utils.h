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
        if (range <= 0 || targetTicks <= 1) {
            return 1.0f;
        }

        // 关键：刻度数 = 步数 + 1，所以除以 (targetTicks - 1)
        float roughStep = range / static_cast<float>(targetTicks - 1);
        float magnitude = std::pow(10.0f, std::floor(std::log10(roughStep)));
        float normalizedStep = roughStep / magnitude;

        // 简化的阈值：只用 1, 2, 5, 10
        float niceStep;
        if (normalizedStep <= 1.0f) {
            niceStep = 1.0f;
        } else if (normalizedStep <= 2.0f) {
            niceStep = 2.0f;
        } else if (normalizedStep <= 5.0f) {
            niceStep = 5.0f;
        } else {
            niceStep = 10.0f;
        }

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
        // 量程模式枚举
        enum class RangeMode {
            Uninitialized, // 未初始化
            Fixed, // 固定量程（使用初始范围）
            Dynamic // 动态量程（根据数据调整）
        };

        int m_frameCounter = 0;
        // 当前显示范围
        float m_currentMin = 0.0f;
        float m_currentMax = 0.0f;

        // 目标范围
        float m_targetMin = 0.0f;
        float m_targetMax = 0.0f;

        // 状态管理
        RangeMode m_mode = RangeMode::Uninitialized;

        // 保存初始范围
        float m_initialMin = 0.0f;
        float m_initialMax = 0.0f;

        // 数据平滑相关
        std::array<std::pair<float, float>, 10> m_recentDataRanges;
        int m_recentDataIndex = 0;
        int m_recentDataCount = 0;
        float m_recentMaxSum = 0.0f;
        int m_stableRangeCounter = 0;

    public:
        struct DynamicRangeConfig {
            // ========== 核心参数 ==========
            float bufferRatio = 0.2f; // 上限缓冲区比例（20%）
            float responseSpeed = 0.8f; // 响应速度（0.1-1.0）
            bool smartAdjustment = true; // 智能调整（扩展快，收缩慢）

            // ========== 美观控制 ==========
            int targetTickCount = 6; // 目标刻度数量
            // ========== 范围控制 ==========
            bool enableRangeRecovery = true; // 启用范围恢复功能
            int recoveryFrameThreshold = 20; // 恢复需要的稳定帧数
            float recoveryRangeRatio = 0.8f; // 恢复判断的范围比例阈值
            float sameValueRangeRatio = 0.15f; // 相同值扩展比例（15%）

            // ========== 硬限制 ==========
            bool enableHardLimits = false; // 显式控制是否启用
            float hardLimitMin = -1000.0f; // 硬限制最小值
            float hardLimitMax = 1000.0f; // 硬限制最大值

            DynamicRangeConfig()
                : bufferRatio(0.3f), // 添加30%的上限缓冲区，使显示更美观
                  responseSpeed(0.7f), // 启用智能调整
                  sameValueRangeRatio(0.1f) // 默认+10%
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
              m_initialMin(initialMin),
              m_initialMax(initialMax),
              m_config(config),
              m_mode(RangeMode::Uninitialized) {
        }

        // 设置配置
        void setConfig(const DynamicRangeConfig &config) {
            m_config = config;
        }

        DynamicRangeConfig getConfig() const { return m_config; }

        // 更新范围，返回是否需要重建绘图数据
        bool updateRange(const std::vector<float> &newData) {
            if (newData.empty())
                return false;

            auto [minIt, maxIt] = std::minmax_element(newData.begin(), newData.end());
            float dataMin = *minIt;
            float dataMax = *maxIt;

            float oldMin = m_currentMin;
            float oldMax = m_currentMax;

            if (m_recentDataCount < 10) {
                m_recentDataRanges[m_recentDataCount] = {dataMin, dataMax};
                m_recentMaxSum += dataMax;
                m_recentDataCount++;
            } else {
                m_recentMaxSum -= m_recentDataRanges[m_recentDataIndex].second;
                m_recentDataRanges[m_recentDataIndex] = {dataMin, dataMax};
                m_recentMaxSum += dataMax;
                m_recentDataIndex = (m_recentDataIndex + 1) % 10;
            }

            bool needsRebuild = false;
            switch (m_mode) {
                case RangeMode::Uninitialized:
                    needsRebuild = handleUninitializedMode(dataMin, dataMax);
                    break;

                case RangeMode::Fixed:
                    needsRebuild = handleFixedMode(dataMin, dataMax);
                    break;

                case RangeMode::Dynamic:
                    needsRebuild = handleDynamicMode(dataMin, dataMax, oldMin, oldMax);
                    break;
            }

            applyHardLimitsToCurrentRange();
            return needsRebuild;
        }


        // 获取当前显示范围
        std::pair<float, float> getDisplayRange() const { return {m_currentMin, m_currentMax}; }

        // 设置显示范围
        void setDisplayRange(float min, float max) {
            // 直接设置目标范围
            m_targetMin = min;
            m_targetMax = max;

            // 直接应用，不使用平滑过渡
            m_currentMin = min;
            m_currentMax = max;

            applyHardLimitsToCurrentRange();
        }

        // 直接设置目标范围
        void setTargetRange(float min, float max) {
            m_targetMin = min;
            m_targetMax = max;
        }

        void setInitialRange(float min, float max) {
            m_initialMin = min;
            m_initialMax = max;

            if (m_mode == RangeMode::Uninitialized || m_mode == RangeMode::Fixed) {
                m_currentMin = min;
                m_currentMax = max;
                m_targetMin = min;
                m_targetMax = max;
            }
        }

        std::pair<float, float> getInitialRange() const {
            return {m_initialMin, m_initialMax};
        }

        void setHardLimits(float min, float max, bool enabled = true) {
            m_config.hardLimitMin = min;
            m_config.hardLimitMax = max;
            m_config.enableHardLimits = enabled;

            if (enabled) {
                applyHardLimitsToCurrentRange();
            }
        }

        std::pair<float, float> getHardLimits() const {
            return {m_config.hardLimitMin, m_config.hardLimitMax};
        }

        void enableHardLimits(bool enabled) {
            m_config.enableHardLimits = enabled;
            if (enabled) {
                applyHardLimitsToCurrentRange();
            }
        }

        bool isHardLimitsEnabled() const {
            return m_config.enableHardLimits;
        }


        // 重置
        void reset() {
            m_mode = RangeMode::Uninitialized;
            m_currentMin = 0.0f;
            m_currentMax = 0.0f;
            m_targetMin = 0.0f;
            m_targetMax = 0.0f;
            m_recentDataIndex = 0;
            m_recentDataCount = 0;
            m_recentMaxSum = 0.0f;
            m_stableRangeCounter = 0;
        }

        bool isDataExceedingRange(float dataMin, float dataMax) const {
            return dataMin < m_currentMin || dataMax > m_currentMax;
        }

        std::pair<float, float> clampData(float value, bool isMin) const {
            if (isMin) {
                return {m_currentMin, value > m_currentMax ? m_currentMax : value};
            } else {
                return {value < m_currentMin ? m_currentMin : value, m_currentMax};
            }
        }

    private:
        // ===== 状态处理函数 =====

        /// 处理未初始化状态
        bool handleUninitializedMode(float dataMin, float dataMax) {
            bool dataWithinInitialRange = isDataWithinInitialRange(dataMin, dataMax);

            if (dataWithinInitialRange) {
                // 数据在初始范围内，使用固定量程
                m_currentMin = m_initialMin;
                m_currentMax = m_initialMax;
                m_targetMin = m_initialMin;
                m_targetMax = m_initialMax;
                m_mode = RangeMode::Fixed;
                return false; // 不需要重建
            } else {
                // 数据超出初始范围，启动动态量程
                initializeRange(dataMin, dataMax);
                m_mode = RangeMode::Dynamic;
                return true; // 需要重建
            }
        }

        /// 处理固定量程模式
        bool handleFixedMode(float dataMin, float dataMax) {
            bool dataWithinInitialRange = isDataWithinInitialRange(dataMin, dataMax);

            if (dataWithinInitialRange) {
                // 数据仍在初始范围内，保持固定量程
                return false;
            } else {
                // 数据突破初始范围，切换到动态量程
                initializeRange(dataMin, dataMax);
                m_mode = RangeMode::Dynamic;
                m_stableRangeCounter = 0; // 重置恢复计数器
                return true; // 需要重建
            }
        }

        /// 处理动态量程模式
        bool handleDynamicMode(float dataMin, float dataMax, float oldMin, float oldMax) {
            // 检查是否应该恢复到固定量程
            if (m_config.enableRangeRecovery) {
                if (isDataWithinInitialRange(dataMin, dataMax)) {
                    // 数据在初始范围内，增加稳定计数器
                    m_stableRangeCounter++;

                    if (m_stableRangeCounter >= m_config.recoveryFrameThreshold) {
                        // 数据已连续稳定在初始范围内，恢复固定量程
                        m_currentMin = m_initialMin;
                        m_currentMax = m_initialMax;
                        m_targetMin = m_initialMin;
                        m_targetMax = m_initialMax;
                        m_mode = RangeMode::Fixed;
                        m_stableRangeCounter = 0;
                        return true; // 需要重建
                    }
                } else {
                    // 数据超出初始范围，重置计数器
                    m_stableRangeCounter = 0;
                }
            }

            // 执行动态量程调整逻辑
            return performDynamicAdjustment(dataMin, dataMax, oldMin, oldMax);
        }

        /// 执行动态量程调整
        bool performDynamicAdjustment(float dataMin, float dataMax, float oldMin, float oldMax) {
            // 检查数据是否超出当前范围
            bool dataExceedsRange = (dataMin < m_currentMin || dataMax > m_currentMax);

            if (dataExceedsRange) {
                // 数据超出范围，立即扩展
                updateTargetRange(dataMin, dataMax);
                smoothUpdateCurrentRange(calculateSmoothFactor(true, false));
                return true; // 需要重建
            }

            // 周期性检查是否需要收缩
            if (++m_frameCounter % 5 != 0) {
                return false;
            }
            m_frameCounter = 0;

            // 检查数据使用率
            float dataRange = dataMax - dataMin;
            float currentRange = m_currentMax - m_currentMin;
            float usageRatio = dataRange / currentRange;

            if (usageRatio < 0.3f) {
                // 数据使用率过低，收缩范围
                float avgMax = calculateAverageMax();
                updateTargetRange(dataMin, avgMax > dataMax ? avgMax : dataMax);
                smoothUpdateCurrentRange(calculateSmoothFactor(false, dataMax < m_currentMax * 0.5f));
                return isSignificantChange(oldMin, oldMax);
            }

            return false;
        }

        /// 检查数据是否适合恢复到初始范围
        bool isDataWithinInitialRange(float dataMin, float dataMax) const {
            float dataRange = dataMax - dataMin;
            float initialRange = m_initialMax - m_initialMin;

            if (initialRange < 1e-6f) {
                return true;
            }

            return dataRange <= initialRange * m_config.recoveryRangeRatio;
        }

        // ===== 原有私有方法 =====

        // 初始化范围
        void initializeRange(float dataMin, float dataMax) {
            if (dataMin >= m_initialMin && dataMax <= m_initialMax) {
                // 数据在初始范围内，保持初始范围不变
                m_currentMin = m_initialMin;
                m_currentMax = m_initialMax;
                m_targetMin = m_initialMin;
                m_targetMax = m_initialMax;
                // qDebug() << "initializeRange: 数据在初始范围内，保持初始范围["
                //         << m_initialMin << "," << m_initialMax << "]";
                return;
            }

            float dataRange = dataMax - dataMin;

            // 检查是否为相同值或极小差值的情况
            const float MIN_MEANINGFUL_RANGE = 1.0f; // 最小有意义的范围

            if (dataRange < MIN_MEANINGFUL_RANGE) {
                // 使用 sameValueRangeRatio 逻辑
                float centerValue = (dataMin + dataMax) / 2.0f; // 使用中心值
                float extension;

                if (std::abs(centerValue) < 1e-6f) {
                    // 中心值接近0，使用固定扩展
                    extension = 5.0f; // 默认扩展到±5
                    m_currentMin = centerValue - extension;
                    m_currentMax = centerValue + extension;
                } else {
                    // 中心值不为0，使用百分比扩展
                    extension = std::abs(centerValue) * m_config.sameValueRangeRatio;
                    // 确保最小扩展量
                    extension = std::max(extension, 2.0f);

                    m_currentMin = centerValue - extension;
                    m_currentMax = centerValue + extension;
                }

                // 计算美观范围
                auto [niceMin, niceMax] = calculateNiceRange(
                    m_currentMin, m_currentMax, m_config.targetTickCount);

                m_targetMin = niceMin;
                m_targetMax = niceMax;
                m_currentMin = niceMin;
                m_currentMax = niceMax;
                return;
            }

            // 正常的动态范围处理
            float buffer = dataRange * m_config.bufferRatio;

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

            // qDebug() << "正常初始化范围: 数据[" << dataMin << "," << dataMax
            //         << "] -> 显示[" << m_currentMin << "," << m_currentMax
            //         << "] (缓冲区:" << (m_currentMax - dataMax) << ")";
        }

        void applyHardLimitsToCurrentRange() {
            if (!m_config.enableHardLimits) return;


            m_currentMin = std::max(m_currentMin, m_config.hardLimitMin);
            m_currentMax = std::min(m_currentMax, m_config.hardLimitMax);
            m_targetMin = std::max(m_targetMin, m_config.hardLimitMin);
            m_targetMax = std::min(m_targetMax, m_config.hardLimitMax);
        }

        // 更新目标范围
        void updateTargetRange(float dataMin, float dataMax) {
            float dataRange = dataMax - dataMin;

            // 检查是否为相同值或极小差值的情况
            const float MIN_MEANINGFUL_RANGE = 1.0f;

            if (dataRange < MIN_MEANINGFUL_RANGE) {
                float centerValue = (dataMin + dataMax) / 2.0f;
                float extension;

                if (std::abs(centerValue) < 1e-6f) {
                    extension = 5.0f;
                    m_targetMin = centerValue - extension;
                    m_targetMax = centerValue + extension;
                } else {
                    extension = std::abs(centerValue) * m_config.sameValueRangeRatio;
                    extension = std::max(extension, 2.0f);
                    m_targetMin = centerValue - extension;
                    m_targetMax = centerValue + extension;
                }
                // 计算美观范围
                auto [niceMin, niceMax] = calculateNiceRange(
                    m_targetMin, m_targetMax, m_config.targetTickCount);

                m_targetMin = niceMin;
                m_targetMax = niceMax;

                return;
            }

            // 正常处理逻辑
            float buffer = dataRange * m_config.bufferRatio;
            float rawMin = dataMin;
            float rawMax = dataMax + buffer;

            auto [niceMin, niceMax] = calculateNiceRange(
                rawMin, rawMax, m_config.targetTickCount,
                true, dataMax, m_config.bufferRatio);

            // 避免抽搐：如果目标范围与当前目标范围非常接近，保持不变
            if (m_mode != RangeMode::Uninitialized) {
                float currentRange = m_targetMax - m_targetMin;
                float minChange = std::abs(niceMin - m_targetMin) / currentRange;
                float maxChange = std::abs(niceMax - m_targetMax) / currentRange;

                if (minChange < 0.05f && maxChange < 0.05f) {
                    return;
                }
            }

            m_targetMin = niceMin;
            m_targetMax = niceMax;

            // qDebug() << "正常更新目标范围: 数据[" << dataMin << "," << dataMax
            //         << "] -> 目标[" << m_targetMin << "," << m_targetMax << "]";
        }

        // 平滑更新当前范围，接受平滑因子参数
        void smoothUpdateCurrentRange(float smoothFactor = -1.0f) {
            if (smoothFactor < 0.0f) {
                bool isExpanding = (m_targetMax > m_currentMax) || (m_targetMin < m_currentMin);
                smoothFactor = calculateSmoothFactor(isExpanding, false);
            }

            smoothFactor = std::min(smoothFactor, 0.5f);

            m_currentMin += (m_targetMin - m_currentMin) * smoothFactor;
            m_currentMax += (m_targetMax - m_currentMax) * smoothFactor;
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

        float calculateAverageMax() {
            if (m_recentDataCount == 0) {
                return 0.0f;
            }
            return m_recentMaxSum / m_recentDataCount;
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
