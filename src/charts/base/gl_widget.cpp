#include "prographics/charts/base/gl_widget.h"

namespace ProGraphics {
    QSurfaceFormat chartSurfaceFormat() {
        QSurfaceFormat f;
        f.setRenderableType(QSurfaceFormat::OpenGL);
        f.setVersion(4, 1);
        f.setProfile(QSurfaceFormat::CoreProfile);
        f.setDepthBufferSize(24);
        f.setStencilBufferSize(8);
        f.setSamples(0);
        f.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
        f.setSwapInterval(1);
        return f;
    }

    BaseGLWidget::BaseGLWidget(QWidget *parent)
        : QOpenGLWidget(parent), m_program(nullptr) {
        setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
        // setFormat(chartSurfaceFormat());
    }

    BaseGLWidget::~BaseGLWidget() {
        makeCurrent();
        delete m_program;
        doneCurrent();
    }

    void BaseGLWidget::initializeGL() {
        initializeOpenGLFunctions();

        if (!QOpenGLContext::currentContext()->isValid()) {
            qDebug() << "OpenGL上下文无效";
            return;
        }
        // 检查版本支持
        QSurfaceFormat format = QOpenGLContext::currentContext()->format();
        if (format.version() < qMakePair(4, 1)) {
            qDebug() << "不支持OpenGL 4.1 Core";
            return;
        }

        m_timer.start();

        // qDebug() << "OpenGL Version:"
        //         << reinterpret_cast<const char *>(glGetString(GL_VERSION));
        // qDebug() << "OpenGL Vendor:"
        //         << reinterpret_cast<const char *>(glGetString(GL_VENDOR));
        // qDebug() << "OpenGL Renderer:"
        //         << reinterpret_cast<const char *>(glGetString(GL_RENDERER));
        // qDebug() << "OpenGL Shading Language Version:"
        //         << reinterpret_cast<const char *>(
        //             glGetString(GL_SHADING_LANGUAGE_VERSION));

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

        initializeGLObjects();
    }

    void BaseGLWidget::paintGL() {
        glClear(GL_COLOR_BUFFER_BIT);
        paintGLObjects();
    }

    void BaseGLWidget::resizeGL(int w, int h) { glViewport(0, 0, w, h); }
} // namespace ProGraphics
