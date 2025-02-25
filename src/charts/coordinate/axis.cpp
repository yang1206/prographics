#include "prographics/charts/coordinate/axis.h"

namespace ProGraphics {
Axis::Axis() {
  // 创建批处理渲染器
  m_batchRenderer = std::make_unique<Primitive2DBatch>();

  // 创建三个轴
  m_xAxis = std::make_shared<Line2D>();
  m_yAxis = std::make_shared<Line2D>();
  m_zAxis = std::make_shared<Line2D>();

  // 设置默认配置
  Config defaultConfig;
  setConfig(defaultConfig);
}

Axis::~Axis() { cleanup(); }

void Axis::initialize() {
  if (m_initialized)
    return;

  updateAxes();
  updateBatch();
  m_initialized = true;
}

void Axis::cleanup() {
  m_initialized = false;
  m_batchRenderer.reset();
  m_xAxis.reset();
  m_yAxis.reset();
  m_zAxis.reset();
}

void Axis::render(const QMatrix4x4 &projection, const QMatrix4x4 &view) {
  if (!m_initialized)
    return;

  if (m_batchDirty) {
    updateBatch();
  }

  m_batchRenderer->draw(projection, view);
}

void Axis::setConfig(const Config &config) {
  m_config = config;
  updateAxes();
  m_batchDirty = true;
}

void Axis::updateAxes() {
  if (!m_xAxis || !m_yAxis || !m_zAxis)
    return;

  // X轴
  {
    m_xAxis->setPoints(QVector3D(0, 0, 0), QVector3D(m_config.length, 0, 0));
    m_xAxis->setColor(m_config.xAxisColor);

    Primitive2DStyle style;
    style.lineWidth = m_config.xAxisThickness;
    m_xAxis->setStyle(style);
    m_xAxis->setVisible(m_config.xAxisVisible);
  }

  // Y轴
  {
    m_yAxis->setPoints(QVector3D(0, 0, 0), QVector3D(0, m_config.length, 0));
    m_yAxis->setColor(m_config.yAxisColor);

    Primitive2DStyle style;
    style.lineWidth = m_config.yAxisThickness;
    m_yAxis->setStyle(style);
    m_yAxis->setVisible(m_config.yAxisVisible);
  }

  // Z轴
  {
    m_zAxis->setPoints(QVector3D(0, 0, 0), QVector3D(0, 0, m_config.length));
    m_zAxis->setColor(m_config.zAxisColor);

    Primitive2DStyle style;
    style.lineWidth = m_config.zAxisThickness;
    m_zAxis->setStyle(style);
    m_zAxis->setVisible(m_config.zAxisVisible);
  }
}

void Axis::updateBatch() {
  m_batchRenderer->begin();

  if (m_xAxis->isVisible()) {
    m_xAxis->addToRenderBatch(*m_batchRenderer);
  }

  if (m_yAxis->isVisible()) {
    m_yAxis->addToRenderBatch(*m_batchRenderer);
  }

  if (m_zAxis->isVisible()) {
    m_zAxis->addToRenderBatch(*m_batchRenderer);
  }

  m_batchRenderer->end();
  m_batchDirty = false;
}
} // namespace ProGraphics
