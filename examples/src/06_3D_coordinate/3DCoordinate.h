#pragma once

#include "prographics/charts/base/gl_widget.h"
#include "prographics/core/renderer/text_renderer.h"
#include "prographics/utils/camera.h"
#include "prographics/utils/orbit_controls.h"

#include <QMouseEvent>

class CoordinateComponent {
public:
  CoordinateComponent() = default;

  // 禁用拷贝
  CoordinateComponent(const CoordinateComponent &) = delete;

  CoordinateComponent &operator=(const CoordinateComponent &) = delete;

  // 启用移动
  CoordinateComponent(CoordinateComponent &&) = delete;

  CoordinateComponent &operator=(CoordinateComponent &&) = delete;

  ~CoordinateComponent() { cleanup(); }

  void cleanup() {
    if (vbo.isCreated())
      vbo.destroy();
    if (vao.isCreated())
      vao.destroy();
  }

  void setVisible(bool v) { visible = v; }
  bool isVisible() const { return visible; }

  QOpenGLBuffer vbo{QOpenGLBuffer::VertexBuffer};
  QOpenGLVertexArrayObject vao;
  int vertexCount = 0;

private:
  bool visible = true;
};

class ThreeDCoordinate : public ProGraphics::BaseGLWidget {
  Q_OBJECT

public:
  explicit ThreeDCoordinate(QWidget *parent = nullptr);

  ~ThreeDCoordinate() override;

protected:
  void initializeGLObjects() override;

  void paintGLObjects() override;

  void resizeGL(int w, int h) override;

  void mousePressEvent(QMouseEvent *event) override;

  void mouseMoveEvent(QMouseEvent *event) override;

  void mouseReleaseEvent(QMouseEvent *event) override;

  void wheelEvent(QWheelEvent *event) override;

private:
  // 各个组件
  CoordinateComponent m_axes; // 坐标轴

  CoordinateComponent m_xyGrid; // XY平面网格
  CoordinateComponent m_xzGrid; // XZ平面网格
  CoordinateComponent m_yzGrid; // YZ平面网格

  CoordinateComponent m_xTicks; // X轴刻度
  CoordinateComponent m_yTicks; // Y轴刻度
  CoordinateComponent m_zTicks; // Z轴刻度

  CoordinateComponent m_xyPlane; // XY平面
  CoordinateComponent m_xzPlane; // XZ平面
  CoordinateComponent m_yzPlane; // YZ平面

  std::vector<float> generateRandomData(int count, float minVal, float maxVal);

  struct LineGroup {
    CoordinateComponent lines;
    float zPosition = 5.0f;
    bool isActive = true;
  };

  std::vector<std::unique_ptr<LineGroup>> m_lineGroups; // 存储所有线条组

  QTimer m_generationTimer;           // 用于定期生成新数据的定时器
  float m_prpsAnimationSpeed = 0.05f; // 动画速度

  // 新增函数
  void createNewLineGroup();    // 创建新的线条组
  void cleanupInactiveGroups(); // 清理已完成动画的线条组

  float m_prpsZPosition = 5.0f; // 初始Z轴位置
  QTimer m_prpsAnimationTimer;  // 动画定时器

  void updatePRPSAnimation(); // 更新动画

  ProGraphics::TextRenderer m_textRenderer;

  struct {
    std::vector<ProGraphics::TextRenderer::Label *> x;
    std::vector<ProGraphics::TextRenderer::Label *> y;
    std::vector<ProGraphics::TextRenderer::Label *> z;
  } m_axisLabels;

  // 相机
  ProGraphics::Camera m_camera;

  std::unique_ptr<ProGraphics::OrbitControls> m_controls;

  int m_axisVertexCount = 0; // 坐标轴顶点数

  // 初始化各个组件
  void initializeAxes();   // 初始化坐标轴
  void initializeGrids();  // 初始化网格线
  void initializeTicks();  // 初始化刻度
  void initializePlanes(); // 初始化平面（调试用）
  void initializeLabels();

  // 辅助函数
  void setupComponent(CoordinateComponent &component,
                      const std::vector<float> &vertices);

  void drawComponent(CoordinateComponent &component, GLenum mode);

  void cleanupComponent(CoordinateComponent &component);
};
