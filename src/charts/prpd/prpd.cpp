#include "prographics/charts/prpd//prpd.h"
#include "prographics/utils/utils.h"

namespace ProGraphics {
  PRPDChart::PRPDChart(QWidget *parent) : Coordinate2D(parent) {
    // 设置坐标轴名称和单位
    setAxisName('x', "相位", "°");
    setAxisName('y', "幅值", "dBmV");

    // 设置坐标轴范围和刻度
    setTicksRange('x', PRPDConstants::PHASE_MIN, PRPDConstants::PHASE_MAX, 45);
    setTicksRange('y', m_amplitudeMin, m_amplitudeMax, 5);


    // 设置网格
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
      if (batch.transforms.empty()) continue;

      // 设置该批次的颜色
      QVector4D color = calculateColor(batch.frequency);
      m_pointRenderer->setColor(color);

      // 绘制该批次的所有点
      m_pointRenderer->drawInstanced(
        camera().getProjectionMatrix(),
        camera().getViewMatrix(),
        batch.transforms
      );
    }
    // 恢复OpenGL状态
    glPopAttrib();
  }

  void PRPDChart::addCycleData(const std::vector<float> &cycleData) {
    if (cycleData.size() != PRPDConstants::PHASE_POINTS) {
      qWarning() << "Invalid cycle data size:" << cycleData.size()
          << "expected:" << PRPDConstants::PHASE_POINTS;
      return;
    }
    updateAmplitudeRange(cycleData);

    // 计算并存储当前显示范围下的bin索引
    std::vector<BinIndex> currentBinIndices(PRPDConstants::PHASE_POINTS);
    for (int i = 0; i < PRPDConstants::PHASE_POINTS; ++i) {
      currentBinIndices[i] = getAmplitudeBinIndex(cycleData[i]);
    }

    // 如果缓冲区已满，先移除最老的数据的频次
    if (m_cycleBuffer.data.size() == PRPDConstants::MAX_CYCLES) {
      const auto &oldestBinIndices = m_cycleBuffer.binIndices[m_cycleBuffer.currentIndex];

      // 使用存储的bin索引直接更新频次表
      for (int phaseIdx = 0; phaseIdx < PRPDConstants::PHASE_POINTS; ++phaseIdx) {
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
    for (int phaseIdx = 0; phaseIdx < PRPDConstants::PHASE_POINTS; ++phaseIdx) {
      BinIndex binIdx = currentBinIndices[phaseIdx];
      if (binIdx < PRPDConstants::AMPLITUDE_BINS) {
        int &freq = m_frequencyTable[phaseIdx][binIdx];
        freq++;
        if (freq > m_maxFrequency) {
          m_maxFrequency = freq;
        }
      }
    }
    // 更新最大频次（每10个周期完全重新计算一次，确保准确性）
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

    for (int phaseIdx = 0; phaseIdx < PRPDConstants::PHASE_POINTS; ++phaseIdx) {
      float phase = static_cast<float>(phaseIdx) *
                    (PRPDConstants::PHASE_MAX / PRPDConstants::PHASE_POINTS);

      for (BinIndex binIdx = 0; binIdx < PRPDConstants::AMPLITUDE_BINS; ++binIdx) {
        int frequency = m_frequencyTable[phaseIdx][binIdx];
        if (frequency <= 0) continue;

        float amplitude = getBinCenterAmplitude(binIdx);

        Transform2D transform;
        transform.position = QVector2D(
          mapPhaseToGL(phase),
          mapAmplitudeToGL(amplitude)
        );
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
    m_amplitudeMin = min;
    m_amplitudeMax = max;
    setTicksRange('y', min, max, calculateTickStep(max - min));
    update();
  }

  void PRPDChart::setPhaseRange(float min, float max) {
    m_phaseMin = min;
    m_phaseMax = max;
    setTicksRange('x', min, max, 45); // 相位通常以45度为刻度
    update();
  }

  float PRPDChart::mapPhaseToGL(float phase) const {
    return (phase - m_phaseMin) / (m_phaseMax - m_phaseMin) *
           PRPDConstants::GL_AXIS_LENGTH;
  }

  float PRPDChart::mapAmplitudeToGL(float amplitude) const {
    return (amplitude - m_amplitudeMin) / (m_amplitudeMax - m_amplitudeMin) *
           PRPDConstants::GL_AXIS_LENGTH;
  }


  void PRPDChart::updateAmplitudeRange(const std::vector<float> &newData) {
    if (newData.empty())
      return;

    // 记录旧的显示范围
    float oldDisplayMin = m_displayMin;
    float oldDisplayMax = m_displayMax;

    // 找出新数据的范围
    auto [minIt, maxIt] = std::minmax_element(newData.begin(), newData.end());
    float newMin = *minIt;
    float newMax = *maxIt;

    // 更新实际渲染范围（不带余量）
    m_amplitudeMin = std::min(m_amplitudeMin, newMin);
    m_amplitudeMax = std::max(m_amplitudeMax, newMax);
    // 调整更新策略 - 每收集10个周期才考虑更新显示范围
    static int updateCounter = 0;
    updateCounter = (updateCounter + 1) % 10;
    if (updateCounter != 0) {
      return; // 跳过本次更新
    }

    // 计算目标显示范围（带余量）
    float targetMin = m_amplitudeMin;
    float targetMax = std::ceil(m_amplitudeMax + std::abs(m_amplitudeMax * 0.1f));

    // 平滑过渡参数优化
    const float fastTransition = 0.3f; // 快速过渡
    const float slowTransition = 0.1f; // 慢速过渡
    const float bigThreshold = 20.0f; // 大变化阈值
    const float smallThreshold = 5.0f; // 小变化阈值

    // 自适应过渡速度：变化大时快速过渡，变化小时慢速过渡
    bool rangeChanged = false;
    float minDiff = targetMin - m_displayMin;
    if (std::abs(minDiff) > bigThreshold) {
      m_displayMin += minDiff * fastTransition;
      rangeChanged = true;
    } else if (std::abs(minDiff) > smallThreshold) {
      m_displayMin += minDiff * slowTransition;
      rangeChanged = true;
    }

    float maxDiff = targetMax - m_displayMax;
    if (std::abs(maxDiff) > bigThreshold) {
      m_displayMax += maxDiff * fastTransition;
      rangeChanged = true;
    } else if (std::abs(maxDiff) > smallThreshold) {
      m_displayMax += maxDiff * slowTransition;
      rangeChanged = true;
    }

    // 范围收缩速度降低，只有当数据范围大幅缩小时才考虑收缩显示范围
    if (m_amplitudeMin > m_displayMin + bigThreshold) {
      m_displayMin += slowTransition * 2.0f;
      rangeChanged = true;
    }
    if (m_amplitudeMax < m_displayMax - bigThreshold) {
      m_displayMax -= slowTransition * 2.0f;
      rangeChanged = true;
    }


    // 检查显示范围是否发生了重大变化
    // 降低触发频次表重建的阈值，只有在变化非常大时才重建
    float oldRange = oldDisplayMax - oldDisplayMin;
    float newRange = m_displayMax - m_displayMin;
    float rangeRatio = newRange / oldRange;

    // 如果范围变化超过30%，或者最小值/最大值变化超过20%，则清空频次表并重建
    if (rangeRatio < 0.5f || rangeRatio > 1.5f ||
        std::abs(m_displayMin - oldDisplayMin) > 0.3f * oldRange ||
        std::abs(m_displayMax - oldDisplayMax) > 0.3f * oldRange) {
      qDebug() << "显示范围变化较大，重建频次表:"
          << "旧范围:" << oldDisplayMin << "到" << oldDisplayMax
          << "新范围:" << m_displayMin << "到" << m_displayMax;
      // 清空频次表
      clearFrequencyTable();

      // 重新添加所有缓冲区中的数据
      for (size_t i = 0; i < m_cycleBuffer.data.size(); ++i) {
        const auto &cycle = m_cycleBuffer.data[i];

        // 重新计算bin索引
        std::vector<BinIndex> newBinIndices(PRPDConstants::PHASE_POINTS);
        for (int j = 0; j < PRPDConstants::PHASE_POINTS; ++j) {
          newBinIndices[j] = getAmplitudeBinIndex(cycle[j]);
        }

        // 更新存储的bin索引
        m_cycleBuffer.binIndices[i] = newBinIndices;

        // 更新频次表
        for (int phaseIdx = 0; phaseIdx < PRPDConstants::PHASE_POINTS; ++phaseIdx) {
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
    }

    if (rangeChanged) {
      // 计算合适的刻度间隔
      float range = m_displayMax - m_displayMin;
      float step = calculateTickStep(range);

      // 更新坐标轴刻度
      setTicksRange('y', m_displayMin, m_displayMax, step);
    }
  }


  // 将幅值映射到格子索引
  PRPDChart::BinIndex PRPDChart::getAmplitudeBinIndex(float amplitude) const {
    // 处理极端情况 - 确保不会返回无效索引，而是最近的有效索引
    if (amplitude <= m_displayMin) {
      return 0;
    }
    if (amplitude >= m_displayMax) {
      return PRPDConstants::AMPLITUDE_BINS - 1;
    }

    float range = m_displayMax - m_displayMin;
    // 防止除以接近零的值
    if (range < 1e-6f) {
      return PRPDConstants::AMPLITUDE_BINS / 2;
    }

    float normalizedPos = (amplitude - m_displayMin) / range;
    // 添加一个小的epsilon值来处理浮点精度问题
    normalizedPos = std::max(0.0f, std::min(0.9999f, normalizedPos));
    int binIndex = static_cast<int>(normalizedPos * PRPDConstants::AMPLITUDE_BINS);

    // 最后一道保险，确保返回有效索引
    return std::clamp(binIndex, 0, PRPDConstants::AMPLITUDE_BINS - 1);
  }

  // 获取格子中心幅值
  float PRPDChart::getBinCenterAmplitude(BinIndex binIndex) const {
    if (binIndex >= PRPDConstants::AMPLITUDE_BINS) {
      return m_displayMin;
    }

    float normalizedPos = (binIndex + 0.5f) / PRPDConstants::AMPLITUDE_BINS;
    return m_displayMin + normalizedPos * (m_displayMax - m_displayMin);
  }
} // namespace ProGraphics
