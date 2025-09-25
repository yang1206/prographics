#include "prographics/charts/prpd/prpd.h"

#include "prographics/charts/prps/prps.h"
#include "prographics/utils/utils.h"

namespace ProGraphics {
    PRPDChart::PRPDChart(QWidget *parent) : Coordinate2D(parent) {
        // 设置坐标轴名称和单位
        setAxisName('x', "Phase", "°");
        setAxisName('y', "", "dBm");

        // 初始化动态量程
        auto [initialMin, initialMax] = m_dynamicRange.getDisplayRange();
        float step = calculateNiceTickStep(initialMax - initialMin);
        // 设置初始坐标轴范围
        setTicksRange('x', PRPSConstants::PHASE_MIN, PRPSConstants::PHASE_MAX, 90);
        setTicksRange('y', initialMin, initialMax, step);

        setAxisVisible('z', false);

        // 设置网格和坐标轴可见性
        setGridVisible(true);
        setAxisVisible('x', false);
        setAxisVisible('y', false);

        // 初始化频次表为全0
        for (auto &phaseRow: m_frequencyTable) {
            std::fill(phaseRow.begin(), phaseRow.end(), 0);
        }
    }

    PRPDChart::~PRPDChart() {
        makeCurrent();
        m_pointRenderer.reset();
        doneCurrent();
    }

    void PRPDChart::initializeGLObjects() {
        Coordinate2D::initializeGLObjects();

        // 初始化点渲染器
        m_pointRenderer = std::make_unique<Point2D>();

        // 设置点的样式
        Primitive2DStyle pointStyle;
        pointStyle.pointSize = PRPDConstants::POINT_SIZE;
        m_pointRenderer->setStyle(pointStyle);
        m_pointRenderer->initialize();

        // 预分配缓冲区空间
        m_cycleBuffer.data.reserve(PRPDConstants::MAX_CYCLES);
        m_cycleBuffer.binIndices.reserve(PRPDConstants::MAX_CYCLES);
        m_renderBatches.reserve(100); // 预估的批次数量
    }

    void PRPDChart::paintGLObjects() {
        // 先绘制坐标系
        Coordinate2D::paintGLObjects();

        if (!m_pointRenderer || m_renderBatches.empty()) {
            return;
        }

        // 绘制放电点
        for (const auto &batch: m_renderBatches) {
            if (batch.transforms.empty())
                continue;

            // 设置该批次的颜色
            QVector4D color = calculateColor(batch.frequency);
            m_pointRenderer->setColor(color);

            // 绘制该批次的所有点
            m_pointRenderer->drawInstanced(camera().getProjectionMatrix(), camera().getViewMatrix(), batch.transforms);
        }
        // 恢复OpenGL状态
        glPopAttrib();
    }

