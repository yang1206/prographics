#pragma once

#include "prographics/charts/base/gl_widget.h"
#include "prographics/core/graphics/primitive2d.h"
#include "prographics/core/graphics/shape3d.h"
#include "prographics/core/renderer//text_renderer.h"
#include "prographics/utils/camera.h"
#include "prographics/utils/orbit_controls.h"

#include <QMouseEvent>

class PlayGround : public ProGraphics::BaseGLWidget {
  Q_OBJECT

public:
  explicit PlayGround(QWidget *parent = nullptr);

  ~PlayGround() override;

protected:
  void initializeGLObjects() override;

  void paintGLObjects() override;

  void resizeGL(int w, int h) override;

  void mousePressEvent(QMouseEvent *event) override;

  void mouseMoveEvent(QMouseEvent *event) override;

  void mouseReleaseEvent(QMouseEvent *event) override;

  void wheelEvent(QWheelEvent *event) override;

private:
  struct CoordinateComponent {
    QOpenGLBuffer vbo;
    QOpenGLVertexArrayObject vao;
    int vertexCount = 0;
    bool visible = true;
  };

  // TODO
  std::unique_ptr<ProGraphics::Line2D> m_testLine;
  std::unique_ptr<ProGraphics::Point2D> m_testPoint;
  std::unique_ptr<ProGraphics::Triangle2D> m_testTriangle;
  std::unique_ptr<ProGraphics::Rectangle2D> m_testRectangle;
  std::unique_ptr<ProGraphics::Circle2D> m_testCircle;

  std::unique_ptr<ProGraphics::Cube> m_testCube;
  std::unique_ptr<ProGraphics::Cylinder> m_testCylinder;
  std::unique_ptr<ProGraphics::Sphere> m_testSphere;
  std::unique_ptr<ProGraphics::Arrow> m_testArrow;
  std::vector<std::unique_ptr<ProGraphics::Cube> > m_cubes;

  std::unique_ptr<ProGraphics::Sphere> m_instancedSpheres;
  std::vector<ProGraphics::Transform> m_sphereInstances;
  float m_instanceRotation = 0.0f; // 用于动画
  float m_animationAngle = 0.0f; // 用于控制动画
  std::unique_ptr<ProGraphics::Sphere> m_lodSphere;

  std::unique_ptr<ProGraphics::Line2D> m_instancedLines;
  std::vector<ProGraphics::Transform2D> m_lineInstances;
  float m_lineInstanceRotation = 0.0f;

  std::unique_ptr<ProGraphics::Point2D> m_instancedPoints;
  std::vector<ProGraphics::Transform2D> m_pointInstances;
  float m_pointInstanceRotation = 0.0f;

  std::unique_ptr<ProGraphics::OrbitControls> m_controls;

  // 相机
  ProGraphics::Camera m_camera;

  int m_axisVertexCount = 0; // 坐标轴顶点数

  QElapsedTimer *m_elapsedTimer;
  QTimer *m_animationTimer;
};
