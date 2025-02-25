//
// Created by Yang1206 on 2025/2/16.
//

#include "prographics/charts/coordinate/axis_ticks.h"

namespace ProGraphics {
  AxisTicks::AxisTicks() { m_textRenderer = std::make_unique<TextRenderer>(); }

  AxisTicks::~AxisTicks() = default;

  void AxisTicks::initialize() {
    if (m_initialized)
      return;
    updateTicks();
    m_initialized = true;
  }

  void AxisTicks::render(QPainter &painter, const QMatrix4x4 &view,
                         const QMatrix4x4 &projection, int width, int height) {
    if (!m_initialized)
      return;
    m_textRenderer->render(painter, view, projection, width, height);
  }

  void AxisTicks::setConfig(const Config &config) {
    m_config = config;
    updateTicks();
  }

  void AxisTicks::updateTicks() {
    m_textRenderer->clear();
    m_tickLabels.clear();

    updateAxisTicks('x', m_config.x);
    updateAxisTicks('y', m_config.y);
    updateAxisTicks('z', m_config.z);
  }

  void AxisTicks::updateAxisTicks(char axis, const TickConfig &config) {
    if (!config.visible)
      return;

    const auto &range = config.range;
    int tickCount = static_cast<int>((range.max - range.min) / range.step);

    for (int i = 0; i <= tickCount; ++i) {
      float value = range.min + i * range.step;
      QVector3D position = calculateTickPosition(axis, value, config.offset);
      QString text = config.formatter(value);

      auto *label = m_textRenderer->addLabel(text, position, config.style);
      m_textRenderer->setAlignment(label, config.alignment);
      m_tickLabels.push_back(label);
    }
  }

  QVector3D AxisTicks::calculateTickPosition(char axis, float value,
                                             QVector3D offset) const {
    const auto &config = (axis == 'x'
                            ? m_config.x
                            : axis == 'y'
                                ? m_config.y
                                : m_config.z);
    const auto &range = config.range;

    float normalizedValue = (value - range.min) / (range.max - range.min);
    float geometryValue = normalizedValue * m_config.size;
    QVector3D basePosition;
    switch (axis) {
      case 'x':
        basePosition = QVector3D(geometryValue, 0, m_config.size);
        break;
      case 'y':
        basePosition = QVector3D(0, geometryValue, 0);
        break;
      case 'z':
        basePosition = QVector3D(0, 0, geometryValue);
        break;
    }
    // 应用偏移
    return basePosition + offset;
  }
} // namespace ProGraphics
