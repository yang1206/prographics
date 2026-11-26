#pragma once
#include "prographics/core/renderer/text_renderer.h"
#include <QImage>
#include <cmath>

namespace ProGraphics {
// 只缓存文字实际覆盖的区域；所有目标位置仍使用原来的 Qt 屏幕排版。
class CoordinateTextAtlas {
public:
    struct Quad { QRect targetPixels,sourcePixels; qsizetype labelIndex; };
    const QImage& image() const { return m_image; }
    const QVector<Quad>& quads() const { return m_quads; }

    bool rebuild(const QVector<TextRenderer::ScreenLabel>& labels,QSize framePixels,
                 double dpr,int dpiX,int dpiY,int maxTextureSize) {
        m_quads.clear();
        if(!std::isfinite(dpr) || dpr<=0 || maxTextureSize<=0) return false;
        int widest=1;
        for(qsizetype i=0;i<labels.size();++i) {
            if(labels[i].text.isEmpty()) continue;
            const QRectF ink=QRectF(labels[i].bounds).translated(labels[i].baseline).adjusted(-2,-2,2,2);
            const QRect target=QRectF(ink.topLeft()*dpr,ink.size()*dpr).toAlignedRect().intersected(QRect(QPoint(),framePixels));
            if(target.isEmpty()) continue;
            if(target.width()>maxTextureSize || target.height()>maxTextureSize) return false;
            widest=std::max(widest,target.width());
            m_quads.append({target,{},i});
        }
        if(m_quads.isEmpty()) return true;
        const auto powerOfTwo=[maxTextureSize](int needed) {
            int size=1;
            while(size<needed && size<=maxTextureSize/2) size*=2;
            return size<needed?maxTextureSize:size;
        };
        int width=std::max(powerOfTwo(std::max(widest,std::min(256,maxTextureSize))),m_image.width());
        width=std::min(width,maxTextureSize);
        int usedHeight=0;
        for(;;) {
            int x=0,y=0,rowHeight=0;
            for(auto& quad:m_quads) {
                const auto size=quad.targetPixels.size();
                if(x+size.width()>width) { x=0; y+=rowHeight+1; rowHeight=0; }
                quad.sourcePixels=QRect(QPoint(x,y),size);
                x+=size.width()+1; rowHeight=std::max(rowHeight,size.height());
            }
            usedHeight=y+rowHeight;
            if(usedHeight<=maxTextureSize) break;
            if(width==maxTextureSize) return false;
            width=std::min(width*2,maxTextureSize);
        }
        const int height=std::min(maxTextureSize,std::max(powerOfTwo(usedHeight),m_image.height()));
        if(m_image.size()!=QSize(width,height)) m_image=QImage(width,height,QImage::Format_RGBA8888_Premultiplied);
        if(m_image.isNull()) return false;
        m_image.setDevicePixelRatio(dpr);
        m_image.setDotsPerMeterX(qRound(dpiX/0.0254)); m_image.setDotsPerMeterY(qRound(dpiY/0.0254));
        m_image.fill(Qt::transparent);
        QPainter painter(&m_image);
        painter.setRenderHint(QPainter::Antialiasing); painter.setRenderHint(QPainter::TextAntialiasing);
        for(const auto& quad:m_quads) {
            const auto& label=labels[quad.labelIndex];
            const QRect& source=quad.sourcePixels;
            const QPointF shift=QPointF(source.topLeft()-quad.targetPixels.topLeft())/dpr;
            painter.save();
            painter.setClipRect(QRectF(QPointF(source.topLeft())/dpr,QSizeF(source.size())/dpr));
            painter.setFont(label.font); painter.setPen(label.color);
            // 以整数设备像素平移，保留原始基线的小数部分，避免 150% DPI 的字形相位变化。
            painter.translate(shift);
            painter.drawText(label.baseline,label.text);
            painter.restore();
        }
        return true;
    }
private:
    QImage m_image;
    QVector<Quad> m_quads;
};
} // namespace ProGraphics
