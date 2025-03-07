#include "prographics/charts/prps/prps.h"
#include <random>
#include "prographics/utils/utils.h"

namespace ProGraphics {
  PRPSChart::PRPSChart(QWidget *parent) : Coordinate3D(parent) {
    // 设置动画定时器
    connect(&m_prpsAnimationTimer, &QTimer::timeout, this,
            &PRPSChart::updatePRPSAnimation);
    m_prpsAnimationTimer.setInterval(16); // 约60fps
    m_prpsAnimationTimer.start();
    setSize(PRPSConstants::GL_AXIS_LENGTH);

    // 设置各轴的名称和单位
    setAxisName('x', "相位", "°");
    setAxisName('y', "幅值", "dBm");
    setAxisEnabled(false);

    // 设置各轴的刻度范围
    setTicksRange('x', PRPSConstants::PHASE_MIN, PRPSConstants::PHASE_MAX, 85);
    setTicksRange('y', m_amplitudeMin, m_amplitudeMax, 8);
    setAxisVisible('z', false);
  }

  PRPSChart::~PRPSChart() {
    makeCurrent();
    m_lineGroups.clear();
    doneCurrent();
  }

  void PRPSChart::initializeGLObjects() { Coordinate3D::initializeGLObjects(); }

  void PRPSChart::paintGLObjects() {
    Coordinate3D::paintGLObjects();
    if (m_lineGroups.empty()) {
      return;
    }

    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2.0f);
    for (const auto &group: m_lineGroups) {
      if (group->instancedLine && group->isActive) {
        // 更新Z位置的模型矩阵
        QMatrix4x4 model;
        model.translate(0, 0, group->zPosition);

        // 使用实例化渲染绘制线段
        group->instancedLine->drawInstanced(camera().getProjectionMatrix(),
                                            camera().getViewMatrix() *
                                            model, // 组合视图和模型矩阵
                                            group->transforms);
      }
    }

