#pragma once
#include "axis_name.h"
#include "axis_ticks.h"
#include "prographics/charts/base/gl_widget.h"
#include "prographics/core/renderer/text_renderer.h"
#include "prographics/utils/camera.h"
#include "prographics/utils/orbit_controls.h"

namespace ProGraphics {
  class Axis;
  class AxisTicks;
  class AxisName;
  class Grid;

  /**
   * @brief 三维坐标系统类
   * 
   * 提供完整的三维坐标系统功能，包括：
   * - 坐标轴的显示和样式配置
   * - 三个平面的网格系统显示和样式配置
   * - 轴名称的显示和位置配置
   * - 刻度的显示和格式化配置
   * - 轨道控制相机视角
   */
  class Coordinate3D : public BaseGLWidget {
    Q_OBJECT

  public:
    /**
     * @brief 单个轴的配置结构
     */
    struct AxisConfig {
      bool visible = true; ///< 是否显示该轴
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
      QVector4D majorColor{0.5f, 0.5f, 0.5f, 0.5f}; ///< 主网格线颜色
      QVector4D minorColor{0.3f, 0.3f, 0.3f, 0.3f}; ///< 次网格线颜色
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
        AxisConfig x{true, 2.0f, QVector4D(1.0f, 0.0f, 0.0f, 1.0f)}; ///< X轴配置（红色）
        AxisConfig y{true, 2.0f, QVector4D(0.0f, 1.0f, 0.0f, 1.0f)}; ///< Y轴配置（绿色）
        AxisConfig z{true, 2.0f, QVector4D(0.0f, 0.0f, 1.0f, 1.0f)}; ///< Z轴配置（蓝色）
      } axis;

      /**
       * @brief 网格配置组
       */
      struct {
        bool enabled = true; ///< 网格系统是否启用
        GridConfig xy{
          true, 1.0f, 1.0f, ///< XY平面网格配置
          QVector4D(0.7f, 0.7f, 0.7f, 0.6f),
          QVector4D(0.6f, 0.6f, 0.6f, 0.3f)
        };
        GridConfig xz{
          true, 1.0f, 1.0f, ///< XZ平面网格配置
          QVector4D(0.7f, 0.7f, 0.7f, 0.6f),
          QVector4D(0.6f, 0.6f, 0.6f, 0.3f)
        };
        GridConfig yz{
          false, 1.0f, 0.5f, ///< YZ平面网格配置
          QVector4D(0.7f, 0.7f, 0.7f, 0.6f),
          QVector4D(0.6f, 0.6f, 0.6f, 0.3f)
        };
      } grid;

      /**
       * @brief 轴名称配置组
       */
      struct {
        bool enabled = true; ///< 轴名称系统是否启用
        AxisName::NameConfig x{
          true, "X", "", QVector3D(0.5f, 0.0f, 0.5f),
          0.1f, AxisName::Location::End,
          TextRenderer::TextStyle()
        }; ///< X轴名称配置
        AxisName::NameConfig y{
          true, "Y", "", QVector3D(-0.5f, 0.5f, 0.0f),
          0.1f, AxisName::Location::End,
          TextRenderer::TextStyle()
        }; ///< Y轴名称配置
        AxisName::NameConfig z{
          true, "Z", "", QVector3D(-0.5f, 0.0f, 0.0f),
          0.1f, AxisName::Location::Middle,
          TextRenderer::TextStyle()
        }; ///< Z轴名称配置
      } names;

      /**
       * @brief 刻度配置组
       */
      struct {
        bool enabled = true; ///< 刻度系统是否启用
        AxisTicks::TickConfig x{
          true, QVector3D(0.0f, 0.0f, 0.5f)
        }; /// x轴的默认刻度配置
        AxisTicks::TickConfig y{
          true, QVector3D(-0.2f, 0.0f, 0.0f)
        }; /// y轴的默认刻度配置
        AxisTicks::TickConfig z{
          true, QVector3D(-0.5f, 0.0f, 0.0f)
        }; ///< Z轴的刻度配置
      } ticks;
    };

