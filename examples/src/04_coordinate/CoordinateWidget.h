#pragma once

#include "prographics/charts/base/gl_widget.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QGroupBox>
#include <QLabel>
#include <QMatrix4x4>
#include <QPushButton>
#include <QResizeEvent>
#include <QSlider>
#include <QVBoxLayout>
#include <QVector3D>
#include <QVector4D>

class CoordinateWidget : public ProGraphics::BaseGLWidget {
  Q_OBJECT

public:
  explicit CoordinateWidget(QWidget *parent = nullptr);

  ~CoordinateWidget() override;

protected:
  void initializeGLObjects() override;

  void paintGLObjects() override;

  void resizeGL(int w, int h) override;

  void resizeEvent(QResizeEvent *event) override;

private slots:
  void onMixValueChanged(int value);

  void onFovChanged(int value);

  void onNearPlaneChanged(int value);

  void onFarPlaneChanged(int value);

  void onWireframeModeChanged(bool checked);

  void onTranslateXChanged(int value);

  void onTranslateYChanged(int value);

  void onTranslateZChanged(int value);

  void onTintColorChanged();

  void onBgColorChanged();

private:
  QOpenGLBuffer m_vbo;
  QOpenGLTexture *m_texture1;
  QOpenGLTexture *m_texture2;

  QSlider *m_mixSlider;
  QSlider *m_fovSlider;
  QSlider *m_nearSlider;
  QSlider *m_farSlider;
  QCheckBox *m_wireframeCheckBox;
  QWidget *m_controlWidget;
  QSlider *m_translateXSlider;
  QSlider *m_translateYSlider;
  QSlider *m_translateZSlider;
  QPushButton *m_tintColorBtn;
  QPushButton *m_bgColorBtn;

  float m_mixValue{0.2f};
  float m_fov{45.0f};
  float m_nearPlane{0.1f};
  float m_farPlane{100.0f};
  bool m_wireframeMode{false};
  float m_radius{10.0f};       // 相机旋转半径
  QElapsedTimer m_cameraTimer; // 用于相机动画
  QVector4D m_tintColor{1.0f, 1.0f, 1.0f, 1.0f};
  QVector4D m_bgColor{0.0f, 0.0f, 0.0f, 1.0f};
  QVector3D m_translateVec{0.0f, 0.0f, 0.0f};

  // 立方体位置数组
  QVector<QVector3D> m_cubePositions;

  void loadTextures();

  void setupUI();

  void initCubePositions();
};
