#pragma once
#include "prographics/core/graphics/primitive2d.h"
#include <QMatrix4x4>
#include <memory>

namespace ProGraphics {
  class Grid {
  public:
    struct SineWaveConfig {
      bool visible = false;
      float thickness = 2.0f;
      QVector4D color{0.96f, 0.96f, 0.96f, 0.8f}; // 默认白色，透明度0.8
      float amplitude = 1.0f; // 振幅
    };

    struct PlaneConfig {
      bool visible = true;
      float thickness = 1.0f;
      float spacing = 1.0f;
      QVector4D majorColor{0.5f, 0.5f, 0.5f, 0.5f};
      QVector4D minorColor{0.3f, 0.3f, 0.3f, 0.3f};
      SineWaveConfig sineWave;
    };

    struct Config {
      float size = 5.0f;
      PlaneConfig xy; // XY平面配置
      PlaneConfig xz; // XZ平面配置
      PlaneConfig yz; // YZ平面配置
    };

    Grid();

    ~Grid();

    void initialize();

    void render(const QMatrix4x4 &projection, const QMatrix4x4 &view);

    void cleanup();

    void setConfig(const Config &config);

    const Config &config() const { return m_config; }

  private:
    void updateGrids();

    void updateBatch();

    void generateGridLines(std::vector<std::shared_ptr<Line2D> > &lines,
                           const QVector3D &planeNormal,
                           const PlaneConfig &planeConfig);

    void generateSineWave(std::vector<std::shared_ptr<Line2D> > &lines,
                          const QVector3D &planeNormal,
                          const SineWaveConfig &config,
                          float size);

    Config m_config;
    // XY平面网格线
    std::vector<std::shared_ptr<Line2D> > m_xyGridLines;
    // XZ平面网格线
    std::vector<std::shared_ptr<Line2D> > m_xzGridLines;
    // YZ平面网格线
    std::vector<std::shared_ptr<Line2D> > m_yzGridLines;

    std::vector<std::shared_ptr<Line2D> > m_xySineWaveLines;
    std::vector<std::shared_ptr<Line2D> > m_xzSineWaveLines;
    std::vector<std::shared_ptr<Line2D> > m_yzSineWaveLines;

    std::unique_ptr<Primitive2DBatch> m_batchRenderer;
    bool m_batchDirty = true;
    bool m_initialized = false;
  };
} // namespace ProGraphics
