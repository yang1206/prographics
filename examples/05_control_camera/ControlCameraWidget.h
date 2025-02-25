#pragma once

#include "prographics/charts/base/gl_widget.h"
#include "prographics/utils/camera.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QSlider>
#include <QVector4D>

class ControlCameraWidget : public ProGraphics::BaseGLWidget {
  Q_OBJECT

public:
  explicit ControlCameraWidget(QWidget *parent = nullptr);

  ~ControlCameraWidget() override;

protected:
  void initializeGLObjects() override;

  void paintGLObjects() override;

  void resizeGL(int w, int h) override;

  void resizeEvent(QResizeEvent *event) override;

  void keyPressEvent(QKeyEvent *event) override;

  void keyReleaseEvent(QKeyEvent *event) override;

  void mousePressEvent(QMouseEvent *event) override;

  void mouseMoveEvent(QMouseEvent *event) override;

  void wheelEvent(QWheelEvent *event) override;

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

  void onCameraTypeChanged(int index);

  void onProjectionTypeChanged(int index);

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
  QComboBox *m_cameraTypeCombo;
  QComboBox *m_projectionTypeCombo;

  float m_mixValue{0.2f};

  bool m_wireframeMode{false};

  float m_radius{10.0f}; // 相机旋转半径

  QVector4D m_tintColor{1.0f, 1.0f, 1.0f, 1.0f};
  QVector4D m_bgColor{0.18f, 0.23f, 0.33f, 1.0f};
  QVector3D m_translateVec{0.0f, 0.0f, 0.0f};

  float m_cameraSpeed{0.05f};

  ProGraphics::Camera m_camera;
  float m_deltaTime{0.0f};
  float m_lastFrame{0.0f};
  QElapsedTimer m_frameTimer;
  QPoint m_lastMousePos;
  bool m_firstMouse{true};

  QMap<int, bool> m_pressedKeys;

  // 立方体位置数组
  QVector<QVector3D> m_cubePositions;

  void loadTextures();

  void setupUI();

  void initCubePositions();
};
