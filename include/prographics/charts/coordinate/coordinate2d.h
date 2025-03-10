//
// Created by Yang1206 on 2025/2/16.
//

#pragma once
#include "axis.h"
#include "axis_name.h"
#include "axis_ticks.h"
#include "grid.h"
#include "prographics/charts/base/gl_widget.h"
#include "prographics/utils/camera.h"

namespace ProGraphics {
  /**
   * @brief 二维坐标系统类
   *
   * 提供完整的二维坐标系统功能，包括：
   * - 坐标轴的显示和样式配置
   * - 网格系统的显示和样式配置
   * - 轴名称的显示和位置配置
   * - 刻度的显示和格式化配置
   * - 边距的自定义配置
   */
  class Coordinate2D : public BaseGLWidget {
    Q_OBJECT

  public:
    /**
     * @brief 单个轴的配置结构
     */
    struct AxisConfig {
      bool visible = false; ///< 是否显示该轴
      float thickness = 2.0f; ///< 轴线粗细
      QVector4D color{1.0f, 1.0f, 1.0f, 1.0f}; ///< 轴线颜色
    };

    /**
     * @brief 网格配置结构
     */
    struct GridConfig {
      bool visible = true; ///< 是否显示网格
      float thickness = 1.0f; ///< 网格线粗细
      float spacing = 0.5f; ///< 网格线间距
      QVector4D majorColor{0.7f, 0.7f, 0.7f, 0.6f}; ///< 主网格线颜色
      QVector4D minorColor{0.6f, 0.6f, 0.6f, 0.3f}; ///< 次网格线颜色
      Grid::SineWaveConfig sineWave;
    };

    /**
     * @brief 整体配置结构
     */
    struct Config {
      float size = 5.0f; ///< 坐标系整体大小
      bool enabled = true; ///< 坐标系是否启用

      /**
       * @brief 轴线配置组
       */
      struct {
        bool enabled = true; ///< 轴线系统是否启用
        AxisConfig x{true, 1.5f, QVector4D(1.0f, 0.0f, 0.0f, 1.0f)}; ///< X轴配置
        AxisConfig y{true, 1.5f, QVector4D(0.0f, 1.0f, 0.0f, 1.0f)}; ///< Y轴配置
      } axis;

      /**
       * @brief 网格配置组
       */
      struct {
        bool enabled = true; ///< 网格系统是否启用
        GridConfig xy; ///< XY平面网格配置
      } grid;

      /**
       * @brief 轴名称配置组
       */
      struct {
        bool enabled = true; ///< 轴名称系统是否启用
        AxisName::NameConfig x = {
          true,
          "X",
          "",
          QVector3D(0.1f, -0.1f, 0.0f),
        }; ///< X轴的名称配置

        AxisName::NameConfig y = {
          true,
          "Y",
          "",
          QVector3D(-0.1f, 0.1f, 0.0f),
        }; ///< Y轴的名称配置
      } names;

      /**
       * @brief 刻度配置组
       */
      struct {
        bool enabled = true; ///< 刻度系统是否启用
        AxisTicks::TickConfig x{
          true,
          QVector3D(0.0f, -0.2f, 0.0f)
        }; ///< X轴的刻度配置

        AxisTicks::TickConfig y{
          true,
          QVector3D(-0.1f, 0.0f, 0.0f)
        }; ///< Y轴的刻度配置
      } ticks;

      /**
       * @brief 边距配置结构
       */
      struct Margin {
        float left = 0.5f; ///< 左边距，为Y轴刻度预留空间
        float right = 0.2f; ///< 右边距
        float top = 0.5f; ///< 上边距
        float bottom = 0.5f; ///< 下边距，为X轴刻度预留空间
      } margin;
    };

