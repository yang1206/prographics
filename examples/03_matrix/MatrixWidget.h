//
// Created by Yang1206 on 2025/2/6.
//

#ifndef MATRIXWIDGET_H
#define MATRIXWIDGET_H
#include "prographics/charts/base/gl_widget.h"
#include <QCheckBox>
#include <QMatrix4x4>
#include <QSlider>

class MatrixWidget : public ProGraphics::BaseGLWidget {
  Q_OBJECT
public:
  explicit MatrixWidget(QWidget *parent = nullptr);
  ~MatrixWidget();

protected:
  void initializeGLObjects() override;
  void paintGLObjects() override;
private slots:
  void onMixValueChanged(int value);
  void onTranslateXChanged(int value);
  void onTranslateYChanged(int value);
  void onRotateAngleChanged(int value);
  void onScaleXChanged(int value);
  void onScaleYChanged(int value);
  void onAutoRotateChanged(bool checked);

private:
  QOpenGLBuffer m_vbo;
  QOpenGLBuffer m_ebo{QOpenGLBuffer::IndexBuffer};
  QOpenGLTexture *m_texture1;
  QOpenGLTexture *m_texture2;

  QSlider *m_mixSlider;
  QSlider *m_translateXSlider;
  QSlider *m_translateYSlider;
  QSlider *m_rotateSlider;
  QSlider *m_scaleXSlider;
  QSlider *m_scaleYSlider;
  QCheckBox *m_autoRotateCheckBox;

  float m_mixValue{0.2f};
  float m_translateX{0.1f};
  float m_translateY{-0.1f};
  float m_rotateAngle{0.0f};
  float m_scaleX{1.0f};
  float m_scaleY{1.0f};
  bool m_autoRotate{true};

  void loadTextures();
  void setupUI();
};

#endif // MATRIXWIDGET_H
