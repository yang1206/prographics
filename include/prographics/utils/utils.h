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
    inline std::pair<float, float> calculateNiceRange(float min, float max, int targetTickCount = 6) {
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

        // 特殊处理：如果原始最小值接近0且为正数，则使用0作为最小值
        if (min >= 0 && min < niceStep * 0.5f) {
            niceMin = 0.0f;
        }

        // 特殊处理：如果原始最小值为正数，但计算出的美观最小值为负数，则使用0作为最小值
        if (min > 0 && niceMin < 0) {
            niceMin = 0.0f;
        }

        // 特殊处理：对于较大的正数范围，如果最小值远离0，不应该将最小值设为0
        if (min > 1000.0f && max > 1000.0f) {
            // 确保最小值不会被强制设为0
            niceMin = std::floor(min / niceStep) * niceStep;
        }

        // 计算美观的最大值
        // 确保范围包含足够的刻度
        float niceMax = niceMin + std::ceil((max - niceMin) / niceStep) * niceStep;

        // 确保至少有targetTickCount个刻度
        int steps = std::round((niceMax - niceMin) / niceStep);
        if (steps < targetTickCount) {
            // 对于小范围数据，尝试减小步长而不是扩大范围
            if (range < 10.0f && steps >= targetTickCount - 1) {
                // 如果只差一个刻度，尝试减小步长
                float smallerStep = niceStep * 0.5f;
                if (smallerStep > 0) {
                    steps = std::round((niceMax - niceMin) / smallerStep);
                    if (steps >= targetTickCount) {
                        return {niceMin, niceMax}; // 使用更小的步长
                    }
                }
            }
            // 特殊处理：对于小范围数据，尝试使用更紧凑的范围
            if (range < 5.0f && max <= 2.0f) {
                // 对于[0, 1.5]这样的范围，尝试使用[0, 2.0]而不是[0, 2.5]
                if (niceMin == 0.0f && niceMax > max * 1.5f) {
                    // 找到一个更合适的最大值
                    float betterMax = std::ceil(max * 1.2f / niceStep) * niceStep;
                    if (betterMax >= max && (betterMax - niceMin) / niceStep >= targetTickCount - 1) {
                        niceMax = betterMax;
                    }
                }
            }
            niceMax = niceMin + targetTickCount * niceStep;
        }

        return {niceMin, niceMax};
    }

    // 动态量程配置类
    class DynamicRange {
    public:
        // 统一的范围参数配置结构体
        struct DynamicRangeConfig {
            //==================== 范围扩展与收缩控制 ====================
            float dataExceedThreshold; // 数据超出当前范围多少比例时立即扩展范围 (0.1表示超出10%)
            float dataUnderUtilizationThreshold; // 数据仅使用显示范围的多少比例时开始收缩 (0.3表示使用不足70%时收缩)
            float expandPaddingRatio; // 扩展范围时额外添加的缓冲区比例 (0.1表示添加10%的缓冲区)
            float shrinkPaddingRatio; // 收缩范围时保留的缓冲区比例 (0.1表示保留10%的缓冲区)
            float minRangeUpdateThreshold; // 范围变化超过此比例才更新显示 (0.08表示变化超过8%才更新)

            //==================== 刻度与显示控制 ====================
            int targetTickCount; // 坐标轴期望的刻度数量 (通常为5-7)

            //==================== 稳定性检测与处理 ====================
            int stabilityHistorySize; // 稳定性检测的历史数据点数量 (20表示跟踪最近20次更新)
            float dataFluctuationThreshold; // 数据波动低于此比例时判定为稳定 (0.2表示波动不超过当前范围的20%)
            int stableStateCountThreshold; // 连续检测到稳定状态多少次后强制更新范围 (3表示连续3次稳定后更新)

            //==================== 特殊情况处理 ====================
            float smallRangeThreshold; // 小范围数据阈值 (数据范围小于5.0时使用特殊处理)
            float minValuePaddingRatio; // 最小值的缓冲区比例 (0.0表示不为最小值添加额外缓冲区)
            float maxValuePaddingRatio; // 最大值的缓冲区比例 (0.01表示为最大值添加1%缓冲区)

            //==================== 收缩速度控制 ====================
            float normalShrinkSpeed; // 普通情况下的收缩速度 (0.05表示每次向目标收缩5%)
            float maxShrinkSpeed; // 最大允许的收缩速度 (0.2表示每次最多收缩20%)

            //==================== 小数据范围特殊处理 ====================
            float tinyDataRangeThreshold; // 微小数据范围阈值 (数据范围小于1.0时使用固定量程)
            float tinyDataFixedRange; // 微小数据使用的固定量程大小 (5.0表示使用固定量程5.0)
            bool enableTinyDataFixedRange; // 是否启用微小数据固定量程 (true表示启用)

            //==================== 更新频率控制 ====================
            int dataUpdateThrottleCount; // 数据更新多少次才进行动态量程处理 (10表示每10次数据更新处理一次)
            int periodicRangeCheckCount; // 多少次更新后强制检查范围 (50表示每50次更新强制检查一次)

            //==================== 全正值数据处理 ====================
            bool forceZeroMinForPositiveOnly; // 全正值数据时是否强制从0开始 (true表示强制从0开始)
            float positiveOnlyShrinkAcceleration; // 全正值数据时的收缩加速系数 (0.3表示比普通收缩快3倍)
            int positiveOnlyDetectionCount; // 需要连续检测到多少次全正值才启用特殊处理 (5表示连续5次)


            DynamicRangeConfig(): dataExceedThreshold(0.1f), // 数据超出范围10%时扩展
                                  dataUnderUtilizationThreshold(0.3f), // 数据使用不足70%时收缩
                                  expandPaddingRatio(0.10f), // 扩展时添加10%缓冲区
                                  shrinkPaddingRatio(0.1f), // 收缩时保留10%缓冲区
                                  minRangeUpdateThreshold(0.05f), // 范围变化超过8%才更新显示
                                  // 刻度与显示控制
                                  targetTickCount(6), // 期望显示6个刻度

                                  // 稳定性检测与处理
                                  stabilityHistorySize(20), // 跟踪最近20个数据点
                                  dataFluctuationThreshold(0.2f), // 波动不超过20%判定为稳定
                                  stableStateCountThreshold(3), // 连续3次稳定后强制更新

                                  // 特殊情况处理
                                  smallRangeThreshold(5.0f), // 范围小于5.0使用特殊处理
                                  minValuePaddingRatio(0.0f), // 最小值不添加缓冲区
                                  maxValuePaddingRatio(0.01f), // 最大值添加1%缓冲区

                                  // 收缩速度控制
                                  normalShrinkSpeed(0.15f), // 普通收缩速度5%
                                  maxShrinkSpeed(0.35f), // 最大收缩速度20%

                                  // 小数据范围特殊处理
                                  tinyDataRangeThreshold(1.0f), // 范围小于1.0使用固定量程
                                  tinyDataFixedRange(5.0f), // 微小数据使用固定量程5.0
                                  enableTinyDataFixedRange(false), // 默认不启用微小数据固定量程

                                  // 更新频率控制
                                  dataUpdateThrottleCount(10), // 每10次数据更新处理一次
                                  periodicRangeCheckCount(20), // 每50次更新强制检查一次

                                  // 全正值数据处理
                                  forceZeroMinForPositiveOnly(false), // 默认不强制全正值数据从0开始
                                  positiveOnlyShrinkAcceleration(0.1f), // 全正值数据收缩加速10%
                                  positiveOnlyDetectionCount(5) // 连续5次检测到全正值才启用特殊处理
            {
            }
        };

        // 构造函数
        DynamicRange(float initialMin = 0.0f,
                     float initialMax = 0.0f,
                     const DynamicRangeConfig &config = DynamicRangeConfig())
            : m_dataMin(initialMin),
              m_dataMax(initialMax),
              m_displayMin(initialMin),
              m_displayMax(initialMax),
              m_config(config),
              m_rangeInitialized(false) {
        }

        const DynamicRangeConfig &getConfig() const {
            return m_config;
        }

        // 设置配置
        void setConfig(const DynamicRangeConfig &config) {
            m_config = config;
        }


        // 更新范围，返回是否需要重建绘图数据
        bool updateRange(const std::vector<float> &newData) {
            if (newData.empty())
                return false;

            // 保存旧范围以检测变化
            float oldDisplayMin = m_displayMin;
            float oldDisplayMax = m_displayMax;

            // 计算新数据范围
            auto [minIt, maxIt] = std::minmax_element(newData.begin(), newData.end());
            float newMin = *minIt;
            float newMax = *maxIt;
            float newRange = newMax - newMin;

            // 检测数据范围突变 - 新增代码
            static float lastMaxRange = 0.0f;
            bool suddenRangeDecrease = false;

            if (m_rangeInitialized && lastMaxRange > 0.0f) {
                // 如果新数据范围显著小于历史最大范围（如缩小到30%或更少）
                float currentDisplayRange = m_displayMax - m_displayMin;
                if (newRange < currentDisplayRange * 0.3f &&
                    newMax < m_displayMax * 0.7f && // 确保最大值也显著减小
                    m_recentRanges.size() >= 3) {
                    // 确保有足够历史数据

                    // 检查最近几次的数据是否都在较小范围内
                    bool allRecentSmall = true;
                    for (size_t i = m_recentRanges.size() - 3; i < m_recentRanges.size(); i++) {
                        if (m_recentRanges[i].second - m_recentRanges[i].first > newRange * 1.5f) {
                            allRecentSmall = false;
                            break;
                        }
                    }

                    if (allRecentSmall) {
                        suddenRangeDecrease = true;
                        // qDebug() << "检测到数据范围显著缩小:"
                        //         << "当前:" << newMin << "-" << newMax
                        //         << "显示:" << m_displayMin << "-" << m_displayMax;
                    }
                }
            }

            // 更新历史最大范围
            lastMaxRange = std::max(lastMaxRange, m_displayMax - m_displayMin);

            // 检查数据是否全为正值
            bool currentDataAllPositive = true;
            for (const auto &value: newData) {
                if (value < 0) {
                    currentDataAllPositive = false;
                    break;
                }
            }

            // 跟踪连续的全正值数据
            if (currentDataAllPositive) {
                m_consecutivePositiveDataCount++;
                if (m_consecutivePositiveDataCount >= m_config.positiveOnlyShrinkAcceleration) {
                    m_allPositiveData = true;
                }
            } else {
                m_consecutivePositiveDataCount = 0;
                m_allPositiveData = false;
            }

            // 首次数据初始化范围
            if (!m_rangeInitialized) {
                m_dataMin = newMin;
                m_dataMax = newMax;

                // 计算初始显示范围（带缓冲）
                float dataRange = newMax - newMin;
                float buffer = dataRange * 0.001f; // 使用较小的缓冲区

                // 对于正数范围，确保最小值不会因缓冲区变为负数
                float minWithBuffer = (newMin >= 0) ? std::max(0.0f, newMin - buffer) : newMin - buffer;
                float maxWithBuffer = newMax + buffer;

                // 先检查是否为小数据情况
                if (m_config.enableTinyDataFixedRange && dataRange < m_config.tinyDataRangeThreshold) {
                    // qDebug() << "初始化检测到小数据:" << newMin << "到" << newMax;
                    // 计算范围中心
                    float center = (newMin + newMax) / 2.0f;
                    // 计算固定量程的一半
                    float halfRange = m_config.tinyDataFixedRange / 2.0f;
                    // 如果全部是正数据，确保下限不小于0
                    if (newMin >= 0 && center - halfRange < 0) {
                        m_displayMin = 0.0f;
                        m_displayMax = m_config.tinyDataFixedRange;
                    } else {
                        m_displayMin = center - halfRange;
                        m_displayMax = center + halfRange;
                    }

                    // 计算美观范围
                    auto [niceMin, niceMax] = calculateNiceRange(m_displayMin, m_displayMax, m_config.targetTickCount);
                    m_displayMin = niceMin;
                    m_displayMax = niceMax;
                } else {
                    // 正常计算美观范围
                    auto [initialMin, initialMax] =
                            calculateNiceRange(minWithBuffer, maxWithBuffer, m_config.targetTickCount);
                    m_displayMin = initialMin;
                    m_displayMax = initialMax;
                }
                m_rangeInitialized = true;

                // qDebug() << "初始化范围:" << newMin << "到" << newMax;
                // qDebug() << "缓冲后范围:" << minWithBuffer << "到" << maxWithBuffer;
                // qDebug() << "美观范围:" << m_displayMin << "到" << m_displayMax;

                return true;
            }

            // 更新实际数据范围
            bool dataRangeChanged = false;
            if (newMin < m_dataMin) {
                m_dataMin = newMin;
                dataRangeChanged = true;
            }
            if (newMax > m_dataMax) {
                m_dataMax = newMax;
                dataRangeChanged = true;
            }

            // 跟踪最近范围以检测稳定性
            m_recentRanges.push_back({newMin, newMax});
            if (m_recentRanges.size() > m_config.stabilityHistorySize) {
                m_recentRanges.pop_front();
            }

            if (++m_updateCounter % m_config.dataUpdateThrottleCount != 0 && !dataRangeChanged) {
                return false;
            }
            m_updateCounter = 0;

            // 检查数据稳定性
            bool forceUpdate = checkDataStability();

            // 检查数据是否超出当前显示范围
            bool dataOutOfRange = newMin < m_displayMin || newMax > m_displayMax;

            // 周期性范围检查或强制更新或数据超出范围
            if (++m_resetCounter % m_config.periodicRangeCheckCount == 0 ||
                forceUpdate || dataOutOfRange || suddenRangeDecrease) {
                m_resetCounter = 0;

                // 确保数据范围有效
                if (m_dataMin > m_dataMax) {
                    std::swap(m_dataMin, m_dataMax);
                }

                // 如果是突然范围减小，可以额外设置一个加速因子
                bool fastShrink = suddenRangeDecrease;

                // 计算新的显示范围
                auto [newDisplayMin, newDisplayMax] = calculateDisplayRange(forceUpdate || dataOutOfRange, fastShrink);

                // 检查显示范围是否发生显著变化
                bool rangeChanged = isRangeChangedSignificantly(oldDisplayMin, oldDisplayMax,
                                                                newDisplayMin, newDisplayMax,
                                                                forceUpdate || dataOutOfRange);

                if (rangeChanged) {
                    m_displayMin = newDisplayMin;
                    m_displayMax = newDisplayMax;

                    // qDebug() << "动态范围:" << m_displayMin << "到" << m_displayMax;
                    // qDebug() << "计算步长:" << calculateNiceTickStep(m_displayMax - m_displayMin, m_config.targetTickCount);

                    return true;
                }
            }

            return false;
        }

        // 获取实际数据范围
        std::pair<float, float> getDataRange() const { return {m_dataMin, m_dataMax}; }

        // 获取显示范围
        std::pair<float, float> getDisplayRange() const { return {m_displayMin, m_displayMax}; }

        // 设置显示范围
        void setDisplayRange(float min, float max) {
            m_displayMin = min;
            m_displayMax = max;
            m_rangeInitialized = true;
        }

        // 重置范围
        void reset() {
            m_rangeInitialized = false;
            m_dataMin = 0.0f;
            m_dataMax = 0.0f;
            m_displayMin = 0.0f;
            m_displayMax = 0.0f;
            m_recentRanges.clear();
            m_stableCounter = 0;
            m_dataStable = false;
            m_updateCounter = 0;
            m_resetCounter = 0;
        }

    private
    :
        // 检查数据稳定性，返回是否应该强制更新范围
        bool checkDataStability() {
            if (m_recentRanges.size() < m_config.stabilityHistorySize) {
                return false;
            }

            // 计算最近范围的极值
            float minRange = m_recentRanges[0].first;
            float maxRange = m_recentRanges[0].second;

            for (const auto &range: m_recentRanges) {
                minRange = std::min(minRange, range.first);
                maxRange = std::max(maxRange, range.second);
            }

            // 计算数据波动范围比例
            float fluctuation = 0.0f;
            if (m_displayMax > m_displayMin) {
                fluctuation = (maxRange - minRange) / (m_displayMax - m_displayMin);
            }

            // 如果数据波动很小，认为数据稳定
            if (fluctuation < m_config.dataFluctuationThreshold) {
                if (!m_dataStable) {
                    // 首次检测到稳定
                    m_stableCounter = 1;
                    m_stableMin = minRange;
                    m_stableMax = maxRange;
                    m_dataStable = true;
                } else {
                    // 持续稳定
                    m_stableCounter++;
                    // 更新稳定范围
                    m_stableMin = std::min(m_stableMin, minRange);
                    m_stableMax = std::max(m_stableMax, maxRange);
                }
            } else {
                // 数据不稳定
                m_dataStable = false;
                m_stableCounter = 0;
            }

            // 如果数据持续稳定一段时间，强制更新显示范围
            if (m_dataStable && m_stableCounter
                >=
                m_config.stableStateCountThreshold
            ) {
                // 使用稳定数据范围强制更新显示范围
                m_dataMin = m_stableMin;
                m_dataMax = m_stableMax;
                m_stableCounter = 0; // 重置计数器，避免频繁更新

                // qDebug() << "检测到数据稳定，强制更新量程:" << m_stableMin << "到" << m_stableMax;
                return true;
            }

            return false;
        }


        // 优化的显示范围计算
        std::pair<float, float> calculateDisplayRange(bool forceUpdate, bool fastShrink = false) {
            // 先检查小数据情况 - 在任何其他处理之前
            float dataRange = m_dataMax - m_dataMin;
            if (m_config.enableTinyDataFixedRange && dataRange < m_config.tinyDataRangeThreshold) {
                // 计算范围中心
                float center = (m_dataMin + m_dataMax) / 2.0f;
                // 计算固定量程的一半
                float halfRange = m_config.tinyDataFixedRange / 2.0f;
                // 如果全部是正数据，确保下限不小于0
                if (m_dataMin >= 0 && center - halfRange < 0) {
                    return calculateNiceRange(0.0f, m_config.tinyDataFixedRange, m_config.targetTickCount);
                }
                // 返回以中心为基准的固定量程
                return calculateNiceRange(center - halfRange, center + halfRange, m_config.targetTickCount);
            }

            // 当前显示范围
            float currentRange = m_displayMax - m_displayMin;

            // 检查数据是否超出当前显示范围
            bool dataOutOfRange = m_dataMin < m_displayMin || m_dataMax > m_displayMax;

            if (forceUpdate || dataOutOfRange) {
                // 需要扩展范围
                float expandedMin = m_dataMin;
                float expandedMax = m_dataMax;

                // 对于小范围数据，使用更小的缓冲区
                float bufferRatio = (dataRange < 10.0f) ? 0.05f : m_config.expandPaddingRatio;

                // 添加缓冲区
                float buffer = dataRange * bufferRatio;

                // 对于正数范围，确保最小值不会因缓冲区变为负数
                if (m_dataMin >= 0) {
                    expandedMin = std::max(0.0f, m_dataMin - buffer * m_config.minValuePaddingRatio);
                } else {
                    expandedMin = m_dataMin - buffer * m_config.minValuePaddingRatio;
                }

                expandedMax = m_dataMax + buffer * m_config.maxValuePaddingRatio;
                // 特殊处理：对于较大的正数范围，保持最小值接近数据最小值
                if (m_dataMin > 1000.0f && m_dataMax > 1000.0f) {
                    // 不要让最小值太远离数据最小值
                    expandedMin = m_dataMin - dataRange * 0.05f;
                }

                // 计算美观范围
                return calculateNiceRange(expandedMin, expandedMax, m_config.targetTickCount);
            } else {
                // 检测是否应该收缩范围
                float usageRatio = dataRange / currentRange;
                float dataUnderUtilizationThreshold = m_config.dataUnderUtilizationThreshold;

                // 如果检测到范围突变，使用更积极的收缩阈值
                if (fastShrink) {
                    dataUnderUtilizationThreshold = 0.1f; // 更积极的收缩阈值
                }

                if (usageRatio < (1.0f - dataUnderUtilizationThreshold) ||
                    (m_allPositiveData && m_displayMin < 0)) {
                    // 基本收缩速度
                    float shrinkStep = m_config.normalShrinkSpeed;

                    // 计算当前范围与数据范围的差异比例
                    float rangeDiffRatio = currentRange / (dataRange > 0.001f ? dataRange : 0.001f);

                    // 如果差异很大或者是快速收缩模式，使用更积极的收缩速度
                    if (rangeDiffRatio > 3.0f || fastShrink) {
                        // 差异越大，收缩越快，但不超过最大速度
                        shrinkStep = std::min(m_config.maxShrinkSpeed,
                                              shrinkStep * std::min(3.0f, rangeDiffRatio / 2.0f));

                        if (fastShrink) {
                            // 快速收缩模式时使用更激进的收缩步长
                            shrinkStep = std::min(0.5f, shrinkStep * 3.0f);
                            // qDebug() << "使用快速收缩步长:" << shrinkStep;
                        } else {
                            // qDebug() << "数据范围差异大，使用加速收缩步长:" << shrinkStep;
                        }
                    }

                    // 目标范围
                    float targetMin = m_dataMin;

                    // 当数据全为正值且配置启用了强制零最小值，强制目标最小值为0
                    if (m_allPositiveData && m_config
                        .
                        forceZeroMinForPositiveOnly
                    ) {
                        targetMin = 0.0f;
                        // qDebug() << "检测到全正值数据，强制目标最小值为0";
                    }
                    // 对于正数范围，确保最小值不会变为负数
                    else if (m_dataMin >= 0) {
                        targetMin = std::max(0.0f, targetMin);
                    }

                    float targetMax = m_dataMax + dataRange * m_config.maxValuePaddingRatio;

                    // 计算新范围
                    float newMin = m_displayMin + (targetMin - m_displayMin) * shrinkStep;
                    float newMax = m_displayMax + (targetMax - m_displayMax) * shrinkStep;

                    // 对于全正值数据，如果计算出的新最小值仍然为负，直接调整为0
                    if (m_allPositiveData && newMin < 0) {
                        newMin = 0.0f;
                    }

                    // 计算美观范围
                    return calculateNiceRange(newMin, newMax, m_config.targetTickCount);
                }
            }

            // 默认返回当前范围
            return {m_displayMin, m_displayMax};
        }

        // 检查范围是否发生显著变化
        bool isRangeChangedSignificantly(float oldMin, float oldMax,
                                         float newMin, float newMax,
                                         bool forceUpdate) {
            // 如果强制更新，则始终返回true
            if (forceUpdate) {
                return true;
            }

            // 计算范围变化比例
            float oldRange = oldMax - oldMin;
            float newRange = newMax - newMin;

            // 避免除以零
            if (oldRange < 0.001f) {
                oldRange = 0.001f;
            }

            float rangeChangeRatio = std::abs(newRange - oldRange) / oldRange;

            // 检查最小值和最大值的变化
            float minChangeRatio = std::abs(newMin - oldMin) / oldRange;
            float maxChangeRatio = std::abs(newMax - oldMax) / oldRange;

            // 如果范围变化比例或最小值/最大值变化比例超过阈值，则认为范围发生了显著变化
            return rangeChangeRatio > m_config.minRangeUpdateThreshold ||
                   minChangeRatio > m_config.minRangeUpdateThreshold ||
                   maxChangeRatio > m_config.minRangeUpdateThreshold;
        }

        // 数据范围
        float m_dataMin = 0.0f;
        float m_dataMax = 0.0f;

        // 显示范围
        float m_displayMin = 0.0f;
        float m_displayMax = 0.0f;

        // 配置参数
        DynamicRangeConfig m_config;

        // 状态标志
        bool m_rangeInitialized = false;

        // 用于稳定性检测
        std::deque<std::pair<float, float> > m_recentRanges;
        bool m_dataStable = false;
        int m_stableCounter = 0;
        float m_stableMin = 0.0f;
        float m_stableMax = 0.0f;

        // 计数器
        int m_updateCounter = 0;
        int m_resetCounter = 0;

        // 全正值数据跟踪
        int m_consecutivePositiveDataCount = 0;
        bool m_allPositiveData = false;
    };
} // namespace ProGraphics
