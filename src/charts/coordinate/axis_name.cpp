﻿//
// Created by Yang1206 on 2025/2/16.
//
#include "prographics/charts/coordinate/axis_name.h"

namespace ProGraphics {
  AxisName::AxisName() { m_textRenderer = std::make_unique<TextRenderer>(); }

  AxisName::~AxisName() = default;

  void AxisName::initialize() {
    if (m_initialized)
      return;
    updateNames();
    m_initialized = true;
  }

  void AxisName::render(QPainter &painter, const QMatrix4x4 &view,
                        const QMatrix4x4 &projection, int width, int height) {
    if (!m_initialized)
      return;
    m_textRenderer->render(painter, view, projection, width, height);
  }

  void AxisName::setConfig(const Config &config) {
    m_config = config;
    updateNames();
  }

  void AxisName::updateNames() {
    m_textRenderer->clear();
    // 更新X轴名称
    if (m_config.x.visible) {
      QString text = m_config.x.text;
      if (!m_config.x.unit.isEmpty()) {
        text += " (" + m_config.x.unit + ")";
      }
      QVector3D pos = calculateNamePosition('x', m_config.x);
      m_xName = m_textRenderer->addLabel(text, pos, m_config.x.style);
    }

    // 更新Y轴名称
    if (m_config.y.visible) {
      QString text = m_config.y.text;
      if (!m_config.y.unit.isEmpty()) {
        text += " (" + m_config.y.unit + ")";
      }
      QVector3D pos = calculateNamePosition('y', m_config.y);
      m_yName = m_textRenderer->addLabel(text, pos, m_config.y.style);
    }

    // 更新Z轴名称
    if (m_config.z.visible) {
      QString text = m_config.z.text;
      if (!m_config.z.unit.isEmpty()) {
        text += " (" + m_config.z.unit + ")";
      }
      QVector3D pos = calculateNamePosition('z', m_config.z);
      m_zName = m_textRenderer->addLabel(text, pos, m_config.z.style);
    }
  }

  QVector3D AxisName::calculateNamePosition(char axis,
                                            const NameConfig &config) const {
    float size = m_config.size;
    QVector3D basePos;

    switch (axis) {
      case 'x':
        switch (config.location) {
          case Location::End:
            // 在X轴正方向末端，考虑间距
            basePos = QVector3D(size + config.gap, 0, m_config.size);
            break;
          case Location::Middle:
            basePos = QVector3D(size / 2, 0, m_config.size);
            break;
          case Location::Start:
            basePos = QVector3D(-config.gap, 0, m_config.size);
            break;
        }
        break;
      case 'y':
        switch (config.location) {
          case Location::End:
            basePos = QVector3D(0, size, 0);
            break;
          case Location::Middle:
            basePos = QVector3D(0, size / 2, 0);
            break;
          case Location::Start:
            basePos = QVector3D(0, 0, 0);
            break;
        }
        break;
      case 'z':
        switch (config.location) {
          case Location::End:
            basePos = QVector3D(0, 0, size);
            break;
          case Location::Middle:
            basePos = QVector3D(0, 0, size / 2);
            break;
          case Location::Start:
            basePos = QVector3D(0, 0, 0);
            break;
        }
        break;
    }

    // 应用偏移
    return basePos + config.offset;
  }
} // namespace ProGraphics
