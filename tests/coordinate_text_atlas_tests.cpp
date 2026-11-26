#include "core/renderer/coordinate_text_atlas_p.h"
#include "prographics/core/renderer/text_renderer.h"
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <cstdlib>
#include <iostream>

#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while(false)
using namespace ProGraphics;

QImage canvas(QSize pixels,double dpr,int dpi) {
    QImage image(pixels,QImage::Format_RGBA8888_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.setDotsPerMeterX(qRound(dpi/0.0254)); image.setDotsPerMeterY(qRound(dpi/0.0254));
    image.fill(Qt::transparent); return image;
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    for(double dpr:{1.0,1.5,2.0}) for(int dpi:{96,120}) {
        const QSize logical(640,360),physical=logical*dpr;
        TextRenderer renderer;
        TextRenderer::TextStyle style; style.fontSize=12;
        auto* first=renderer.addLabel("0.0",{-0.8f,-0.72f,0},style);
        renderer.addLabel("-12.50",{0.0237f,-0.731f,0},style);
        renderer.addLabel(QString::fromUtf8("相位 (°)"),{0,0.8f,0},style);
        style.bold=true; style.color=QColor(255,130,50,170);
        renderer.addLabel(QString::fromUtf8("幅值 (mV)"),{-0.7f,0.5f,0},style);
        style.italic=true; style.bold=false;
        auto* edge=renderer.addLabel("jfy",{-1,0.2f,0},style); renderer.setAlignment(edge,Qt::AlignRight);
        renderer.addLabel("end",{1,-0.1f,0},style);
        QMatrix4x4 matrix;
        auto expected=canvas(physical,dpr,dpi);
        { QPainter p(&expected); p.setRenderHint(QPainter::Antialiasing); renderer.render(p,matrix,matrix,logical.width(),logical.height()); }
        auto probe=canvas({1,1},dpr,dpi);
        QVector<TextRenderer::ScreenLabel> labels;
        { QPainter p(&probe); renderer.collect(p,matrix,matrix,logical.width(),logical.height(),labels); }
        CoordinateTextAtlas atlas;
        CHECK(atlas.rebuild(labels,physical,dpr,dpi,dpi,8192));
        CHECK(atlas.image().sizeInBytes()<expected.sizeInBytes()/4);
        QImage actual(physical,QImage::Format_RGBA8888_Premultiplied); actual.fill(Qt::transparent);
        { QPainter p(&actual); for(const auto& quad:atlas.quads()) {
            auto tile=atlas.image().copy(quad.sourcePixels); tile.setDevicePixelRatio(1);
            p.drawImage(quad.targetPixels.topLeft(),tile);
        } }
        int maxDifference=0;
        for(int y=0;y<physical.height();++y) for(int x=0;x<physical.width();++x) {
            const auto a=actual.pixelColor(x,y),b=expected.pixelColor(x,y);
            maxDifference=std::max({maxDifference,std::abs(a.red()-b.red()),std::abs(a.green()-b.green()),
                                   std::abs(a.blue()-b.blue()),std::abs(a.alpha()-b.alpha())});
        }
        if(maxDifference>1) {
            actual.save("atlas-actual.png"); expected.save("atlas-expected.png"); atlas.image().save("atlas-packed.png");
            for(const auto& label:labels) std::cerr<<label.text.toStdString()<<" baseline="<<label.baseline.x()<<","<<label.baseline.y()
                <<" bounds="<<label.bounds.x()<<","<<label.bounds.y()<<","<<label.bounds.width()<<","<<label.bounds.height()<<'\n';
            for(const auto& q:atlas.quads()) std::cerr<<"quad "<<q.targetPixels.x()<<","<<q.targetPixels.y()<<","<<q.targetPixels.width()<<","<<q.targetPixels.height()
                <<" <- "<<q.sourcePixels.x()<<","<<q.sourcePixels.y()<<'\n';
        }
        CHECK(maxDifference<=1); // Crop/packing must preserve glyph pixels, including fractional DPI and edge clipping.
        const auto* storage=atlas.image().constBits();
        renderer.updateLabel(first,"1.0"); labels.clear();
        { QPainter p(&probe); renderer.collect(p,matrix,matrix,logical.width(),logical.height(),labels); }
        CHECK(atlas.rebuild(labels,physical,dpr,dpi,dpi,8192));
        CHECK(atlas.image().constBits()==storage); // Equal-size text updates retain the raster allocation.
        CHECK(atlas.rebuild({},physical,dpr,dpi,dpi,8192)); CHECK(atlas.quads().isEmpty());
        std::cout<<"DPR "<<dpr<<" DPI "<<dpi<<": glyph pixels preserved; compact raster storage reused\n";
    }
}
