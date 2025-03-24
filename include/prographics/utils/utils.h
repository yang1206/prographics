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

    // 刻度计算工具函数
    inline float calculateTickStep(float range) {
        if (range <= 10.0f)
            return 1.0f;
        if (range <= 20.0f)
            return 2.0f;
        if (range <= 50.0f)
            return 5.0f;
        if (range <= 100.0f)
            return 8.0f;
        if (range <= 200.0f)
            return 10.0f;
        if (range <= 500.0f)
            return 20.0f;
        if (range <= 1000.0f)
            return 50.0f;
        if (range <= 2000.0f)
            return 100.0f;
        if (range <= 3000.0f)
            return 200.0f;
        if (range <= 4000.0f)
            return 300.0f;
        if (range <= 5000.0f)
            return 500.0f;
        return std::pow(10.0f, std::floor(std::log10(range / 10.0f)));
    }

    // 计算美观的刻度步长
    inline float calculateNiceTickStep(float range, int targetTickCount = 5) {
        // 计算粗略步长
        float roughStep = range / targetTickCount;

        // 计算步长的数量级
        float magnitude = std::pow(10.0f, std::floor(std::log10(roughStep)));

        // 标准化步长到 1-10 范围
        float normalizedStep = roughStep / magnitude;

        // 选择最接近的美观步长
        float niceStep;
        if (normalizedStep < 1.5f)
            niceStep = 1.0f;
        else if (normalizedStep < 3.0f)
            niceStep = 2.0f;
        else if (normalizedStep < 7.0f)
            niceStep = 5.0f;
        else
            niceStep = 10.0f;

        return niceStep * magnitude;
    }

    // 计算美观的范围
    inline std::pair<float, float> calculateNiceRange(float min, float max, int tickCount = 5) {
        // 确保min和max不相等
        if (std::abs(max - min) < 1e-6f) {
            max = min + 1.0f;
        }

        // 确保min < max
        if (min > max) {
            std::swap(min, max);
        }

        // 计算范围
        float range = max - min;

        // 计算粗略的刻度间隔
        float roughStep = range / (tickCount - 1);

        // 计算步长的数量级
        float magnitude = std::pow(10.0f, std::floor(std::log10(roughStep)));

        // 标准化步长到1-10范围
        float normalizedStep = roughStep / magnitude;

        // 选择最接近的美观步长
        float niceStep;
        if (normalizedStep < 1.5f)
            niceStep = 1.0f;
        else if (normalizedStep < 3.0f)
            niceStep = 2.0f;
        else if (normalizedStep < 7.0f)
            niceStep = 5.0f;
        else
            niceStep = 10.0f;

        niceStep *= magnitude;

        // 计算美观的最小值和最大值
        float niceMin = std::floor(min / niceStep) * niceStep;
        float niceMax = std::ceil(max / niceStep) * niceStep;

        return {niceMin, niceMax};
    }

    // 动态范围管理类
    class DynamicRange {
    public:
        // 范围参数配置结构体
        struct DynamicRangeConfig {
            // 不再使用默认成员初始化，改为在构造函数中初始化
            float expandThreshold; // 数据超出范围多少比例后扩展
            float shrinkThreshold; // 数据使用范围少于多少比例时收缩
            float expandBufferRatio; // 扩展缓冲区比例
            float shrinkBufferRatio; // 收缩缓冲区比例
            float minRangeChangeRatio; // 最小范围变化比例，小于此比例不更新
            int tickCount; // 目标刻度数
            int stabilityCheckSize; // 稳定性检测样本数
            float stabilityThreshold; // 稳定性阈值
            int stableCounterThreshold; // 稳定计数器阈值

            // 构造函数设置默认值
            DynamicRangeConfig()
                : expandThreshold(0.2f),
                  shrinkThreshold(0.3f),
                  expandBufferRatio(0.10f),
                  shrinkBufferRatio(0.1f),
                  minRangeChangeRatio(0.08f),
                  tickCount(5),
                  stabilityCheckSize(30),
                  stabilityThreshold(0.2f),
                  stableCounterThreshold(5) {
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

            // 首次数据初始化范围
            if (!m_rangeInitialized) {
                m_dataMin = newMin;
                m_dataMax = newMax;

                // 计算初始显示范围（带缓冲）
                float dataRange = newMax - newMin;
                float buffer = dataRange * 0.1f;

                auto [initialMin, initialMax] =
                        calculateNiceRange(newMin - buffer, newMax + buffer, m_config.tickCount);

                m_displayMin = initialMin;
                m_displayMax = initialMax;

                m_rangeInitialized = true;
                // qDebug() << "范围初始化:" << m_displayMin << "到" << m_displayMax;
                return true;
            }

            // 更新实际数据范围
            m_dataMin = std::min(m_dataMin, newMin);
            m_dataMax = std::max(m_dataMax, newMax);

            // 跟踪最近范围以检测稳定性
            m_recentRanges.push_back({newMin, newMax});
            if (m_recentRanges.size() > m_config.stabilityCheckSize) {
                m_recentRanges.pop_front();
            }

            // 减少更新频率
            if (++m_updateCounter % 10 != 0) {
                return false;
            }
            m_updateCounter = 0;

            // 检查数据稳定性
            bool forceUpdate = checkDataStability();

            // 周期性范围检查
            if (++m_resetCounter % 50 == 0 || forceUpdate) {
                m_resetCounter = 0;

                // 确保数据范围有效
                if (m_dataMin > m_dataMax) {
                    std::swap(m_dataMin, m_dataMax);
                }

                // 计算新的显示范围
                auto [newDisplayMin, newDisplayMax] = calculateDisplayRange(forceUpdate);

                // 检查显示范围是否发生显著变化
                bool rangeChanged =
                        isRangeChangedSignificantly(oldDisplayMin, oldDisplayMax, newDisplayMin, newDisplayMax,
                                                    forceUpdate);

                if (rangeChanged) {
                    // qDebug() << "显示范围更新:"
                    //         << "旧范围:" << m_displayMin << "到" << m_displayMax << "新范围:" << newDisplayMin << "到"
                    //         << newDisplayMax << (forceUpdate ? " (强制更新)" : "");

                    m_displayMin = newDisplayMin;
                    m_displayMax = newDisplayMax;
                    return true;
                }
                return false;
            }

            // 检查数据是否超出当前显示范围
            bool dataOutOfRange = newMin < m_displayMin || newMax > m_displayMax;

            if (dataOutOfRange) {
                // 确保数据范围有效
                if (m_dataMin > m_dataMax) {
                    std::swap(m_dataMin, m_dataMax);
                }

                // 计算新的显示范围
                auto [newDisplayMin, newDisplayMax] = calculateDisplayRange(false);

                // 检查显示范围是否发生变化
                bool rangeChanged =
                        isRangeChangedSignificantly(oldDisplayMin, oldDisplayMax, newDisplayMin, newDisplayMax, false);

                if (rangeChanged) {
                    // qDebug() << "显示范围更新(数据超出):"
                    //         << "旧范围:" << m_displayMin << "到" << m_displayMax << "新范围:" << newDisplayMin << "到"
                    //         << newDisplayMax;

                    m_displayMin = newDisplayMin;
                    m_displayMax = newDisplayMax;
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

    private:
        // 检查数据稳定性，返回是否应该强制更新范围
        bool checkDataStability() {
            if (m_recentRanges.size() < m_config.stabilityCheckSize) {
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
            if (fluctuation < m_config.stabilityThreshold) {
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
            if (m_dataStable && m_stableCounter >= m_config.stableCounterThreshold) {
                // 使用稳定数据范围强制更新显示范围
                m_dataMin = m_stableMin;
                m_dataMax = m_stableMax;
                m_stableCounter = 0; // 重置计数器，避免频繁更新

                // qDebug() << "检测到数据稳定，强制更新量程:" << m_stableMin << "到" << m_stableMax;
                return true;
            }

            return false;
        }

        // 计算应该使用的显示范围
        std::pair<float, float> calculateDisplayRange(bool forceUpdate) {
            if (forceUpdate || !m_rangeInitialized) {
                // 计算数据范围
                float dataRange = m_dataMax - m_dataMin;

                // 特殊情况：数据范围太小
                if (dataRange < 1e-6f) {
                    dataRange = 1.0f;
                    m_dataMax = m_dataMin + 1.0f;
                }

                // 使用查找表方式确定缓冲区大小
                float bufferMin, bufferMax;

                // 基于数据范围大小和特性确定缓冲区
                if (m_dataMin >= 0 && m_dataMax >= 0) {
                    // 全正数据
                    if (dataRange < 10.0f) {
                        bufferMin = 1.0f;
                        bufferMax = dataRange * 0.1f + 1.0f;
                    } else if (dataRange < 100.0f) {
                        bufferMin = std::min(5.0f, dataRange * 0.05f);
                        bufferMax = dataRange * 0.08f + 2.0f;
                    } else if (dataRange < 1000.0f) {
                        bufferMin = std::min(10.0f, dataRange * 0.03f);
                        bufferMax = dataRange * 0.05f + 5.0f;
                    } else {
                        bufferMin = std::min(50.0f, dataRange * 0.02f);
                        bufferMax = dataRange * 0.03f + 10.0f;
                    }
                    // 如果最小值接近0，直接从0开始
                    if (m_dataMin < bufferMin * 2) {
                        bufferMin = m_dataMin;
                    }
                } else if (m_dataMin < 0 && m_dataMax < 0) {
                    // 全负数据
                    if (std::abs(dataRange) < 10.0f) {
                        bufferMax = 1.0f;
                        bufferMin = dataRange * 0.1f + 1.0f;
                    } else if (std::abs(dataRange) < 100.0f) {
                        bufferMax = std::min(5.0f, dataRange * 0.05f);
                        bufferMin = dataRange * 0.08f + 2.0f;
                    } else if (std::abs(dataRange) < 1000.0f) {
                        bufferMax = std::min(10.0f, dataRange * 0.03f);
                        bufferMin = dataRange * 0.05f + 5.0f;
                    } else {
                        bufferMax = std::min(50.0f, dataRange * 0.02f);
                        bufferMin = dataRange * 0.03f + 10.0f;
                    }
                    // 如果最大值接近0，直接到0结束
                    if (std::abs(m_dataMax) < bufferMax * 2) {
                        bufferMax = std::abs(m_dataMax);
                    }
                } else {
                    // 跨零数据
                    float absMin = std::abs(m_dataMin);
                    float absMax = std::abs(m_dataMax);
                    float maxAbs = std::max(absMin, absMax);

                    if (maxAbs < 10.0f) {
                        // 小范围数据，使用对称范围
                        float buffer = maxAbs * 0.1f + 0.5f;
                        bufferMin = buffer;
                        bufferMax = buffer;
                    } else if (maxAbs < 100.0f) {
                        float buffer = maxAbs * 0.08f + 2.0f;
                        bufferMin = buffer;
                        bufferMax = buffer;
                    } else if (maxAbs < 1000.0f) {
                        float buffer = maxAbs * 0.05f + 5.0f;
                        bufferMin = buffer;
                        bufferMax = buffer;
                    } else {
                        float buffer = maxAbs * 0.03f + 10.0f;
                        bufferMin = buffer;
                        bufferMax = buffer;
                    }

                    // 如果正负值明显不对称，调整缓冲区比例
                    if (absMin / absMax < 0.3f || absMax / absMin < 0.3f) {
                        if (absMin < absMax) {
                            bufferMin = std::min(bufferMin, absMin * 0.5f);
                        } else {
                            bufferMax = std::min(bufferMax, absMax * 0.5f);
                        }
                    }
                }

                // 应用缓冲区
                float minWithBuffer = m_dataMin - bufferMin;
                float maxWithBuffer = m_dataMax + bufferMax;

                // 计算美观范围
                auto [niceMin, niceMax] = calculateNiceRange(minWithBuffer, maxWithBuffer, m_config.tickCount);

                // 进一步优化美观范围 - 修剪过大的空白区域
                if (niceMin < m_dataMin - dataRange * 0.5f && m_dataMin > 0) {
                    // 对于正数据，避免在0附近有太多空白
                    niceMin = 0.0f;
                }

                if (niceMax > m_dataMax + dataRange * 0.5f && m_dataMax < 0) {
                    // 对于负数据，避免在0附近有太多空白
                    niceMax = 0.0f;
                }

                return {niceMin, niceMax};
            }

            // 非强制更新情况下的逻辑
            float currentRange = m_displayMax - m_displayMin;

            // 检查数据是否超出当前显示范围
            bool dataOutOfRange = m_dataMin < m_displayMin || m_dataMax > m_displayMax;

            if (dataOutOfRange) {
                // 数据超出显示范围，计算新的范围
                float expandedMin = std::min(m_displayMin, m_dataMin);
                float expandedMax = std::max(m_displayMax, m_dataMax);

                // 仅为新超出部分添加缓冲区
                float bufferMin = 0.0f, bufferMax = 0.0f;

                if (m_dataMin < m_displayMin) {
                    // 下限需要扩展
                    float overshoot = m_displayMin - m_dataMin;
                    bufferMin = std::min(overshoot * 0.2f, 50.0f);
                }

                if (m_dataMax > m_displayMax) {
                    // 上限需要扩展
                    float overshoot = m_dataMax - m_displayMax;
                    bufferMax = std::min(overshoot * 0.2f, 50.0f);
                }

                // 计算美观范围
                return calculateNiceRange(expandedMin - bufferMin, expandedMax + bufferMax, m_config.tickCount);
            } else {
                // 检测是否应该收缩范围
                float dataRange = m_dataMax - m_dataMin;
                float usageRatio = dataRange / currentRange;

                if (usageRatio < (1.0f - m_config.shrinkThreshold)) {
                    // 数据只使用了显示范围的一小部分，应该收缩
                    float targetMin, targetMax;

                    if (m_dataMin >= 0 && m_dataMax >= 0) {
                        // 全正数据
                        targetMin = (m_dataMin < dataRange * 0.1f) ? 0.0f : m_dataMin - dataRange * 0.1f;
                        targetMax = m_dataMax + dataRange * 0.15f;
                    } else if (m_dataMin < 0 && m_dataMax < 0) {
                        // 全负数据
                        targetMin = m_dataMin - dataRange * 0.15f;
                        targetMax = (m_dataMax > -dataRange * 0.1f) ? 0.0f : m_dataMax + dataRange * 0.1f;
                    } else {
                        // 跨零数据
                        float absMin = std::abs(m_dataMin);
                        float absMax = std::abs(m_dataMax);

                        if (absMin < absMax * 0.2f) {
                            // 负值较小，给正值更多空间
                            targetMin = m_dataMin - dataRange * 0.08f;
                            targetMax = m_dataMax + dataRange * 0.15f;
                        } else if (absMax < absMin * 0.2f) {
                            // 正值较小，给负值更多空间
                            targetMin = m_dataMin - dataRange * 0.15f;
                            targetMax = m_dataMax + dataRange * 0.08f;
                        } else {
                            // 正负值相当，均匀分配空间
                            targetMin = m_dataMin - dataRange * 0.12f;
                            targetMax = m_dataMax + dataRange * 0.12f;
                        }
                    }

                    // 平滑过渡，但当使用率低时收缩更快
                    float shrinkStep = 0.03f * (1.0f - usageRatio) * 2.0f;
                    shrinkStep = std::min(shrinkStep, 0.2f); // 限制最大收缩步长

                    float newMin = m_displayMin + (targetMin - m_displayMin) * shrinkStep;
                    float newMax = m_displayMax + (targetMax - m_displayMax) * shrinkStep;

                    // 计算美观范围
                    return calculateNiceRange(newMin, newMax, m_config.tickCount);
                }
            }

            // 默认返回当前范围
            return {m_displayMin, m_displayMax};
        }

        // 检查范围是否发生显著变化
        bool isRangeChangedSignificantly(float oldMin, float oldMax, float newMin, float newMax, bool forceUpdate) {
            if (forceUpdate)
                return true;

            const float epsilon = 0.0001f; // 浮点数比较的容差

            // 检查最小值和最大值是否有明显变化
            bool minChanged = std::abs(newMin - oldMin) > epsilon;
            bool maxChanged = std::abs(newMax - oldMax) > epsilon;

            if (!minChanged && !maxChanged)
                return false;

            // 计算变化的比例
            float oldRange = oldMax - oldMin;
            float minChangePct = std::abs(newMin - oldMin) / oldRange;
            float maxChangePct = std::abs(newMax - oldMax) / oldRange;

            // 只有当变化比例超过阈值时才认为是显著变化
            return (minChangePct > m_config.minRangeChangeRatio || maxChangePct > m_config.minRangeChangeRatio);
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
    };
} // namespace ProGraphics