    void PRPDChart::addCycleData(const std::vector<float> &cycleData) {
        if (cycleData.size() != m_phasePoints) {
            qWarning() << "Invalid cycle data size:" << cycleData.size() << "expected:" << m_phasePoints;
            return;
        }


        // 更新动态量程
        bool rangeChanged = false;
        if (m_dynamicRangeEnabled) {
            rangeChanged = m_dynamicRange.updateRange(cycleData);
        }

        // 如果范围改变，更新坐标轴并重建频次表
        if (rangeChanged) {
            auto [newDisplayMin, newDisplayMax] = m_dynamicRange.getDisplayRange();
            // 更新坐标轴刻度
            float step = calculateNiceTickStep(newDisplayMax - newDisplayMin);
            setTicksRange('y', newDisplayMin, newDisplayMax, step);
            // qDebug() << "PRPD - 量程更新:" << newDisplayMin << "-" << newDisplayMax;

            // 立即重建频次表和所有点位置
            rebuildFrequencyTable();
            return;
        }

        // 计算并存储当前显示范围下的bin索引
        std::vector<BinIndex> currentBinIndices(m_phasePoints);
        for (int i = 0; i < m_phasePoints; ++i) {
            currentBinIndices[i] = getAmplitudeBinIndex(cycleData[i]);
        }

        // 如果缓冲区已满，先移除最老的数据的频次
        if (m_cycleBuffer.data.size() == PRPDConstants::MAX_CYCLES) {
            const auto &oldestBinIndices = m_cycleBuffer.binIndices[m_cycleBuffer.currentIndex];

            // 使用存储的bin索引直接更新频次表
            for (int phaseIdx = 0; phaseIdx < m_phasePoints; ++phaseIdx) {
                BinIndex binIdx = oldestBinIndices[phaseIdx];
                if (binIdx < PRPDConstants::AMPLITUDE_BINS) {
                    int &freq = m_frequencyTable[phaseIdx][binIdx];
                    if (freq > 0) {
                        freq--;
                        // 如果频次变为0，标记需要更新最大频次
                        if (freq == 0 || freq + 1 == m_maxFrequency) {
                            // 在下一步更新最大频次
                        }
                    }
                }
            }
        }

        // 添加新数据到环形缓冲区
        if (m_cycleBuffer.data.size() < PRPDConstants::MAX_CYCLES) {
            m_cycleBuffer.data.push_back(cycleData);
            m_cycleBuffer.binIndices.push_back(currentBinIndices);
        } else {
            m_cycleBuffer.data[m_cycleBuffer.currentIndex] = cycleData;
            m_cycleBuffer.binIndices[m_cycleBuffer.currentIndex] = currentBinIndices;
            m_cycleBuffer.currentIndex = (m_cycleBuffer.currentIndex + 1) % PRPDConstants::MAX_CYCLES;
            m_cycleBuffer.isFull = true;
        }

        // 更新新数据的频次
        for (int phaseIdx = 0; phaseIdx < m_phasePoints; ++phaseIdx) {
            BinIndex binIdx = currentBinIndices[phaseIdx];
            if (binIdx < PRPDConstants::AMPLITUDE_BINS) {
                int &freq = m_frequencyTable[phaseIdx][binIdx];
                freq++;
                if (freq > m_maxFrequency) {
                    m_maxFrequency = freq;
                }
            }
        }

        // 定期更新最大频次（每10个周期完全重新计算一次，确保准确性）
        static int cycleCount = 0;
        cycleCount = (cycleCount + 1) % 10;
        if (cycleCount == 0) {
            m_maxFrequency = 0;
            for (const auto &phaseMap: m_frequencyTable) {
                for (int freq: phaseMap) {
                    m_maxFrequency = std::max(m_maxFrequency, freq);
                }
            }
        }

        // 更新渲染数据
        updatePointTransformsFromFrequencyTable();
        update();
    }

    void PRPDChart::clearFrequencyTable() {
        for (auto &phaseRow: m_frequencyTable) {
            std::fill(phaseRow.begin(), phaseRow.end(), 0);
        }
        m_maxFrequency = 0;
    }

    void PRPDChart::updatePointTransformsFromFrequencyTable() {
        // 按频次分组
        std::map<int, RenderBatch> batchMap; // 频次 -> 批次

        for (int phaseIdx = 0; phaseIdx < m_phasePoints; ++phaseIdx) {
            float phase = static_cast<float>(phaseIdx) * (PRPDConstants::PHASE_MAX / m_phasePoints);

            for (BinIndex binIdx = 0; binIdx < PRPDConstants::AMPLITUDE_BINS; ++binIdx) {
                int frequency = m_frequencyTable[phaseIdx][binIdx];
                if (frequency <= 0)
                    continue;

                float amplitude = getBinCenterAmplitude(binIdx);

                Transform2D transform;
                transform.position = QVector2D(mapPhaseToGL(phase), mapAmplitudeToGL(amplitude));
                transform.scale = QVector2D(1.0f, 1.0f);
                transform.color = calculateColor(frequency);

                auto &batch = batchMap[frequency];
                batch.frequency = frequency;
                batch.transforms.push_back(std::move(transform));
            }
        }

        // 转换为vector
        m_renderBatches.clear();
        m_renderBatches.reserve(batchMap.size());
        for (auto &[_, batch]: batchMap) {
            m_renderBatches.push_back(std::move(batch));
        }
    }

    QVector4D PRPDChart::calculateColor(int frequency) const {
        float intensity = static_cast<float>(frequency) / m_maxFrequency;

        // 从蓝色渐变到红色
        float hue = 240.0f - intensity * 240.0f;
        float saturation = 1.0f;
        float value = 0.8f + intensity * 0.2f;
        float alpha = 0.6f + intensity * 0.4f;

        return hsvToRgb(hue, saturation, value, alpha);
    }

    void PRPDChart::setAmplitudeRange(float min, float max) {
        // 设置固定范围
        m_dynamicRange.setDisplayRange(min, max);

        // 更新坐标轴
        float step = calculateNiceTickStep(max - min, m_dynamicRange.getConfig().targetTickCount);
        setTicksRange('y', min, max, step);

        // 重建频次表
        rebuildFrequencyTable();
        update();
    }

    void PRPDChart::setPhaseRange(float min, float max) {
        m_phaseMin = min;
        m_phaseMax = max;
        setTicksRange('x', min, max, 85); // 相位通常以45度为刻度

        // 不需要重建频次表，因为相位范围不影响bin索引
        update();
    }