    /**
     * @brief 构造函数
     * @param parent 父窗口部件
     */
    explicit Coordinate2D(QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~Coordinate2D() override;

    /**
     * @brief 设置整体配置
     * @param config 新的配置
     */
    void setConfig(const Config &config);

    /**
     * @brief 获取当前配置
     * @return 当前配置的常量引用
     */
    const Config &config() const { return m_config; }

    // 快捷配置方法
    /**
     * @brief 设置坐标系大小
     * @param size 坐标系大小
     */
    void setSize(float size);

    /**
     * @brief 设置坐标系启用状态
     * @param enabled 是否启用
     */
    void setEnabled(bool enabled);

    // 轴相关的快捷方法
    /**
     * @brief 设置轴系统启用状态
     * @param enabled 是否启用
     */
    void setAxisEnabled(bool enabled);

    /**
     * @brief 设置指定轴的可见性
     * @param axis 轴标识符('x', 'y')
     * @param visible 是否可见
     */
    void setAxisVisible(char axis, bool visible);

    /**
     * @brief 设置指定轴的颜色
     * @param axis 轴标识符('x', 'y')
     * @param color 颜色值
     */
    void setAxisColor(char axis, const QVector4D &color);

    /**
     * @brief 设置指定轴的粗细
     * @param axis 轴标识符('x', 'y')
     * @param thickness 粗细值
     */
    void setAxisThickness(char axis, float thickness);

    // 轴名称相关的方法
    /**
     * @brief 设置轴名称系统启用状态
     * @param enabled 是否启用
     */
    void setAxisNameEnabled(bool enabled);

    /**
     * @brief 设置指定轴名称的可见性
     * @param axis 轴标识符('x', 'y')
     * @param visible 是否可见
     */
    void setAxisNameVisible(char axis, bool visible);

    /**
     * @brief 设置指定轴的名称和单位
     * @param axis 轴标识符('x', 'y')
     * @param name 轴名称
     * @param unit 单位（可选）
     */
    void setAxisName(char axis, const QString &name,
                     const QString &unit = QString());

    /**
     * @brief 设置指定轴名称的位置类型
     * @param axis 轴标识符('x', 'y')
     * @param location 位置类型
     */
    void setAxisNameLocation(char axis, AxisName::Location location);

    /**
     * @brief 设置指定轴名称的偏移量
     * @param axis 轴标识符('x', 'y')
     * @param offset 三维偏移量
     */
    void setAxisNameOffset(char axis, const QVector3D &offset);

    /**
     * @brief 设置指定轴名称的偏移量（重载版本）
     * @param axis 轴标识符('x', 'y')
     * @param x X方向偏移
     * @param y Y方向偏移
     * @param z Z方向偏移（默认为0）
     */
    void setAxisNameOffset(char axis, float x, float y, float z = 0.0f) {
      setAxisNameOffset(axis, QVector3D(x, y, z));
    }

    /**
     * @brief 设置指定轴名称与轴线的间距
     * @param axis 轴标识符('x', 'y')
     * @param gap 间距值
     */
    void setAxisNameGap(char axis, float gap);

    /**
     * @brief 设置指定轴名称的文本样式
     * @param axis 轴标识符('x', 'y')
     * @param style 文本样式
     */
    void setAxisNameStyle(char axis, const TextRenderer::TextStyle &style);

    // 网格相关的快捷方法
    /**
     * @brief 设置网格系统启用状态
     * @param enabled 是否启用
     */
    void setGridEnabled(bool enabled);

    /**
     * @brief 设置网格可见性
     * @param visible 是否可见
     */
    void setGridVisible(bool visible);

    /**
     * @brief 设置网格线颜色
     * @param majorColor 主网格线颜色
     * @param minorColor 次网格线颜色
     */
    void setGridColors(const QVector4D &majorColor, const QVector4D &minorColor);

    /**
     * @brief 设置网格线间距
     * @param spacing 间距值
     */
    void setGridSpacing(float spacing);

    /**
     * @brief 设置网格线粗细
     * @param thickness 粗细值
     */
    void setGridThickness(float thickness);

    // 刻度相关的方法
    /**
     * @brief 设置刻度系统启用状态
     * @param enabled 是否启用
     */
    void setTicksEnabled(bool enabled);

    /**
     * @brief 设置指定轴刻度的可见性
     * @param axis 轴标识符('x', 'y')
     * @param visible 是否可见
     */
    void setTicksVisible(char axis, bool visible);

    /**
     * @brief 设置指定轴的刻度范围
     * @param axis 轴标识符('x', 'y')
     * @param min 最小值
     * @param max 最大值
     * @param step 步长
     */
    void setTicksRange(char axis, float min, float max, float step);

    /**
     * @brief 设置指定轴刻度的偏移量
     * @param axis 轴标识符('x', 'y')
     * @param offset 三维偏移量
     */
    void setTicksOffset(char axis, const QVector3D &offset);

    /**
     * @brief 设置指定轴刻度的偏移量（重载版本）
     * @param axis 轴标识符('x', 'y')
     * @param x X方向偏移
     * @param y Y方向偏移
     * @param z Z方向偏移（默认为0）
     */
    void setTicksOffset(char axis, float x, float y, float z = 0.0f) {
      setTicksOffset(axis, QVector3D(x, y, z));
    }

    /**
     * @brief 设置指定轴刻度的对齐方式
     * @param axis 轴标识符('x', 'y')
     * @param alignment 对齐方式
     */
    void setTicksAlignment(char axis, Qt::Alignment alignment);

    /**
     * @brief 设置指定轴刻度的文本样式
     * @param axis 轴标识符('x', 'y')
     * @param style 文本样式
     */
    void setTicksStyle(char axis, const TextRenderer::TextStyle &style);

    /**
     * @brief 设置指定轴刻度的格式化函数
     * @param axis 轴标识符('x', 'y')
     * @param formatter 格式化函数
     */
    void setTicksFormatter(char axis, std::function<QString(float)> formatter);

    /**
     * @brief 设置坐标系边距
     * @param left 左边距
     * @param right 右边距
     * @param top 上边距
     * @param bottom 下边距
     */
    void setMargin(float left, float right, float top, float bottom);

    /**
     * @brief 设置背景颜色
     * @param color
     */
    void setBackgroundColor(QColor color) {
      m_backgroundcolor = color;
      update();
    }

    Camera camera() {
      return m_camera;
    }

  protected:
    /**
     * @brief 初始化OpenGL对象
     */
    void initializeGLObjects() override;

    /**
     * @brief 绘制OpenGL对象
     */
    void paintGLObjects() override;

    /**
     * @brief 处理窗口大小变化
     * @param w 新的宽度
     * @param h 新的高度
     */
    void resizeGL(int w, int h) override;

  private:
    /**
     * @brief 初始化着色器
     * @return 是否初始化成功
     */
    bool initializeShaders();

    /**
     * @brief 更新轴系统
     */
    void updateAxisSystem();

    /**
     * @brief 更新网格系统
     */
    void updateGridSystem();

    /**
     * @brief 更新名称系统
     */
    void updateNameSystem();

    /**
     * @brief 更新刻度系统
     */
    void updateTickSystem();

    /**
     * @brief 设置相机参数
     */
    void setupCamera();

  private:
    Config m_config; ///< 当前配置
    Camera m_camera; ///< 相机对象

    std::unique_ptr<Axis> m_axisSystem; ///< 轴系统
    std::unique_ptr<Grid> m_gridSystem; ///< 网格系统
    std::unique_ptr<AxisName> m_nameSystem; ///< 名称系统
    std::unique_ptr<AxisTicks> m_tickSystem; ///< 刻度系统

    static const char *vertexShaderSource; ///< 顶点着色器源码
    static const char *fragmentShaderSource; ///< 片段着色器源码

    QColor m_backgroundcolor{46, 59, 84};; ///< 背景颜色
  };
} // namespace ProGraphics
