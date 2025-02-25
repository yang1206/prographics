#include "prographics/charts/coordinate/grid.h"

namespace ProGraphics {
  Grid::Grid() {
    m_batchRenderer = std::make_unique<Primitive2DBatch>();

    // 设置默认配置
    Config defaultConfig;
    defaultConfig.xy.sineWave.visible = true;
    defaultConfig.xz.sineWave.visible = true;
    defaultConfig.yz.sineWave.visible = false;
    setConfig(defaultConfig);
  }

  Grid::~Grid() { cleanup(); }

  void Grid::initialize() {
    if (m_initialized)
      return;

    updateGrids();
    updateBatch();
    m_initialized = true;
  }

  void Grid::cleanup() {
    m_initialized = false;
    m_batchRenderer.reset();
    m_xyGridLines.clear();
    m_xzGridLines.clear();
    m_yzGridLines.clear();
  }

  void Grid::render(const QMatrix4x4 &projection, const QMatrix4x4 &view) {
    if (!m_initialized)
      return;

    if (m_batchDirty) {
      updateBatch();
    }

    m_batchRenderer->draw(projection, view);
  }

  void Grid::setConfig(const Config &config) {
    m_config = config;
    updateGrids();
    m_batchDirty = true;
  }

  void Grid::generateGridLines(std::vector<std::shared_ptr<Line2D> > &lines,
                               const QVector3D &planeNormal,
                               const PlaneConfig &planeConfig) {
    if (!planeConfig.visible) {
      lines.clear();
      return;
    }
    lines.clear();

    // 确定平面的两个方向向量
    QVector3D dir1, dir2;
    if (planeNormal == QVector3D(0, 0, 1)) {
      // XY平面
      dir1 = QVector3D(1, 0, 0);
      dir2 = QVector3D(0, 1, 0);
    } else if (planeNormal == QVector3D(0, 1, 0)) {
      // XZ平面
      dir1 = QVector3D(1, 0, 0);
      dir2 = QVector3D(0, 0, 1);
    } else {
      // YZ平面
      dir1 = QVector3D(0, 1, 0);
      dir2 = QVector3D(0, 0, 1);
    }

    // 只生成第一象限的网格
    float spacing = planeConfig.spacing;
    int lineCount = static_cast<int>(m_config.size / spacing);

    // 生成平行于dir1方向的线
    for (int i = 0; i <= lineCount; ++i) {
      auto line = std::make_shared<Line2D>();
      float pos = i * spacing;

      // 判断是否为主网格线
      bool isMajor = std::abs(std::fmod(pos, 1.0f)) < 0.001f;
      QVector4D color = isMajor ? planeConfig.majorColor : planeConfig.minorColor;

      QVector3D start = QVector3D(0, 0, 0) + dir1 * pos;
      QVector3D end = dir2 * m_config.size + dir1 * pos;

      line->setPoints(start, end);
      line->setColor(color);

      Primitive2DStyle style;
      style.lineWidth =
          isMajor ? planeConfig.thickness : planeConfig.thickness * 0.5f;
      line->setStyle(style);
      line->setVisible(true);

      lines.push_back(line);
    }

    // 生成平行于dir2方向的线
    for (int i = 0; i <= lineCount; ++i) {
      auto line = std::make_shared<Line2D>();
      float pos = i * spacing;

      bool isMajor = std::abs(std::fmod(pos, 1.0f)) < 0.001f;
      QVector4D color = isMajor ? planeConfig.majorColor : planeConfig.minorColor;

      QVector3D start = QVector3D(0, 0, 0) + dir2 * pos;
      QVector3D end = dir1 * m_config.size + dir2 * pos;

      line->setPoints(start, end);
      line->setColor(color);

      Primitive2DStyle style;
      style.lineWidth =
          isMajor ? planeConfig.thickness : planeConfig.thickness * 0.5f;
      line->setStyle(style);
      line->setVisible(true);

      lines.push_back(line);
    }
  }