    void PRPDChart::setDynamicRangeEnabled(bool enabled) { m_dynamicRangeEnabled = enabled; }

    bool PRPDChart::isDynamicRangeEnabled() const { return m_dynamicRangeEnabled; }

    void PRPDChart::setDynamicRangeConfig(const DynamicRange::DynamicRangeConfig &config) {
        m_dynamicRange.setConfig(config);
        // 更新坐标轴刻度
        auto [newMin, newMax] = m_dynamicRange.getDisplayRange();
        setTicksRange('y', newMin, newMax, calculateNiceTickStep(newMax - newMin, config.targetTickCount));

        // 重建频次表
        rebuildFrequencyTable();
        update();
    }

    float PRPDChart::mapPhaseToGL(float phase) const {
        return (phase - m_phaseMin) / (m_phaseMax - m_phaseMin) * PRPDConstants::GL_AXIS_LENGTH;
    }

    float PRPDChart::mapAmplitudeToGL(float amplitude) const {
        auto [displayMin, displayMax] = m_dynamicRange.getDisplayRange();

        // 处理超出范围的数据
        if (amplitude < displayMin) {
            return 0.0f; // 映射到坐标系底部
        }
        if (amplitude > displayMax) {
            return PRPDConstants::GL_AXIS_LENGTH; // 映射到坐标系顶部
        }

        return (amplitude - displayMin) / (displayMax - displayMin) * PRPDConstants::GL_AXIS_LENGTH;
    }

    void PRPDChart::rebuildFrequencyTable() {
        // qDebug() << "完全重建频次表和数据点位置 - 当前数据量:" << m_cycleBuffer.data.size() << "周期";

        // 清空频次表
        clearFrequencyTable();

        // 重新计算所有缓冲区数据在新范围下的bin索引
        for (size_t i = 0; i < m_cycleBuffer.data.size(); ++i) {
            const auto &cycle = m_cycleBuffer.data[i];

            // 在新范围下重新计算所有bin索引
            std::vector<BinIndex> newBinIndices(m_phasePoints);
            for (int j = 0; j < m_phasePoints; ++j) {
                newBinIndices[j] = getAmplitudeBinIndex(cycle[j]);
            }

            // 更新存储的bin索引
            m_cycleBuffer.binIndices[i] = newBinIndices;

            // 更新频次表
            for (int phaseIdx = 0; phaseIdx < m_phasePoints; ++phaseIdx) {
                BinIndex binIdx = newBinIndices[phaseIdx];
                if (binIdx < PRPDConstants::AMPLITUDE_BINS) {
                    int &freq = m_frequencyTable[phaseIdx][binIdx];
                    freq++;
                    if (freq > m_maxFrequency) {
                        m_maxFrequency = freq;
                    }
                }
            }
        }

        // 重新构建所有渲染批次
        updatePointTransformsFromFrequencyTable();

        // 强制立即重绘
        update();
    }

    // 将幅值映射到格子索引
    PRPDChart::BinIndex PRPDChart::getAmplitudeBinIndex(float amplitude) const {
        auto [displayMin, displayMax] = m_dynamicRange.getDisplayRange();

        // 处理超出范围的数据
        if (amplitude <= displayMin) {
            return 0; // 第一个bin
        }
        if (amplitude >= displayMax) {
            return PRPDConstants::AMPLITUDE_BINS - 1; // 最后一个bin
        }

        float range = displayMax - displayMin;
        if (range < 1e-6f) {
            return PRPDConstants::AMPLITUDE_BINS / 2;
        }

        float normalizedPos = (amplitude - displayMin) / range;
        normalizedPos = std::max(0.0f, std::min(0.9999f, normalizedPos));
        int binIndex = static_cast<int>(normalizedPos * PRPDConstants::AMPLITUDE_BINS);

        return std::clamp(binIndex, 0, PRPDConstants::AMPLITUDE_BINS - 1);
    }

    // 获取格子中心幅值
    float PRPDChart::getBinCenterAmplitude(BinIndex binIndex) const {
        auto [displayMin, displayMax] = m_dynamicRange.getDisplayRange();

        if (binIndex >= PRPDConstants::AMPLITUDE_BINS) {
            return displayMin;
        }

        float normalizedPos = (binIndex + 0.5f) / PRPDConstants::AMPLITUDE_BINS;
        return displayMin + normalizedPos * (displayMax - displayMin);
    }

    void PRPDChart::setPhasePoint(int phasePoint) {
        m_phasePoints = phasePoint;
    }
} // namespace ProGraphics
