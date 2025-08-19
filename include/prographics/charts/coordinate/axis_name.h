//
// Created by Yang1206 on 2025/2/16.
//

#pragma once
#include "prographics/core/renderer/text_renderer.h"
#include <QMatrix4x4>
#include <memory>

namespace ProGraphics {
  /**
   * @brief 坐标轴名称管理类
   *
   * 负责管理和渲染坐标轴的名称标签，包括位置、样式和单位的显示。
   * 支持三维坐标系中X、Y、Z轴的名称显示，可以自定义名称的位置、偏移和样式。
   */
  class AxisName {
  public:
    /**
     * @brief 轴名称位置枚举
     *
     * 定义轴名称在轴上的位置类型
     */
    enum class Location {
      Start, ///< 轴的起始位置（坐标原点）
      Middle, ///< 轴的中间位置
      End ///< 轴的结束位置（轴的最大值处）
    };

    /**
     * @brief 单个轴名称的配置结构
     */
    struct NameConfig {
      bool visible; ///< 是否显示该轴的名称
      QString text; ///< 轴名称文本
      QString unit; ///< 单位文本，将显示在名称后的括号中
      QVector3D offset; ///< 相对于基准位置的三维偏移量
      float gap; ///< 与轴线端点的间距
      Location location; ///< 名称在轴上的位置类型
      TextRenderer::TextStyle style; ///< 文本渲染样式

      // 构造函数
      NameConfig();

      NameConfig(bool visible, const QString &text, const QString &unit = QString());

      NameConfig(bool visible, const QString &text, const QString &unit, const QVector3D &offset);

      NameConfig(bool visible, const QString &text, const QString &unit,
                 const QVector3D &offset, float gap = 0.1f,
                 Location location = Location::End);

      NameConfig(bool visible, const QString &text, const QString &unit,
                 const QVector3D &offset,
                 Location location = Location::End);
    };

    /**
   * @brief 整体配置结构
   */
    struct Config {
      float size; ///< 坐标系整体大小
      NameConfig x; ///< X轴名称配置
      NameConfig y; ///< Y轴名称配置
      NameConfig z; ///< Z轴名称配置

      // 构造函数
      Config();

      Config(float size);
    };

    AxisName();

    ~AxisName();

    /**
     * @brief 初始化轴名称系统
     *
     * 必须在OpenGL上下文中调用
    */
    void initialize();

    /**
     * @brief 渲染轴名称
     *
     * @param painter QPainter对象，用于文本渲染
     * @param view 视图矩阵
     * @param projection 投影矩阵
     * @param width 视口宽度
     * @param height 视口高度
     */
    void render(QPainter &painter, const QMatrix4x4 &view,
                const QMatrix4x4 &projection, int width, int height);

    /**
     * @brief 设置配置
     *
     * @param config 新的配置
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
        * @brief 更新所有轴的名称
        *
        * 根据当前配置更新所有轴名称的文本和位置
     */
    void updateNames();

    /**
     * @brief 计算轴名称的位置
     *
     * @param axis 轴标识符('x', 'y', 'z')
     * @param config 该轴的名称配置
     * @return 计算得到的三维位置
     */
    QVector3D calculateNamePosition(char axis, const NameConfig &config) const;

    /**
     * @brief 默认配置
     *
     * 包含所有轴的默认名称配置
     */
    Config m_config;

    std::unique_ptr<TextRenderer> m_textRenderer;
    TextRenderer::Label *m_xName = nullptr;
    TextRenderer::Label *m_yName = nullptr;
    TextRenderer::Label *m_zName = nullptr;
    bool m_initialized = false;
  };
} // namespace ProGraphics
