#pragma once

#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QMatrix4x4>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QOpenGLTextureBlitter>
#include <QOpenGLWidget>
#include <QPainter>
#include <memory>
#include <utility>

namespace ProGraphics {

// 坐标文字仍用原来的 QPainter 排版，只在内容或投影变化时重画并上传。
// 稳定帧直接混合预乘透明纹理，避免在每张 QOpenGLWidget 上反复进入文字绘制引擎。
class CoordinateTextLayer final : protected QOpenGLFunctions {
public:
    explicit CoordinateTextLayer(QOpenGLWidget& widget) {
        m_cleanup = QObject::connect(widget.context(), &QOpenGLContext::aboutToBeDestroyed,
            &m_connectionOwner, [this, &widget] {
                widget.makeCurrent();
                m_texture.reset();
                m_blitter.destroy();
                m_initialized = false;
                m_dirty = true;
                widget.doneCurrent();
            });
    }
    ~CoordinateTextLayer() { QObject::disconnect(m_cleanup); }

    void invalidate() { m_dirty = true; }

    template<class Paint>
    bool render(QOpenGLWidget& widget, const QMatrix4x4& view, const QMatrix4x4& projection,
                Paint&& paint, bool& rebuilt) {
        rebuilt = false;
        if (!initialize()) return false;
        const QSize logicalSize = widget.size();
        const qreal dpr = widget.devicePixelRatioF();
        // 与 Qt 的 QSize 高 DPI 换算采用相同的取整方式，不额外缩放文字。
        const QSize pixels = logicalSize * dpr;
        if (pixels.isEmpty()) return true;
        if (pixels.width() > m_maxTextureSize || pixels.height() > m_maxTextureSize) return false;
        const int dpiX = widget.logicalDpiX(), dpiY = widget.logicalDpiY();
        const QFont appFont = QGuiApplication::font();
        if (m_dirty || !m_texture || logicalSize != m_logicalSize || dpr != m_dpr ||
            dpiX != m_dpiX || dpiY != m_dpiY || appFont != m_appFont ||
            view != m_view || projection != m_projection) {
            QImage image(pixels, QImage::Format_RGBA8888_Premultiplied);
            if (image.isNull()) return false;
            image.setDevicePixelRatio(dpr);
            image.setDotsPerMeterX(qRound(dpiX / 0.0254));
            image.setDotsPerMeterY(qRound(dpiY / 0.0254));
            image.fill(Qt::transparent);
            {
                QPainter painter(&image);
                painter.setRenderHint(QPainter::Antialiasing);
                paint(painter);
            }
            if (!m_texture || m_texture->width() != pixels.width() || m_texture->height() != pixels.height()) {
                auto texture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
                if (!texture->create()) return false;
                texture->setFormat(QOpenGLTexture::RGBA8_UNorm);
                texture->setSize(pixels.width(), pixels.height());
                texture->setMipLevels(1);
                texture->setAutoMipMapGenerationEnabled(false);
                texture->allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
                if (!texture->isStorageAllocated()) return false;
                texture->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
                texture->setWrapMode(QOpenGLTexture::ClampToEdge);
                m_texture = std::move(texture);
            }
            m_texture->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, image.constBits());
            m_logicalSize = logicalSize;
            m_dpr = dpr;
            m_dpiX = dpiX; m_dpiY = dpiY;
            m_appFont = appFont;
            m_view = view; m_projection = projection;
            m_dirty = false;
            rebuilt = true;
        }

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        m_blitter.bind();
        // QImage 首行在上方，使用 Qt 的顶端原点变换保持文字方向。
        m_blitter.blit(m_texture->textureId(), QMatrix4x4(), QOpenGLTextureBlitter::OriginTopLeft);
        m_blitter.release();
        glDepthMask(GL_TRUE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        return true;
    }

private:
    bool initialize() {
        if (m_initialized) return true;
        if (m_failed) return false;
        initializeOpenGLFunctions();
        if (!m_blitter.create()) {
            m_failed = true;
            return false;
        }
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_maxTextureSize);
        m_initialized = true;
        return true;
    }

    QObject m_connectionOwner;
    QMetaObject::Connection m_cleanup;
    QOpenGLTextureBlitter m_blitter;
    std::unique_ptr<QOpenGLTexture> m_texture;
    QSize m_logicalSize;
    qreal m_dpr = 0;
    int m_dpiX = 0, m_dpiY = 0;
    GLint m_maxTextureSize = 0;
    QFont m_appFont;
    QMatrix4x4 m_view, m_projection;
    bool m_dirty = true, m_initialized = false, m_failed = false;
};

} // namespace ProGraphics