    /**
     * @brief 构造函数
     * @param parent 父窗口部件
     */
    explicit Coordinate3D(QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~Coordinate3D() override;

    void setConfig(const Config &config);

    const Config &config() const { return m_config; }

    // 快捷方法
    void setSize(float size);

    void setEnabled(bool enabled);

    // 轴相关的快捷方法
    void setAxisEnabled(bool enabled);

    void setAxisVisible(char axis, bool visible); // axis: 'x', 'y', 'z'
    void setAxisColor(char axis, const QVector4D &color);

    void setAxisThickness(char axis, float thickness);

    // 轴名称相关的方法
    void setAxisNameEnabled(bool enabled);

    void setAxisNameVisible(char axis, bool visible);

    void setAxisName(char axis, const QString &name,
                     const QString &unit = QString());

    void setAxisNameLocation(char axis, AxisName::Location location);

    void setAxisNameOffset(char axis, const QVector3D &offset);

    void setAxisNameOffset(char axis, float x, float y, float z = 0.0f) {
      setAxisNameOffset(axis, QVector3D(x, y, z));
    }

    /**
     * @brief 设置轴名称与轴线之间的间距
     *
     * @param axis 指定轴('x', 'y', 'z')
     * @param gap 间距值，表示轴名称与轴线末端之间的距离
     *
     * 当轴名称位置为 Location::End 时，gap 为正值表示向外延伸
     * 当轴名称位置为 Location::Start 时，gap 为正值表示向内缩进
     * 当轴名称位置为 Location::Middle 时，gap 不起作用
     */
    void setAxisNameGap(char axis, float gap);

    void setAxisNameStyle(char axis, const TextRenderer::TextStyle &style);

    // 网格相关的快捷方法
    void setGridEnabled(bool enabled);

    void setGridVisible(const QString &plane,
                        bool visible); // plane: "xy", "xz", "yz"
    void setGridColors(const QString &plane, const QVector4D &majorColor,
                       const QVector4D &minorColor);

    void setGridSpacing(const QString &plane, float spacing);

    void setGridThickness(const QString &plane, float thickness);

    // 刻度相关的方法
    void setTicksEnabled(bool enabled);

    void setTicksVisible(char axis, bool visible);

    void setTicksRange(char axis, float min, float max, float step);

    void setTicksOffset(char axis, const QVector3D &offset);

    void setTicksAlignment(char axis, Qt::Alignment alignment);

    void setTicksStyle(char axis, const TextRenderer::TextStyle &style);

    void setTicksFormatter(char axis, std::function<QString(float)> formatter);

    // 组合配置方法
    void setAxisAndGridColor(char axis, const QVector4D &axisColor,
                             const QVector4D &gridMajorColor,
                             const QVector4D &gridMinorColor);

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

    /**
     * @brief 处理鼠标按下事件
     * 用于轨道控制相机视角
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * @brief 处理鼠标移动事件
     * 用于轨道控制相机视角
     */
    void mouseMoveEvent(QMouseEvent *event) override;

    /**
     * @brief 处理鼠标释放事件
     * 用于轨道控制相机视角
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

    /**
     * @brief 处理鼠标滚轮事件
     * 用于轨道控制相机缩放
     */
    void wheelEvent(QWheelEvent *event) override;

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

  private:
    Config m_config; ///< 当前配置
    Camera m_camera; ///< 相机对象
    std::unique_ptr<OrbitControls> m_controls; ///< 轨道控制器

    std::unique_ptr<Axis> m_axisSystem; ///< 轴系统
    std::unique_ptr<Grid> m_gridSystem; ///< 网格系统
    std::unique_ptr<AxisName> m_nameSystem; ///< 名称系统
    std::unique_ptr<AxisTicks> m_tickSystem; ///< 刻度系统

    static const char *vertexShaderSource; ///< 顶点着色器源码
    static const char *fragmentShaderSource; ///< 片段着色器源码
  };
} // namespace ProGraphics
