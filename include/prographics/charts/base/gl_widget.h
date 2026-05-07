#pragma once
#include "prographics/prographics_export.h"
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QSurfaceFormat>
#include <qelapsedtimer.h>

namespace ProGraphics {
  /**
   * 专用于本库 QOpenGLWidget 的表面格式：OpenGL 4.1 Core、无多重采样（GL_POINTS 为矩形、开销低）。
   * 在图表构造函数中对此控件调用 setFormat(chartSurfaceFormat()) 即可，无需在 main 里 setDefaultFormat 连累整个进程。
   */
  [[nodiscard]] PROGRAPHICS_EXPORT QSurfaceFormat chartSurfaceFormat();

  class PROGRAPHICS_EXPORT BaseGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
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
