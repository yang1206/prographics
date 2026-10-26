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
#include <array>
#include <cstddef>

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

    // 诊断开关不改变数据接收和统计，默认绘制全部层并启用文字缓存。
    struct RenderOptions {
      bool geometry = true;
      bool text = true;
      bool data = true;
      bool referenceLines = true;
      bool cachedText = true;
      bool operator==(const RenderOptions& other) const {
        return geometry == other.geometry && text == other.text && data == other.data &&
            referenceLines == other.referenceLines && cachedText == other.cachedText;
      }
    };
    enum class RenderStage { Geometry, Text, Data, ReferenceLines };
    struct FrameRenderMetrics {
      std::array<qint64, 4> cpuNs{};
      std::array<bool, 4> measured{};
      bool textCacheUsed = false;
      bool textCacheRebuilt = false;
    };
    void setRenderOptions(const RenderOptions& options) {
      if (m_renderOptions == options) return;
      m_renderOptions = options;
      update();
    }
    const RenderOptions& renderOptions() const { return m_renderOptions; }
    void setRenderProfilingEnabled(bool enabled) {
      m_renderProfiling = enabled;
      m_frameRenderMetrics = {};
    }
    const FrameRenderMetrics& lastFrameRenderMetrics() const { return m_frameRenderMetrics; }

  protected:
    class RenderStageScope {
    public:
      RenderStageScope(BaseGLWidget& owner, RenderStage stage)
          : m_owner(owner), m_stage(static_cast<size_t>(stage)), m_enabled(owner.m_renderProfiling) {
        if (m_enabled) m_clock.start();
      }
      ~RenderStageScope() {
        if (!m_enabled) return;
        m_owner.m_frameRenderMetrics.cpuNs[m_stage] += m_clock.nsecsElapsed();
        m_owner.m_frameRenderMetrics.measured[m_stage] = true;
      }
      RenderStageScope(const RenderStageScope&) = delete;
      RenderStageScope& operator=(const RenderStageScope&) = delete;
    private:
      BaseGLWidget& m_owner;
      size_t m_stage;
      bool m_enabled;
      QElapsedTimer m_clock;
    };
    RenderStageScope measureRenderStage(RenderStage stage) { return RenderStageScope(*this, stage); }
    void recordTextCacheState(bool used, bool rebuilt) {
      if (!m_renderProfiling) return;
      m_frameRenderMetrics.textCacheUsed = used;
      m_frameRenderMetrics.textCacheRebuilt = rebuilt;
    }

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

  private:
    RenderOptions m_renderOptions;
    bool m_renderProfiling = false;
    FrameRenderMetrics m_frameRenderMetrics;
  };
} // namespace ProGraphics
