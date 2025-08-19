//
// Created by Yang1206 on 2025/2/16.
//


#pragma once
#include "prographics/core/renderer/text_renderer.h"
#include <QMatrix4x4>
#include <memory>

namespace ProGraphics {
  /**
   * @brief 坐标轴刻度管理类
   *
   * 负责管理和渲染坐标轴的刻度标签，包括位置、样式和格式化。
   * 支持三维坐标系中X、Y、Z轴的刻度显示，可以自定义刻度的位置、偏移、样式和数值格式化方式。
   */
  class AxisTicks {
  public:
    /**
     * @brief 单个轴的刻度配置结构
     */
    struct TickConfig {
      bool visible = true; ///< 是否显示该轴的刻度
      QVector3D offset{0.0f, 0.0f, 0.0f}; ///< 相对于基准位置的三维偏移量
      float margin = 0.5f; ///< 刻度与轴线的间距
      TextRenderer::TextStyle style; ///< 刻度文本的渲染样式
      Qt::Alignment alignment = Qt::AlignCenter; ///< 刻度文本的对齐方式
      /**
       * @brief 刻度值格式化函数
       *
       * 默认保留一位小数
       * @param value 刻度数值
       * @return 格式化后的文本
       */
      std::function<QString(float)> formatter = [](float value) {
        return QString::number(value, 'f', 1);
      };

      /**
    * @brief 刻度范围配置结构
    */
      struct Range {
        float min; ///< 最小刻度值
        float max; ///< 最大刻度值
        float step; ///< 主刻度间隔

        Range();

        Range(float min, float max, float step);
      } range;

      TickConfig() = default;

      TickConfig(const Range &r) : range(r) {
      }

      TickConfig(bool visible, const QVector3D &offset, const Range &r = Range{}) : visible(visible), offset(offset),
        range(r) {
      }

      TickConfig(bool visible, const QVector3D &offset, const TextRenderer::TextStyle &style,
                 const Range &r = Range{}) : visible(visible), offset(offset), style(style),
                                             range(r) {
      }

      TickConfig(bool visible, const QVector3D &offset, const Qt::Alignment &alignment,
                 const Range &r = Range{}) : visible(visible), offset(offset), alignment(alignment),
                                             range(r) {
      }

      TickConfig(bool visible, const QVector3D &offset, float margin, const TextRenderer::TextStyle &style,
                 Qt::Alignment alignment,
                 const Range &r = Range{})
        : visible(visible), offset(offset), margin(margin), style(style),
          alignment(alignment), range(r) {
      }
    };


    /**
     * @brief 整体配置结构
     */
    struct Config {
      float size = 5.0f; ///< 坐标系整体大小
      float majorSpacing = 1.0f; ///< 主刻度间距
      float minorSpacing = 0.5f; ///< 次刻度间距
      bool showMinorTicks = false; ///< 是否显示次刻度
      TickConfig x; ///< X轴刻度配置
      TickConfig y; ///< Y轴刻度配置
      TickConfig z; ///< Z轴刻度配置
    };

    AxisTicks();

    ~AxisTicks();

    /**
     * @brief 初始化刻度系统
     *
     * 必须在OpenGL上下文中调用
     */
    void initialize();

    /**
     * @brief 渲染刻度
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
    * @brief 更新所有轴的刻度
    *
    * 根据当前配置更新所有轴刻度的文本和位置
    */
    void updateTicks();

    /**
     * @brief 更新单个轴的刻度
     *
     * @param axis 轴标识符('x', 'y', 'z')
     * @param config 该轴的刻度配置
     */
    void updateAxisTicks(char axis, const TickConfig &config);

    /**
    * @brief 计算刻度的位置
    *
    * @param axis 轴标识符('x', 'y', 'z')
    * @param value 刻度值
    * @param offset 位置偏移量
    * @return 计算得到的三维位置
    */
    QVector3D calculateTickPosition(char axis, float value, QVector3D offset) const;

    Config m_config;
    std::unique_ptr<TextRenderer> m_textRenderer;
    std::vector<TextRenderer::Label *> m_tickLabels;
    bool m_initialized = false;
  };
} // namespace ProGraphics
