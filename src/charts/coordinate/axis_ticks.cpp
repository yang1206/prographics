//
// Created by Yang1206 on 2025/2/16.
//

#include "prographics/charts/coordinate/axis_ticks.h"
#include <algorithm>
#include <cmath>

namespace ProGraphics {
  AxisTicks::TickConfig::Range::Range() : min(0.0f), max(5.0f), step(2.0f) {
  }

  AxisTicks::TickConfig::Range::Range(float min, float max, float step)
    : min(min), max(max), step(step) {
  }


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
    if (!std::isfinite(range.min) || !std::isfinite(range.max) ||
        !std::isfinite(range.step) || range.step <= 0 || range.max <= range.min) return;
    const double count = (static_cast<double>(range.max) - range.min) / range.step;
    // 避免错误步长造成无界标签分配；整数索引生成刻度，避免逐次累加漂移。
    const int tickCount = static_cast<int>(std::min(10000.0, std::floor(count + 1e-6)));

    // 确定小数位数
    int decimalPlaces = 1;
    float step = range.step;
    if (step < 1.0f) {
      // 计算步长需要的小数位数
      decimalPlaces = std::max(1, static_cast<int>(-std::floor(std::log10(step))) + 1);
    }

    for (int i = 0; i <= tickCount; ++i) {
      float value = std::min(range.max,
          static_cast<float>(static_cast<double>(range.min) + i * static_cast<double>(range.step)));
      QVector3D position = calculateTickPosition(axis, value, config.offset);

      // 使用自定义格式化函数或根据步长自动确定小数位数
      QString text;
      if (config.formatter) {
        text = config.formatter(value);
      } else {
        // 根据步长自动确定小数位数
        text = QString::number(value, 'f', decimalPlaces);
        // 移除尾随的0
        while (text.contains('.') && text.endsWith('0') && text.indexOf('.') < text.size() - 2) {
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

    float normalizedValue = static_cast<float>((static_cast<double>(value) - range.min) /
        (static_cast<double>(range.max) - range.min));
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