    glLineWidth(1.0f);
    glDisable(GL_BLEND);
    glDisable(GL_LINE_SMOOTH);
  }

  void PRPSChart::addCycleData(const std::vector<float> &cycleData) {
    if (cycleData.size() != PRPSConstants::PHASE_POINTS) {
      qWarning() << "Invalid cycle data size:" << cycleData.size()
          << "expected:" << PRPSConstants::PHASE_POINTS;
      return;
    }

    if (m_lineGroups.size() >= PRPSConstants::MAX_LINE_GROUPS) {
      // 当达到最大数量时，移除最老的一组数据
      m_lineGroups.erase(m_lineGroups.begin());
    }

    updateAmplitudeRange(cycleData);
    m_currentCycles.push_back(cycleData);
    processCurrentCycles();
    m_currentCycles.clear();
  }

  void PRPSChart::processCurrentCycles() {
    makeCurrent();

    auto newGroup = std::make_unique<LineGroup>();
    // 直接使用当前周期的数据
    const auto &cycleData = m_currentCycles.front();
    newGroup->amplitudes = cycleData;

    int validCount = 0;
    for (int i = 0; i < PRPSConstants::PHASE_POINTS; ++i) {
      float glY = mapAmplitudeToGL(cycleData[i]);
      if (glY > 0.0f)
        validCount++;
    }
    // if (validCount < PRPSConstants::PHASE_POINTS * 0.1f) {
    //     // 少于10%的点有效
    //     doneCurrent();
    //     return;
    // }
    // 创建基础线段(从底部到顶部的竖线)
    newGroup->instancedLine = std::make_unique<Line2D>(
      QVector3D(0.0f, 0.0f, 0.0f), // 起点在底部
      QVector3D(0.0f, 1.0f, 0.0f), // 终点在顶部
      QVector4D(1.0f, 1.0f, 1.0f, 1.0f) // 白色，实际颜色将在实例中设置
    );

    newGroup->transforms.reserve(validCount);

    for (int i = 0; i < PRPSConstants::PHASE_POINTS; ++i) {
      float phase = (float) i / (PRPSConstants::PHASE_POINTS - 1) * m_phaseMax;
      float glX = mapPhaseToGL(phase);
      float glY = mapAmplitudeToGL(cycleData[i]);
      if (glY <= 0.0f)
        continue;

      // 创建变换
      Transform2D transform;
      transform.position = QVector2D(glX, 0.0f); // 设置X位置
      transform.scale = QVector2D(1.0f, glY); // Y缩放决定线段高度

      // 使用幅值来决定颜色强度
      float intensity = glY / PRPSConstants::GL_AXIS_LENGTH;
      transform.color = calculateColor(intensity);

      newGroup->transforms.push_back(transform);
    }

    // 初始化实例化渲染
    newGroup->instancedLine->initialize();

    m_lineGroups.push_back(std::move(newGroup));
    doneCurrent();
  }

  void PRPSChart::updatePRPSAnimation() {
    bool needCleanup = false;

    // 更新所有线组的位置和状态
    for (auto &group: m_lineGroups) {
      group->zPosition -= m_prpsAnimationSpeed;

      // 更新透明度
      if (group->zPosition < 2.0f) {
        float alpha = group->zPosition / 2.0f;
        for (auto &transform: group->transforms) {
          transform.color.setW(alpha);
        }
      }

      // 标记需要清理的线组
      if (group->zPosition <= PRPSConstants::MIN_Z_POSITION) {
        group->isActive = false;
        needCleanup = true;
      }
    }

    // 如果有需要清理的线组，立即进行清理
    if (needCleanup) {
      cleanupInactiveGroups();
    }

    update();
  }

  void PRPSChart::cleanupInactiveGroups() {
    makeCurrent();

    // 使用移除-擦除习语删除不活跃的线组
    auto it = std::remove_if(
      m_lineGroups.begin(), m_lineGroups.end(),
      [](const std::unique_ptr<LineGroup> &group) { return !group->isActive; });

    // 实际删除不活跃的线组
    if (it != m_lineGroups.end()) {
      m_lineGroups.erase(it, m_lineGroups.end());
    }

    doneCurrent();
  }


  void PRPSChart::setAmplitudeRange(float min, float max) {
    m_amplitudeMin = min;
    m_amplitudeMax = max;
    update();
  }

  void PRPSChart::setPhaseRange(float min, float max) {
    m_phaseMin = min;
    m_phaseMax = max;
    update();
  }

  float PRPSChart::mapPhaseToGL(float phase) const {
    return (phase - m_phaseMin) / (m_phaseMax - m_phaseMin) *
           PRPSConstants::GL_AXIS_LENGTH;
  }

  float PRPSChart::mapAmplitudeToGL(float amplitude) const {
    return (amplitude - m_amplitudeMin) / (m_amplitudeMax - m_amplitudeMin) *
           PRPSConstants::GL_AXIS_LENGTH;
  }

  float PRPSChart::mapGLToPhase(float glX) const {
    return glX / PRPSConstants::GL_AXIS_LENGTH * (m_phaseMax - m_phaseMin) +
           m_phaseMin;
  }

  float PRPSChart::mapGLToAmplitude(float glY) const {
    return glY / PRPSConstants::GL_AXIS_LENGTH *
           (m_amplitudeMax - m_amplitudeMin) +
           m_amplitudeMin;
  }

  void PRPSChart::updateAmplitudeRange(const std::vector<float> &newData) {
    if (newData.empty())
      return;

    // 找出新数据的范围
    auto [minIt, maxIt] = std::minmax_element(newData.begin(), newData.end());
    float newMin = *minIt;
    float newMax = *maxIt;

    // 更新实际渲染范围（不带余量）
    m_amplitudeMin = std::min(m_amplitudeMin, newMin);
    m_amplitudeMax = std::max(m_amplitudeMax, newMax);

    // 计算目标显示范围（带余量）
    float targetMin =
        std::floor(m_amplitudeMin - std::abs(m_amplitudeMin * 0.1f));
    float targetMax = std::ceil(m_amplitudeMax + std::abs(m_amplitudeMax * 0.1f));

    // 平滑过渡到目标范围
    bool rangeChanged = false;
    if (std::abs(m_displayMin - targetMin) > m_rangeUpdateThreshold) {
      m_displayMin += (targetMin - m_displayMin) * m_rangeTransitionSpeed;
      rangeChanged = true;
    }
    if (std::abs(m_displayMax - targetMax) > m_rangeUpdateThreshold) {
      m_displayMax += (targetMax - m_displayMax) * m_rangeTransitionSpeed;
      rangeChanged = true;
    }

    // 范围收缩（当数据范围缩小时，缓慢收缩显示范围）
    if (m_amplitudeMin > m_displayMin + m_rangeUpdateThreshold) {
      m_displayMin += m_rangeTransitionSpeed * 5.0f; // 稍快的收缩速度
      rangeChanged = true;
    }
    if (m_amplitudeMax < m_displayMax - m_rangeUpdateThreshold) {
      m_displayMax -= m_rangeTransitionSpeed * 5.0f;
      rangeChanged = true;
    }

    if (rangeChanged) {
      // 计算合适的刻度间隔
      float range = m_displayMax - m_displayMin;
      float step = calculateTickStep(range) * 2;

      // 更新坐标轴刻度
      setTicksRange('y', m_displayMin, m_displayMax, step);

      // 重新计算所有现有的线组
      recalculateLineGroups();
    }
  }

  void PRPSChart::recalculateLineGroups() {
    makeCurrent();
    for (auto &group: m_lineGroups) {
      group->transforms.clear();

      // 预计算有效数据数量
      int validCount = 0;
      for (float amplitude: group->amplitudes) {
        float glY = mapAmplitudeToGL(amplitude);
        if (glY > 0.0f)
          validCount++;
      }
      group->transforms.reserve(validCount);

      for (int i = 0; i < PRPSConstants::PHASE_POINTS; ++i) {
        float phase = (float) i / (PRPSConstants::PHASE_POINTS - 1) * m_phaseMax;
        float glX = mapPhaseToGL(phase);
        float glY = mapAmplitudeToGL(group->amplitudes[i]);

        if (glY <= 0.0f)
          continue;

        Transform2D transform;
        transform.position = QVector2D(glX, 0.0f);
        transform.scale = QVector2D(1.0f, glY);
        transform.color = calculateColor(glY / PRPSConstants::GL_AXIS_LENGTH);

        group->transforms.push_back(transform);
      }
    }
    doneCurrent();
    update();
  }
} // namespace ProGraphics
