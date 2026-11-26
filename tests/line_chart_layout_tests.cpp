#include "prographics/charts/line/line_chart.h"
#include "prographics/charts/prpd/prpd.h"
#include "prographics/charts/prps/prps.h"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QRandomGenerator>
#include <QThread>
#include <QWheelEvent>
#include <cmath>
#include <cstdlib>
#include <iostream>

using Chart=ProGraphics::LineChart;
#define CHECK(x) do { if(!(x)) { std::cerr<<__LINE__<<": "<<#x<<'\n'; std::exit(1); } } while(false)
class LayoutProbe:public Chart {
public:
    QVector<ProGraphics::TextRenderer::ScreenLabel> labels() {
        QImage image(1,1,QImage::Format_RGBA8888_Premultiplied); QPainter painter(&image);
        QVector<ProGraphics::TextRenderer::ScreenLabel> result; collectCoordinateText(painter,result); return result;
    }
};
void checkLayout(LayoutProbe& chart,bool full=true) {
    const auto labels=chart.labels();
    if(full) CHECK(labels.size()>=6);
    QVector<QRectF> occupied;
    for(const auto& label:labels) {
        const auto ink=QRectF(label.bounds).translated(label.baseline);
        CHECK(QRectF(chart.rect()).contains(ink));
        CHECK(!ink.intersects(chart.plotRect().adjusted(-0.5,-0.5,0.5,0.5)));
        for(const auto& previous:occupied) CHECK(!previous.intersects(ink));
        occupied.append(ink);
    }
    CHECK(chart.plotRect().width()>0 && chart.plotRect().height()>0);
}
void settle() { QElapsedTimer t; t.start(); while(t.elapsed()<80) { QApplication::processEvents(); QThread::msleep(1); } }
void style(Chart& chart,int pointSize) {
    auto cfg=chart.config();
    ProGraphics::TextRenderer::TextStyle text; text.fontFamily="Microsoft YaHei"; text.fontSize=pointSize; text.color=QColor("#53627c");
    cfg.ticks.x.style=cfg.ticks.y.style=cfg.names.x.style=cfg.names.y.style=text;
    cfg.axis.x.color=cfg.axis.y.color=QVector4D(0.32f,0.38f,0.48f,1);
    cfg.grid.xy.majorColor=QVector4D(0.6f,0.68f,0.78f,0.65f);
    chart.setConfig(cfg); chart.setBackgroundColor(QColor("#f8fafc"));
    chart.setAxisName('x',QString::fromUtf8("时间轴"),"ns"); chart.setAxisName('y',QString::fromUtf8("电压"),"mV");
    chart.setHoverEnabled(false);
}
QVector<Chart::Series> waveforms() {
    QRandomGenerator random(0x5eed1234); QVector<Chart::Series> result;
    for(int channel=0;channel<8;++channel) {
        Chart::Series s; s.id=QString::number(channel); s.color=QColor::fromHsv(channel*40,220,230); s.xStep=400.0/2047;
        for(int i=0;i<2048;++i) s.y.append((random.generateDouble()-0.5)*(channel==2?0.9:0.15+channel*0.035));
        s.y[384+channel*27]=0.7; result.append(s);
    }
    return result;
}
template<class T> void legacyCapture(const QString& directory,const QString& name) {
    T chart; chart.resize(800,450); auto options=chart.renderOptions(); options.data=false; options.referenceLines=false;
    chart.setRenderOptions(options); chart.show(); settle();
    CHECK(chart.isValid()); CHECK(chart.grabFramebuffer().save(directory+"/"+name+"-cached.png"));
    options.cachedText=false; chart.setRenderOptions(options); settle();
    CHECK(chart.grabFramebuffer().save(directory+"/"+name+"-painter.png"));
}
int main(int argc,char** argv) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QSurfaceFormat::setDefaultFormat(ProGraphics::chartSurfaceFormat());
    QApplication app(argc,argv); app.setQuitOnLastWindowClosed(false);
    if(argc>1) {
        const QString directory=QString::fromLocal8Bit(argv[1]); CHECK(QDir().mkpath(directory)); QJsonArray observations;
        struct Case { const char* name; QSize size; int font; Chart::Range x,y; };
        const Case cases[]={{"normal",{800,500},12,{0,400},{-0.4,1.2}},
            {"wide",{1200,240},12,{0,400},{-0.4,1.2}},
            {"large-font",{1200,280},18,{0,400},{-0.4,1.2}},
            {"long-labels",{1000,260},14,{1e12,1e12+0.002},{-0.00004,0.00006}},
            {"tiny",{180,100},18,{0,400},{-0.4,1.2}}};
        for(const auto& item:cases) {
            LayoutProbe chart; chart.resize(item.size); style(chart,item.font); CHECK(chart.setSeries(waveforms()));
            CHECK(chart.setViewRange(item.x,item.y)); chart.show(); settle(); CHECK(chart.isValid());
            CHECK(chart.grabFramebuffer().save(directory+"/"+item.name+".png"));
            checkLayout(chart,QString::fromLatin1(item.name)!="tiny");
            auto options=chart.renderOptions(); options.cachedText=false; chart.setRenderOptions(options);
            CHECK(chart.grabFramebuffer().save(directory+"/"+item.name+"-painter.png"));
            QJsonArray labels; for(const auto& label:chart.labels()) {
                const auto ink=QRectF(label.bounds).translated(label.baseline);
                labels.append(QJsonObject{{"text",label.text},{"rect",QJsonArray{ink.left(),ink.top(),ink.width(),ink.height()}}});
            }
            const auto plot=chart.plotRect(); observations.append(QJsonObject{{"name",item.name},{"dpr",chart.devicePixelRatioF()},
                {"dpi",chart.logicalDpiX()},{"size",QJsonArray{chart.width(),chart.height()}},
                {"plot",QJsonArray{plot.left(),plot.top(),plot.right(),plot.bottom()}},{"labels",labels}});
        }
        legacyCapture<ProGraphics::PRPDChart>(directory,"prpd"); legacyCapture<ProGraphics::PRPSChart>(directory,"prps");
        QFile file(directory+"/layout.json"); CHECK(file.open(QIODevice::WriteOnly)); file.write(QJsonDocument(observations).toJson());
    }
    LayoutProbe chart; chart.resize(900,300); style(chart,12); CHECK(chart.setViewRange({0,400},{-0.4,1.2}));
    const auto narrow=chart.plotRect(); chart.resize(1800,300); CHECK(chart.setXRange({0,400}));
    chart.labels(); // Unshown widgets defer resizeEvent; prepare the same layout used immediately before painting.
    const auto wide=chart.plotRect();
    if(std::abs(narrow.left()-wide.left())>=2) std::cerr<<"left gutters: "<<narrow.left()<<", "<<wide.left()<<'\n';
    CHECK(std::abs(narrow.left()-wide.left())<2); // A wider waveform must not double the Y-label gutter.
    CHECK(wide.left()<100); // Four short Y digits require a text gutter, not 14% of a wide widget.
    checkLayout(chart);
    const double originalLeft=chart.plotRect().left(); style(chart,20); checkLayout(chart);
    CHECK(chart.plotRect().left()>originalLeft); // Font setters must update layout before the next paint.
    CHECK(chart.setYRange({-1e8,1e8})); checkLayout(chart);
    CHECK(chart.plotRect().left()>originalLeft+20);
    const auto titled=chart.plotRect(); chart.setAxisNameEnabled(false); checkLayout(chart);
    CHECK(chart.plotRect().height()>titled.height());
    chart.setAxisNameEnabled(true);
    for(const auto size:{QSize(200,120),QSize(60,40),QSize(1,1),QSize(800,500)}) {
        chart.resize(size); CHECK(chart.setXRange({0,401})); checkLayout(chart,size.width()>=300);
    }
    QFont italic("Times New Roman",16); italic.setItalic(true);
    const auto aligned=ProGraphics::TextRenderer::alignedScreenLabel("jfy",italic,Qt::black,{100,200},Qt::AlignRight|Qt::AlignTop,&chart);
    const auto ink=QRectF(aligned.bounds).translated(aligned.baseline);
    CHECK(std::abs(ink.right()-100)<0.01 && std::abs(ink.top()-200)<0.01);
    LayoutProbe interaction; interaction.resize(1078,559);
    for(const auto modifiers:{Qt::ShiftModifier,Qt::ControlModifier,Qt::NoModifier}) {
        CHECK(interaction.setViewRange({0,20.47},{-1,20})); interaction.labels();
        const QPointF data(5,8),pixel=interaction.mapFromChart(data).value();
        QWheelEvent wheel(pixel,interaction.mapToGlobal(pixel),{},{0,120},{},modifiers,Qt::NoScrollPhase,false);
        QApplication::sendEvent(&interaction,&wheel);
        const auto projected=interaction.mapFromChart(data).value();
        if(modifiers!=Qt::ControlModifier) CHECK(std::abs(projected.x()-pixel.x())<0.001);
        else CHECK(interaction.xRange().min==0 && interaction.xRange().max==20.47);
        if(modifiers!=Qt::ShiftModifier) CHECK(std::abs(projected.y()-pixel.y())<0.001);
        else CHECK(interaction.yRange().min==-1 && interaction.yRange().max==20);
        checkLayout(interaction);
    }
    CHECK(interaction.setViewRange({-10,10},{-10,10}));
    const QPointF grabbed(0,0),start=interaction.mapFromChart(grabbed).value();
    const auto sendMouse=[&](QEvent::Type type,QPointF pixel,Qt::MouseButton button,Qt::MouseButtons buttons) {
        QMouseEvent event(type,pixel,interaction.mapToGlobal(pixel),button,buttons,Qt::NoModifier);
        QApplication::sendEvent(&interaction,&event);
    };
    sendMouse(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
    for(const QPointF delta:{QPointF(350,60),QPointF(-100,-80),QPointF(15,0)}) {
        sendMouse(QEvent::MouseMove,start+delta,Qt::NoButton,Qt::LeftButton);
        CHECK((interaction.mapFromChart(grabbed).value()-start-delta).manhattanLength()<0.001);
        checkLayout(interaction);
    }
    sendMouse(QEvent::MouseButtonRelease,start,Qt::LeftButton,{});
    QRandomGenerator random(1978);
    for(int i=0;i<80;++i) {
        const double magnitude=std::pow(10.0,int(random.bounded(9u))-4);
        const double x=(random.generateDouble()-0.5)*10*magnitude,y=(random.generateDouble()-0.5)*10*magnitude;
        CHECK(interaction.setViewRange({x,x+magnitude},{y,y+magnitude}));
        const QPointF data(x+magnitude*0.3,y+magnitude*0.6),pixel=interaction.mapFromChart(data).value();
        QWheelEvent wheel(pixel,interaction.mapToGlobal(pixel),{},{0,120},{},{},Qt::NoScrollPhase,false);
        QApplication::sendEvent(&interaction,&wheel);
        CHECK((interaction.mapFromChart(data).value()-pixel).manhattanLength()<0.001);
        checkLayout(interaction);
    }
    std::cout<<"LineChart pixel gutters, glyph baselines, all-edge clearance, font/range changes and tiny-widget checks passed\n";
}
