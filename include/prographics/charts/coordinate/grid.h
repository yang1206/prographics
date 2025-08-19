#pragma once
#include "prographics/core/graphics/primitive2d.h"
#include <QMatrix4x4>
#include <memory>

namespace ProGraphics {
  class Grid {
  public:
    struct SineWaveConfig {
      bool visible;
      float thickness;
      QVector4D color; // 默认白色，透明度0.8
      float amplitude; // 振幅

      // 构造函数
      SineWaveConfig();

      SineWaveConfig(bool visible, float thickness, const QVector4D &color, float amplitude);

      SineWaveConfig(bool visible, const QVector4D &color);
    };

    struct PlaneConfig {
      bool visible;
      float thickness;
      float spacing;
      QVector4D majorColor;
      QVector4D minorColor;
      SineWaveConfig sineWave;

      // 构造函数
      PlaneConfig();

      PlaneConfig(bool visible, float thickness, float spacing,
                  const QVector4D &majorColor, const QVector4D &minorColor);
    };

    struct Config {
      float size;
      PlaneConfig xy; // XY平面配置
      PlaneConfig xz; // XZ平面配置
      PlaneConfig yz; // YZ平面配置

      // 构造函数
      Config();

      Config(float size);
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
