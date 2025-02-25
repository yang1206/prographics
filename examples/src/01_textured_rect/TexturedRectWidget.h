#pragma once

#include "prographics/charts/base/gl_widget.h"

class TexturedRectWidget : public ProGraphics::BaseGLWidget {
  Q_OBJECT

public:
  explicit TexturedRectWidget(QWidget *parent = nullptr);

  ~TexturedRectWidget() override;

protected:
  void initializeGLObjects() override;
  void paintGLObjects() override;

private:
  QOpenGLBuffer m_vbo;                             ///< 顶点缓冲对象（VBO）
  QOpenGLBuffer m_ebo{QOpenGLBuffer::IndexBuffer}; ///< 纹理坐标缓冲对象（EBO）
  QOpenGLTexture *m_texture;

  void loadTexture(const QString &fileName);
};