  void Grid::generateSineWave(std::vector<std::shared_ptr<Line2D> > &lines,
                              const QVector3D &planeNormal,
                              const SineWaveConfig &config,
                              float size) {
    if (!config.visible) {
      lines.clear();
      return;
    }

    lines.clear();
    const int segments = 100; // 增加分段数以获得更平滑的曲线

    // 确定平面的两个方向向量
    QVector3D dir1, dir2;
    if (planeNormal == QVector3D(0, 0, 1)) {
      // XY平面
      dir1 = QVector3D(1, 0, 0);
      dir2 = QVector3D(0, 1, 0);
    } else if (planeNormal == QVector3D(0, 1, 0)) {
      // XZ平面
      dir1 = QVector3D(1, 0, 0);
      dir2 = QVector3D(0, 0, 1);
    } else {
      // YZ平面
      dir1 = QVector3D(0, 1, 0);
      dir2 = QVector3D(0, 0, 1);
    }

    for (int i = 0; i < segments; ++i) {
      float t1 = static_cast<float>(i) / segments;
      float t2 = static_cast<float>(i + 1) / segments;

      // x坐标从0到size
      float x1 = size * t1;
      float x2 = size * t2;

      // y坐标在[0, size]范围内变化
      // 将sin的[-1,1]范围映射到[0,size]范围
      float s1 = (sin(2 * M_PI * t1) + 1) * 0.5f * size; // 将[-1,1]映射到[0,size]
      float s2 = (sin(2 * M_PI * t2) + 1) * 0.5f * size;

      QVector3D start = dir1 * x1 + dir2 * s1;
      QVector3D end = dir1 * x2 + dir2 * s2;

      auto line = std::make_shared<Line2D>();
      line->setPoints(start, end);
      line->setColor(config.color);

      Primitive2DStyle style;
      style.lineWidth = config.thickness;
      line->setStyle(style);
      line->setVisible(true);

      lines.push_back(line);
    }
  }

  void Grid::updateGrids() {
    // 更新XY平面网格
    generateGridLines(m_xyGridLines, QVector3D(0, 0, 1), m_config.xy);

    // 更新XZ平面网格
    generateGridLines(m_xzGridLines, QVector3D(0, 1, 0), m_config.xz);

    // 更新YZ平面网格
    generateGridLines(m_yzGridLines, QVector3D(1, 0, 0), m_config.yz);

    // 更新正弦波
    generateSineWave(m_xySineWaveLines, QVector3D(0, 0, 1), m_config.xy.sineWave, m_config.size);
    generateSineWave(m_xzSineWaveLines, QVector3D(0, 1, 0), m_config.xz.sineWave, m_config.size);
    generateSineWave(m_yzSineWaveLines, QVector3D(1, 0, 0), m_config.yz.sineWave, m_config.size);

    m_batchDirty = true;
  }

  void Grid::updateBatch() {
    if (!m_batchRenderer)
      return;
    m_batchRenderer->begin();

    // 添加所有可见的网格线到批处理中
    for (const auto &line: m_xyGridLines) {
      if (line->isVisible()) {
        line->addToRenderBatch(*m_batchRenderer);
      }
    }

    for (const auto &line: m_xzGridLines) {
      if (line->isVisible()) {
        line->addToRenderBatch(*m_batchRenderer);
      }
    }

    for (const auto &line: m_yzGridLines) {
      if (line->isVisible()) {
        line->addToRenderBatch(*m_batchRenderer);
      }
    }

    // 后渲染正弦波
    for (const auto &line: m_xySineWaveLines) {
      if (line->isVisible()) {
        line->addToRenderBatch(*m_batchRenderer);
      }
    }
    for (const auto &line: m_xzSineWaveLines) {
      if (line->isVisible()) {
        line->addToRenderBatch(*m_batchRenderer);
      }
    }
    for (const auto &line: m_yzSineWaveLines) {
      if (line->isVisible()) {
        line->addToRenderBatch(*m_batchRenderer);
      }
    }

    m_batchRenderer->end();
    m_batchDirty = false;
  }
} // namespace ProGraphics
