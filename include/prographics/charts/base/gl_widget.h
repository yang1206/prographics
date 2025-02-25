#pragma once

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <qelapsedtimer.h>

namespace ProGraphics {
  class BaseGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

  public:
    explicit BaseGLWidget(QWidget *parent = nullptr);

    ~BaseGLWidget() override;

  protected:
    // OpenGL 基础函数
    void initializeGL() override;

    void paintGL() override;

    void resizeGL(int w, int h) override;

    // 子类需要实现的虚函数
    virtual void initializeGLObjects() = 0;

    virtual void paintGLObjects() = 0;

  protected:
    QOpenGLShaderProgram *m_program;
    QOpenGLVertexArrayObject m_vao;
    QElapsedTimer m_timer;
  };
} // namespace ProGraphics
