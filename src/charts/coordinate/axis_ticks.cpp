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

    // 确定小数位数
    int decimalPlaces = 0;
    float step = range.step;
    if (step < 1.0f) {
        // 计算步长需要的小数位数
        decimalPlaces = std::max(1, static_cast<int>(-std::floor(std::log10(step))) + 1);
    }

    for (int i = 0; i <= tickCount; ++i) {
        float value = range.min + i * range.step;
        QVector3D position = calculateTickPosition(axis, value, config.offset);
        
        // 使用自定义格式化函数或根据步长自动确定小数位数
        QString text;
        if (config.formatter) {
            text = config.formatter(value);
        } else {
            // 根据步长自动确定小数位数
            text = QString::number(value, 'f', decimalPlaces);
            // 移除尾随的0
            while (text.contains('.') && text.endsWith('0')) {
                text.chop(1);
            }
            if (text.endsWith('.')) {
                text.chop(1);
            }
        }

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
