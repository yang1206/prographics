#pragma once

#include "prographics/charts/base/gl_widget.h"
#include <QSlider>

class TexturedUnitWidget : public ProGraphics::BaseGLWidget {
  Q_OBJECT

public:
  explicit TexturedUnitWidget(QWidget *parent = nullptr);

  ~TexturedUnitWidget() override;

protected:
  void initializeGLObjects() override;
  void paintGLObjects() override;
private slots:
  void onMixValueChanged(int value);

private:
  QOpenGLBuffer m_vbo;                             ///< 顶点缓冲对象（VBO）
  QOpenGLBuffer m_ebo{QOpenGLBuffer::IndexBuffer}; ///< 纹理坐标缓冲对象（EBO）
  QOpenGLTexture *m_texture1;
  QOpenGLTexture *m_texture2;
  QSlider *m_mixSlider;
  float m_mixValue{0.2f};
  void loadTextures();
};
