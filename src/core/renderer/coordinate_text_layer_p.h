#pragma once
#include "coordinate_text_atlas_p.h"
#include <QGuiApplication>
#include <QOpenGLBuffer>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QVector4D>
#include <memory>

namespace ProGraphics {
// Qt 负责原有文字排版和栅格化；紧凑图集与一批文字四边形复用到下一次排版变化。
class CoordinateTextLayer final : protected QOpenGLFunctions {
public:
    explicit CoordinateTextLayer(QOpenGLWidget& widget) {
        m_cleanup=QObject::connect(widget.context(),&QOpenGLContext::aboutToBeDestroyed,
            &m_connectionOwner,[this,&widget] {
                widget.makeCurrent(); releaseGL(); widget.doneCurrent();
            });
    }
    ~CoordinateTextLayer() { QObject::disconnect(m_cleanup); }
    void invalidate() { m_dirty=true; }

    template<class Collect>
    bool render(QOpenGLWidget& widget,const QMatrix4x4& view,const QMatrix4x4& projection,
                Collect&& collect,bool& rebuilt) {
        rebuilt=false;
        if(!initialize()) return false;
        const QSize logicalSize=widget.size();
        const qreal dpr=widget.devicePixelRatioF();
        const QSize pixels=logicalSize*dpr;
        if(pixels.isEmpty()) return true;
        const int dpiX=widget.logicalDpiX(),dpiY=widget.logicalDpiY();
        const QFont appFont=QGuiApplication::font();
        if(m_dirty || !m_hasLayout || logicalSize!=m_logicalSize || dpr!=m_dpr ||
           dpiX!=m_dpiX || dpiY!=m_dpiY || appFont!=m_appFont || view!=m_view || projection!=m_projection) {
            if(m_probe.isNull()) m_probe=QImage(1,1,QImage::Format_RGBA8888_Premultiplied);
            m_probe.setDevicePixelRatio(dpr);
            m_probe.setDotsPerMeterX(qRound(dpiX/0.0254)); m_probe.setDotsPerMeterY(qRound(dpiY/0.0254));
            m_labels.clear();
            { QPainter painter(&m_probe); collect(painter,m_labels); }
            if(!m_atlas.rebuild(m_labels,pixels,dpr,dpiX,dpiY,m_maxTextureSize)) return false;
            m_vertices.clear();
            if(!m_atlas.quads().isEmpty()) {
                const auto& image=m_atlas.image();
                if(!m_texture || m_texture->width()!=image.width() || m_texture->height()!=image.height()) {
                    auto texture=std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
                    if(!texture->create()) return false;
                    texture->setFormat(QOpenGLTexture::RGBA8_UNorm);
                    texture->setSize(image.width(),image.height()); texture->setMipLevels(1);
                    texture->setAutoMipMapGenerationEnabled(false);
                    texture->allocateStorage(QOpenGLTexture::RGBA,QOpenGLTexture::UInt8);
                    if(!texture->isStorageAllocated()) return false;
                    texture->setMinMagFilters(QOpenGLTexture::Nearest,QOpenGLTexture::Nearest);
                    texture->setWrapMode(QOpenGLTexture::ClampToEdge); m_texture=std::move(texture);
                }
                m_texture->setData(QOpenGLTexture::RGBA,QOpenGLTexture::UInt8,image.constBits());
                m_vertices.reserve(m_atlas.quads().size()*6);
                for(const auto& quad:m_atlas.quads()) {
                    const auto& target=quad.targetPixels; const auto& source=quad.sourcePixels;
                    const float l=float(target.x()),t=float(target.y()),r=l+target.width(),b=t+target.height();
                    const float u0=float(source.x())/image.width(),v0=float(source.y())/image.height();
                    const float u1=float(source.x()+source.width())/image.width(),v1=float(source.y()+source.height())/image.height();
                    // 屏幕 Y 向下，图像首行对应 v=0；三角形在裁剪空间保持逆时针。
                    m_vertices.append({l,t,u0,v0}); m_vertices.append({l,b,u0,v1}); m_vertices.append({r,t,u1,v0});
                    m_vertices.append({l,b,u0,v1}); m_vertices.append({r,b,u1,v1}); m_vertices.append({r,t,u1,v0});
                }
                m_buffer.bind();
                const int bytes=int(m_vertices.size()*sizeof(QVector4D));
                if(bytes>m_bufferCapacity) { m_buffer.allocate(bytes); m_bufferCapacity=bytes; }
                m_buffer.write(0,m_vertices.constData(),bytes); m_buffer.release();
            }
            m_logicalSize=logicalSize; m_dpr=dpr; m_dpiX=dpiX; m_dpiY=dpiY;
            m_appFont=appFont; m_view=view; m_projection=projection;
            m_dirty=false; m_hasLayout=true; rebuilt=true;
        }
        if(m_vertices.isEmpty()) return true;
        glDisable(GL_DEPTH_TEST); glDisable(GL_SCISSOR_TEST); glDisable(GL_STENCIL_TEST);
        glDepthMask(GL_FALSE); glEnable(GL_BLEND); glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
        m_program->bind(); m_program->setUniformValue("viewport",QVector2D(pixels.width(),pixels.height()));
        m_program->setUniformValue("atlas",0);
        m_texture->bind(0); m_vao.bind();
        glDrawArrays(GL_TRIANGLES,0,int(m_vertices.size()));
        m_vao.release(); m_texture->release(0); m_program->release();
        glDepthMask(GL_TRUE); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        return true;
    }
private:
    bool initialize() {
        if(m_initialized) return true;
        if(m_failed) return false;
        initializeOpenGLFunctions();
        m_program=std::make_unique<QOpenGLShaderProgram>();
        const char* vertex=R"(#version 410 core
            layout(location=0) in vec4 vertex;
            uniform vec2 viewport; out vec2 uv;
            void main() { gl_Position=vec4(2*vertex.x/viewport.x-1,1-2*vertex.y/viewport.y,0,1); uv=vertex.zw; }
        )";
        const char* fragment=R"(#version 410 core
            uniform sampler2D atlas; in vec2 uv; out vec4 color;
            void main() { color=texture(atlas,uv); }
        )";
        if(!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex,vertex) ||
           !m_program->addShaderFromSourceCode(QOpenGLShader::Fragment,fragment) || !m_program->link() ||
           !m_vao.create() || !m_buffer.create()) { m_failed=true; return false; }
        m_vao.bind(); m_buffer.bind(); m_buffer.setUsagePattern(QOpenGLBuffer::StreamDraw);
        m_program->enableAttributeArray(0); m_program->setAttributeBuffer(0,GL_FLOAT,0,4,sizeof(QVector4D));
        m_buffer.release(); m_vao.release();
        glGetIntegerv(GL_MAX_TEXTURE_SIZE,&m_maxTextureSize); m_initialized=true; return true;
    }
    void releaseGL() {
        m_texture.reset(); m_buffer.destroy(); m_vao.destroy(); m_program.reset();
        m_bufferCapacity=0; m_initialized=false; m_failed=false; m_dirty=true; m_hasLayout=false;
    }
    QObject m_connectionOwner;
    QMetaObject::Connection m_cleanup;
    std::unique_ptr<QOpenGLShaderProgram> m_program;
    QOpenGLBuffer m_buffer{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject m_vao;
    std::unique_ptr<QOpenGLTexture> m_texture;
    CoordinateTextAtlas m_atlas;
    QImage m_probe;
    QVector<TextRenderer::ScreenLabel> m_labels;
    QVector<QVector4D> m_vertices;
    QSize m_logicalSize;
    qreal m_dpr=0;
    int m_dpiX=0,m_dpiY=0,m_bufferCapacity=0;
    GLint m_maxTextureSize=0;
    QFont m_appFont;
    QMatrix4x4 m_view,m_projection;
    bool m_dirty=true,m_initialized=false,m_failed=false,m_hasLayout=false;
};
} // namespace ProGraphics
