//
// Created by Yang1206 on 2025/2/16.
//

#pragma once
#include "prographics/core/graphics/primitive2d.h"
#include <memory>

namespace ProGraphics {
  /**
   * @brief 坐标轴管理类
   *
   * 负责管理和渲染三维坐标系的坐标轴。
   * 支持独立配置X、Y、Z轴的可见性、粗细和颜色。
   * 使用批处理渲染技术优化性能。
   */
  class Axis {
  public:
    /**
     * @brief 坐标轴配置结构
     */
    struct Config {
      float length; ///< 坐标轴长度

      // X轴配置
      bool xAxisVisible; ///< X轴是否可见
      float xAxisThickness; ///< X轴线条粗细
      QVector4D xAxisColor; ///< X轴颜色（默认红色）

      // Y轴配置
      bool yAxisVisible; ///< Y轴是否可见
      float yAxisThickness; ///< Y轴线条粗细
      QVector4D yAxisColor; ///< Y轴颜色（默认绿色）

      // Z轴配置
      bool zAxisVisible; ///< Z轴是否可见
      float zAxisThickness; ///< Z轴线条粗细
      QVector4D zAxisColor; ///< Z轴颜色（默认蓝色）

      // 构造函数
      Config();

      Config(float length);

      Config(float length,
             bool xAxisVisible, float xAxisThickness, const QVector4D &xAxisColor,
             bool yAxisVisible, float yAxisThickness, const QVector4D &yAxisColor,
             bool zAxisVisible, float zAxisThickness, const QVector4D &zAxisColor);
    };

    Axis();

    ~Axis();

    /**
     * @brief 初始化坐标轴系统
     *
     * 必须在OpenGL上下文中调用
     * 创建必要的OpenGL资源和批处理渲染器
     */
    void initialize();

    /**
     * @brief 渲染坐标轴
     *
     * @param projection 投影矩阵
     * @param view 视图矩阵
     */
    void render(const QMatrix4x4 &projection, const QMatrix4x4 &view);

    /**
     * @brief 清理资源
     *
     * 释放OpenGL资源和批处理渲染器
     */
    void cleanup();

    /**
    * @brief 设置配置
    *
    * @param config 新的配置
    * 更新配置后会自动触发坐标轴和批处理缓存的更新
    */
    void setConfig(const Config &config);

    /**
     * @brief 获取当前配置
     *
     * @return 当前配置的常量引用
     */
    const Config &config() const { return m_config; }

  private:
    /**
     * @brief 更新坐标轴
     *
     * 根据当前配置更新所有坐标轴的属性
     * 包括可见性、粗细、颜色等
     */
    void updateAxes();

    /**
     * @brief 更新批处理缓存
     *
     * 将所有可见的坐标轴添加到批处理渲染器中
     * 优化渲染性能
     */
    void updateBatch();

    Config m_config;
    std::shared_ptr<Line2D> m_xAxis;
    std::shared_ptr<Line2D> m_yAxis;
    std::shared_ptr<Line2D> m_zAxis;

    // 批处理相关
    std::unique_ptr<Primitive2DBatch> m_batchRenderer;
    bool m_batchDirty = true;
    bool m_initialized = false;
  };
} // namespace ProGraphics
