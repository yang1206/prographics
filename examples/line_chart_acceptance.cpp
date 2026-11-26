#include "line_chart_demo.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFunctions_4_1_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <QScreen>
#include <QSpinBox>
#include <QSysInfo>
#include <QTimer>
#include <QWheelEvent>
#include <QWindow>
#include <algorithm>
#include <cmath>

namespace {
using Chart=ProGraphics::LineChart;
class StandaloneLineProbe final : public Chart {
public:
    GLenum error=GL_NO_ERROR;
protected:
    void paintGL() override {
        Chart::paintGL();
        auto* gl=context()->functions();
        for(GLenum next=gl->glGetError();next!=GL_NO_ERROR;next=gl->glGetError())
            if(error==GL_NO_ERROR) error=next;
    }
};
void mouse(Chart* c,QEvent::Type type,QPointF p,Qt::MouseButton button,Qt::MouseButtons buttons,Qt::KeyboardModifiers mods={}) {
    QMouseEvent event(type,p,c->mapToGlobal(p),button,buttons,mods); QApplication::sendEvent(c,&event);
}
void wheel(Chart* c,QPointF p,int delta,Qt::KeyboardModifiers mods={}) {
    QWheelEvent event(p,c->mapToGlobal(p),{},{0,delta},{},mods,Qt::NoScrollPhase,false); QApplication::sendEvent(c,&event);
}
QJsonObject distribution(QVector<double> values) {
    if (values.isEmpty()) return {};
    std::sort(values.begin(),values.end()); double sum=0; for(double v:values) sum+=v;
    return {{"count",values.size()},{"meanMs",sum/values.size()},
            {"p95Ms",values[qsizetype(std::ceil(values.size()*0.95))-1]},{"maxMs",values.last()}};
}
class Acceptance final : public QObject {
public:
    Acceptance(QWidget* demo,QString path) : QObject(demo),window(demo),reportPath(std::move(path)) {
        chart=dynamic_cast<DemoLineChart*>(demo->findChild<Chart*>("lineChart"));
        chart->setRenderProfilingEnabled(true);
        running=demo->findChild<QCheckBox*>("lineRunning");
        chart->installEventFilter(this);
        connect(chart,&Chart::viewRangeChanged,this,[this] {
            if(measuring && phase=="continuous_8x2048") {
                const auto x=chart->xRange(),y=chart->yRange();
                steadyViewChanges.append(QJsonObject{{"elapsedMs",clock.nsecsElapsed()/1e6-phaseStart},
                    {"xMin",x.min},{"xMax",x.max},{"yMin",y.min},{"yMax",y.max}});
            }
        });
        clock.start();
        chart->paintMeasured=[this](double ms) {
            if(measuring) {
                paints.append(ms);
                const auto& measured=chart->lastFrameRenderMetrics();
                geometryTimes.append(measured.cpuNs[0]/1e6);
                textTimes.append(measured.cpuNs[1]/1e6);
                dataTimes.append(measured.cpuNs[2]/1e6);
                double now=clock.nsecsElapsed()/1e6;
                if(lastPaint>0) paintIntervals.append(now-lastPaint);
                lastPaint=now;
                if(ms>10) {
                    const auto& stages=chart->lastFrameRenderMetrics();
                    slowPaints.append(QJsonObject{{"elapsedMs",now-phaseStart},{"cpuMs",ms},
                        {"geometryMs",stages.cpuNs[0]/1e6},{"textMs",stages.cpuNs[1]/1e6},
                        {"dataMs",stages.cpuNs[2]/1e6},{"textCacheRebuilt",stages.textCacheRebuilt}});
                }
            }
        };
        connect(chart,&QOpenGLWidget::frameSwapped,this,[this] {
            double now=clock.nsecsElapsed()/1e6;
            if(measuring && lastFrame>0) {
                const double dt=now-lastFrame; intervals.append(dt);
                if(dt>25) longFrames.append(QJsonObject{{"elapsedMs",now-phaseStart},{"intervalMs",dt}});
            }
            lastFrame=now;
        });
        interactions.setInterval(16); interactions.setTimerType(Qt::PreciseTimer);
        connect(&interactions,&QTimer::timeout,this,[this] { interact(); });
        QTimer::singleShot(1200,this,[this] { functional(); });
    }
private:
    bool eventFilter(QObject* watched,QEvent* event) override {
        if(watched==chart && measuring && event->spontaneous() &&
           (event->type()==QEvent::Wheel || event->type()==QEvent::MouseButtonPress || event->type()==QEvent::KeyPress))
            nativeInputs.append(QJsonObject{{"phase",phase},{"elapsedMs",clock.nsecsElapsed()/1e6-phaseStart},{"eventType",int(event->type())}});
        return QObject::eventFilter(watched,event);
    }
    void check(bool pass,QString name) {
        checks.append(QJsonObject{{"name",name},{"passed",pass}}); okay=okay && pass;
        if(!pass) qWarning().noquote()<<"LineChart acceptance failed:"<<name;
    }
    static bool near(double a,double b) { return std::isfinite(a) && std::abs(a-b)<1e-6; }
    void functional() {
        const auto* rate=window->findChild<QSpinBox*>("lineRate");
        check(rate!=nullptr,"demo provides an adjustable data generation rate");
        check(chart->series().first().y!=makeLineDemoSeries(false,0).first().y,"default demo generates nonperiodic random waveforms");
        if(!rate) { finish(); return; }
        window->grab().save(reportPath+".random.png");
        const auto firstRandomFrame=chart->series().first().y;
        auto* source=window->findChild<QComboBox*>("lineSource");
        source->setCurrentIndex(1);
        const auto& randomX=chart->series().first().x;
        check(randomX.size()==2048 && std::adjacent_find(randomX.cbegin(),randomX.cend(),
              [](double a,double b) { return a>=b; })==randomX.cend(),"random nonuniform X remains strictly sorted");
        check(firstRandomFrame!=chart->series().first().y,"successive random frames contain different raw samples");
        source->setCurrentIndex(0);
        check(chart && chart->isValid(),"real OpenGL widget initialized");
        if(!chart->isValid()) { finish(); return; }
        auto* presentation=window->findChild<QComboBox*>("linePresentation");
        check(presentation!=nullptr,"demo provides wide and large-font layouts");
        if(presentation) {
            const auto x=chart->xRange(),y=chart->yRange();
            const auto samples=chart->series().first().y;
            for(int index:{1,2}) {
                presentation->setCurrentIndex(index); QApplication::processEvents();
                chart->grabFramebuffer();
                check(chart->plotRect().width()>3*chart->plotRect().height(),"wide demo layout: "+QString::number(index));
                check(near(chart->xRange().min,x.min) && near(chart->xRange().max,x.max) &&
                      near(chart->yRange().min,y.min) && near(chart->yRange().max,y.max) && chart->series().first().y==samples,
                      "layout switch preserves view and raw samples: "+QString::number(index));
                window->grab().save(reportPath+(index==1?".wide.png":".large-font.png"));
            }
            check(chart->config().ticks.y.style.fontSize==16,"large-font demo changes chart text size");
            presentation->setCurrentIndex(0); QApplication::processEvents(); chart->grabFramebuffer();
        }
        chart->makeCurrent(); auto* f=chart->context()->functions();
        environment={{"os",QSysInfo::prettyProductName()},{"cpu",QSysInfo::currentCpuArchitecture()},
            {"qt",qVersion()},{"build",PROGRAPHICS_EXAMPLE_BUILD_TYPE},
            {"glVendor",reinterpret_cast<const char*>(f->glGetString(GL_VENDOR))},
            {"glRenderer",reinterpret_cast<const char*>(f->glGetString(GL_RENDERER))},
            {"glVersion",reinterpret_cast<const char*>(f->glGetString(GL_VERSION))},
            {"dpr",chart->devicePixelRatioF()},{"screens",QGuiApplication::screens().size()},
            {"scaleFactorEnv",qEnvironmentVariable("QT_SCALE_FACTOR")},
            {"screenScaleFactorsEnv",qEnvironmentVariable("QT_SCREEN_SCALE_FACTORS")},
            {"screenHz",chart->screen()->refreshRate()},{"randomSeed","0x5eed1234"}};
        chart->doneCurrent();
        viewportDrawing();
        {
            StandaloneLineProbe standalone; standalone.resize(720,420);
            auto options=standalone.renderOptions(); options.geometry=false; options.text=false;
            standalone.setRenderOptions(options);
            Chart::Series series; series.id="standalone"; series.y={0,1,0};
            standalone.setSeries({series}); standalone.show(); standalone.grabFramebuffer();
            standalone.makeCurrent(); GLint flags=0;
            standalone.context()->functions()->glGetIntegerv(GL_CONTEXT_FLAGS,&flags);
            standalone.doneCurrent(); environment.insert("standaloneContextFlags",flags);
            check(standalone.error==GL_NO_ERROR,"standalone chart uses a valid line width in its actual GL context");
        }
        check(chart->series().size()==8 && chart->series()[0].y.size()==2048,"eight series of 2048 samples");
        chart->setViewRange({0,20.47},{-1,20});
        const QPointF anchor=chart->mapFromChart({5,8}).value();
        wheel(chart,anchor,120,Qt::ShiftModifier);
        check(near(chart->mapToChart(anchor)->x(),5) && near(chart->yRange().min,-1) && near(chart->yRange().max,20),"mouse anchored X-only wheel zoom");
        auto x=chart->xRange(); wheel(chart,anchor,120,Qt::ControlModifier);
        check(near(chart->mapToChart(anchor)->y(),8) && near(chart->xRange().min,x.min) && near(chart->xRange().max,x.max),"mouse anchored Y-only wheel zoom");
        const auto before=chart->mapToChart(anchor).value(); wheel(chart,anchor,120);
        auto after=chart->mapToChart(anchor).value();
        check(near(before.x(),after.x()) && near(before.y(),after.y()),"mouse anchored dual-axis zoom");
        auto y=chart->yRange(); x=chart->xRange(); QPointF p=chart->plotRect().center();
        mouse(chart,QEvent::MouseButtonPress,p,Qt::LeftButton,Qt::LeftButton,Qt::ShiftModifier);
        mouse(chart,QEvent::MouseMove,p+QPointF(40,30),Qt::NoButton,Qt::LeftButton,Qt::ShiftModifier);
        mouse(chart,QEvent::MouseButtonRelease,p+QPointF(40,30),Qt::LeftButton,{});
        check(chart->xRange().min<x.min && near(chart->yRange().min,y.min),"X-only drag pan");
        x=chart->xRange(); y=chart->yRange();
        chart->setSeries(makeLineDemoSeries(false,0.2));
        check(near(chart->xRange().min,x.min) && near(chart->yRange().max,y.max),"new frame preserves user view");
        chart->setSeries(makeLineDemoSeries(true,0)); chart->fitToData();
        const double spikeX=chart->series()[0].x[256];
        auto spike=chart->sampleAt(chart->mapFromChart({spikeX,1.8}).value(),2);
        check(spike && spike->index==256 && near(spike->x,spikeX) && near(spike->y,1.8),"irregular X spike reads original sample");
        chart->setSeriesVisible("CH1",false);
        check(!chart->sampleAt(chart->mapFromChart({spikeX,1.8}).value(),2),"hidden series is not pickable");
        const auto hiddenPixel=chart->mapFromChart({spikeX,1.8}).value();
        const auto retainedPixel=chart->mapFromChart({chart->series()[1].x[429],4.3}).value();
        QTimer::singleShot(80,this,[this,hiddenPixel,retainedPixel] {
            const auto image=chart->grabFramebuffer();
            check(!coloredAt(image,hiddenPixel,chart->series()[0].color),"hidden series disappears from real GL draw");
            const QColor color=chart->series()[1].color;
            check(coloredAt(image,retainedPixel,color),"visible series remains in real GL draw");
            chart->clear(); chart->fitToData();
            QTimer::singleShot(80,this,[this,retainedPixel,color] {
                check(chart->series().isEmpty() && !coloredAt(chart->grabFramebuffer(),retainedPixel,color),"empty data is rendered without stale vertices");
                chart->setSeries(makeLineDemoSeries(false,0)); chart->fitToData();
                window->resize(1100,700);
                QTimer::singleShot(250,this,[this] { precision(); });
            });
        });
    }
    bool coloredAt(const QImage& image,QPointF logical,QColor color,double radius=2) {
        const double dpr=chart->devicePixelRatioF();
        const QPointF expected=logical*dpr;
        for(int dy=-qCeil(radius*dpr);dy<=qCeil(radius*dpr);++dy)
            for(int dx=-qCeil(radius*dpr);dx<=qCeil(radius*dpr);++dx) {
                const QPoint at(qRound(expected.x())+dx,qRound(expected.y())+dy);
                if(!image.rect().contains(at)) continue;
                const auto actual=image.pixelColor(at);
                if(std::abs(actual.red()-color.red())+std::abs(actual.green()-color.green())+std::abs(actual.blue()-color.blue())<45) return true;
            }
        return false;
    }
    void viewportDrawing() {
        const auto savedSeries=chart->series(); const auto savedX=chart->xRange(),savedY=chart->yRange();
        const auto savedOptions=chart->renderOptions(); auto options=savedOptions;
        options.geometry=false; options.text=false; options.referenceLines=false; chart->setRenderOptions(options);
        chart->makeCurrent();
        auto* gl=QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_4_1_Core>(chart->context());
        const bool queryReady=gl && gl->initializeOpenGLFunctions();
        check(queryReady,"viewport checks have OpenGL primitive queries");
        if(!queryReady) { chart->doneCurrent(); chart->setRenderOptions(savedOptions); return; }
        chart->doneCurrent();
        const auto draw=[&](const QString& name) {
            chart->makeCurrent(); GLuint query=0,count=0; gl->glGenQueries(1,&query);
            gl->glBeginQuery(GL_PRIMITIVES_GENERATED,query);
            const auto image=chart->grabFramebuffer(); chart->makeCurrent();
            gl->glEndQuery(GL_PRIMITIVES_GENERATED); gl->glGetQueryObjectuiv(query,GL_QUERY_RESULT,&count);
            gl->glDeleteQueries(1,&query);
            check(gl->glGetError()==GL_NO_ERROR,"viewport query has no GL error: "+name);
            chart->doneCurrent();
            viewportCounts.insert(name,int(count)); image.save(reportPath+".viewport-"+name+".png");
            return qMakePair(image,count);
        };
        Chart::Series s; s.id="viewport"; s.color=QColor("#ff42ce");
        s.y.resize(10001); for(qsizetype i=0;i<s.y.size();++i) s.y[i]=i%2?1:-1;
        chart->setSeries({s}); chart->setViewRange({400,410},{-2,2});
        check(draw("uniform").second==12,"uniform X submits only visible segments and boundary neighbours");
        const auto uploads=chart->dataUploadCount(); chart->setXRange({600,610});
        check(draw("panned").second==12 && chart->dataUploadCount()==uploads,"panning changes draw interval without uploading data");
        check(chart->series().first().y==s.y,"viewport drawing retains all raw samples");
        Chart::Series second; second.id="second"; second.color=QColor("#42eaff");
        second.x={0,400,410,10000}; second.y={-1,-1,-1,-1};
        chart->setSeries({s,second}); chart->setXRange({400,410});
        const auto mixed=draw("multiple-series");
        check(mixed.second==15 && coloredAt(mixed.first,chart->mapFromChart({405,-1}).value(),second.color) &&
              coloredAt(mixed.first,chart->mapFromChart({405,1}).value(),s.color),
              "mixed X layouts retain per-series VBO offsets and original colours");
        chart->setSeries({s}); chart->setXRange({600,610}); draw("restored");
        const auto replacementUploads=chart->dataUploadCount();
        s.y[605]=1.75; chart->setSeries({s});
        check(draw("replacement").second==12 && chart->dataUploadCount()==replacementUploads+1 &&
              chart->series().first().y==s.y,"replacement frame uploads complete data and retains visible interval");
        chart->setSeriesVisible(s.id,false);
        check(draw("hidden").second==0,"hidden series submits no viewport primitives");
        chart->setSeriesVisible(s.id,true); chart->setXRange({0,10000});
        check(draw("full").second==10000,"full view retains every original line segment");
        chart->setXRange({20000,20010});
        check(draw("outside").second==0,"series outside X view submits no primitives");
        s.x={-200,-100,-1,-1,-1,1,1,1,100,200}; s.y={-1,-1,-0.8,0.8,0,-0.8,0.8,0,1,1};
        chart->setSeries({s}); chart->setViewRange({-1,1},{-2,2});
        check(draw("duplicates").second==7 && chart->series().first().x==s.x && chart->series().first().y==s.y,
              "explicit X keeps all boundary duplicates, adjacent segments and raw coordinates");
        s.x={-100,100}; s.y={-100,100}; chart->setSeries({s}); chart->setViewRange({-1,1},{-1,1});
        const auto crossing=draw("crossing");
        check(crossing.second==1 && coloredAt(crossing.first,chart->mapFromChart({0,0}).value(),s.color),
              "segment crossing view remains visible with both endpoints outside X and Y");
        s.x.clear(); s.xStart=1e16; s.xStep=0.1; s.y.resize(60);
        for(qsizetype i=0;i<s.y.size();++i) s.y[i]=i%2?0.5:-0.5;
        chart->setSeries({s}); chart->setViewRange({1e16+2,1e16+4},{-1,1});
        const auto rounded=draw("rounded-uniform");
        check(rounded.second>0 && rounded.second<59 && chart->series().first().y==s.y,
              "uniform X culling handles repeated representable coordinates at large origins");
        s.xStart=-1-1.0/chart->plotRect().width(); s.xStep=1; s.y={0};
        chart->setSeries({s}); chart->setViewRange({-1,1},{-1,1});
        const auto edge=draw("edge-point");
        check(edge.second==1 && coloredAt(edge.first,{chart->plotRect().left()+0.5,chart->plotRect().center().y()},s.color,1),
              "point footprint overlapping the viewport edge is retained");
        chart->clear(); check(draw("empty").second==0,"empty snapshot submits no viewport primitives");
        chart->setSeries(savedSeries); chart->setViewRange(savedX,savedY); chart->setRenderOptions(savedOptions);
        chart->grabFramebuffer();
    }
    void precision() {
        Chart::Series s; s.id="precision"; s.color=QColor("#ff42ce");
        s.x={0,100000000,100000001}; s.y={0,1,3};
        chart->setSeries({s}); chart->setViewRange({99999999,100000002},{0,4});
        QTimer::singleShot(80,this,[this,s] {
            auto image=chart->grabFramebuffer();
            check(coloredAt(image,chart->mapFromChart({100000001,3}).value(),s.color),"large-domain zoom GPU endpoint matches double sample mapping");
            chart->setSeries(makeLineDemoSeries(false,0)); chart->fitToData();
            QTimer::singleShot(80,this,[this] { alignment(); });
        });
    }
    void alignment() {
        chart->fitToData();
        const auto pixel=chart->mapFromChart({2.56,1.8}).value();
        const auto value=chart->mapToChart(pixel).value();
        check(near(value.x(),2.56) && near(value.y(),1.8),"resized widget maps spike accurately");
        window->findChild<QCheckBox*>("lineCursors")->setChecked(true);
        mouse(chart,QEvent::MouseButtonPress,pixel,Qt::LeftButton,Qt::LeftButton,Qt::AltModifier);
        mouse(chart,QEvent::MouseButtonRelease,pixel,Qt::LeftButton,{});
        const auto bPixel=chart->mapFromChart({chart->series()[1].xStep*429,4.3}).value();
        mouse(chart,QEvent::MouseButtonPress,bPixel,Qt::RightButton,Qt::RightButton,Qt::AltModifier);
        mouse(chart,QEvent::MouseButtonRelease,bPixel,Qt::RightButton,{});
        auto a=chart->cursor(0),b=chart->cursor(1);
        check(a && b && a->index==256 && b->index==429 && near(b->x-a->x,1.73) && near(b->y-a->y,2.5),"Alt mouse cursors snap to spikes and retain delta");
        // Only this screenshot path reads the GL framebuffer.
        QImage image=chart->grabFramebuffer(); const double dpr=chart->devicePixelRatioF();
        const QPointF expected=pixel*dpr; const QColor color=chart->series()[0].color;
        double closest=1e9;
        for(int dy=-8;dy<=8;++dy) for(int dx=-8;dx<=8;++dx) {
            const QPoint at(qRound(expected.x())+dx,qRound(expected.y())+dy);
            if(!image.rect().contains(at)) continue;
            const auto actual=image.pixelColor(at);
            const int error=std::abs(actual.red()-color.red())+std::abs(actual.green()-color.green())+std::abs(actual.blue()-color.blue());
            if(error<45) closest=std::min(closest,std::hypot(at.x()+0.5-expected.x(),at.y()+0.5-expected.y())/dpr);
        }
        environment["logicalChartSize"]=QJsonArray{chart->width(),chart->height()};
        environment["framebufferSize"]=QJsonArray{image.width(),image.height()};
        environment["spikeAlignmentErrorLogicalPx"]=closest;
        check(closest<=2,"GPU spike aligns with logical mouse coordinates at current DPI");
        image.save(reportPath+".framebuffer.png");
        const auto composed=chart->grab().toImage();
        check(coloredAt(composed,pixel,QColor(255,214,92)) && coloredAt(composed,bPixel,QColor(245,139,206)),"Qt composed A and B cursor markers align with GPU spikes");
        window->grab().save(reportPath+".window.png");
        chart->clearCursors(); window->findChild<QCheckBox*>("lineCursors")->setChecked(false);
        baselineUploads=chart->dataUploadCount(); step=0;
        begin("static_interaction"); interactions.start();
        QTimer::singleShot(1800,this,[this] {
            interactions.stop(); end(); check(chart->dataUploadCount()==baselineUploads,"static pan and zoom reuse uploaded buffer");
            running->setChecked(true);
            QTimer::singleShot(1000,this,[this] {
                preservedX=chart->xRange(); preservedY=chart->yRange();
                continuousUploads=chart->dataUploadCount();
                begin("continuous_8x2048");
                QTimer::singleShot(6000,this,[this] {
                    if(!near(chart->xRange().min,preservedX.min) || !near(chart->yRange().max,preservedY.max))
                        qWarning()<<"Steady view changed: X"<<preservedX.min<<preservedX.max<<"->"<<chart->xRange().min<<chart->xRange().max
                                  <<"Y"<<preservedY.min<<preservedY.max<<"->"<<chart->yRange().min<<chart->yRange().max;
                    check(near(chart->xRange().min,preservedX.min) && near(chart->yRange().max,preservedY.max),"continuous source preserves adjusted ranges over six seconds");
                    check(chart->dataUploadCount()>continuousUploads+200,"continuous source uploads more than 200 changing frames in six seconds");
                    end(); begin("continuous_with_interaction"); step=0; interactions.start();
                    QTimer::singleShot(6000,this,[this] {
                        interactions.stop(); end(); running->setChecked(false); chart->fitToData();
                        QTimer::singleShot(100,this,[this] {
                            const auto feed=lineDemoFeedStats(window);
                            check(feed.submitted>400 && feed.displayed==feed.submitted && !feed.pending && feed.rejected==0,
                                  "continuous full frames are all displayed without replacement or rejection");
                            feedCounts={{"submitted",qint64(feed.submitted)},{"displayed",qint64(feed.displayed)},
                                        {"rejected",qint64(feed.rejected)},{"pending",feed.pending}};
                            rateChecks();
                        });
                    });
                });
            });
        });
    }
    void rateChecks() {
        auto* rate=window->findChild<QSpinBox*>("lineRate");
        rate->setValue(5); running->setChecked(true);
        QTimer::singleShot(100,this,[this,rate] {
            const auto before=lineDemoFeedStats(window);
            QTimer::singleShot(1200,this,[this,rate,before] {
                const auto slow=lineDemoFeedStats(window);
                const quint64 frames=slow.submitted-before.submitted;
                check(frames>=4 && frames<=7,"5 fps setting limits generated full frames");
                rateObservations.append(QJsonObject{{"requestedFps",5},{"durationMs",1200},{"submittedFrames",qint64(frames)}});
                rate->setValue(30); // Change the source rate while it is running.
                QTimer::singleShot(1200,this,[this,slow] {
                    const auto fast=lineDemoFeedStats(window);
                    const quint64 frames=fast.submitted-slow.submitted;
                    check(frames>=20 && frames<=38,"changing source rate to 30 fps takes effect while running");
                    rateObservations.append(QJsonObject{{"requestedFps",30},{"durationMs",1200},{"submittedFrames",qint64(frames)}});
                    running->setChecked(false);
                    QTimer::singleShot(100,this,[this] {
                        const auto feed=lineDemoFeedStats(window);
                        check(feed.displayed==feed.submitted && !feed.pending && feed.rejected==0,"rate changes preserve all submitted frames");
                        feedCounts={{"submitted",qint64(feed.submitted)},{"displayed",qint64(feed.displayed)},
                                    {"rejected",qint64(feed.rejected)},{"pending",feed.pending}};
                        finish();
                    });
                });
            });
        });
    }
    void interact() {
        const double now=clock.nsecsElapsed()/1e6;
        if(lastAction>0 && now-lastAction>25) actionGaps.append(QJsonObject{
            {"elapsedMs",now-phaseStart},{"gapMs",now-lastAction},
            {"windowExposed",window->windowHandle() && window->windowHandle()->isExposed()},
            {"windowActive",window->isActiveWindow()}});
        lastAction=now;
        QElapsedTimer actionClock; actionClock.start();
        const QPointF center=chart->plotRect().center();
        if(step%60==0) chart->fitToData();
        if(step%4==0) wheel(chart,center,step%60<30?120:-120,step%3==0?Qt::ShiftModifier:(step%3==1?Qt::ControlModifier:Qt::NoModifier));
        mouse(chart,QEvent::MouseButtonPress,center,Qt::LeftButton,Qt::LeftButton);
        mouse(chart,QEvent::MouseMove,center+QPointF(step%2?2:-2,step%2?1:-1),Qt::NoButton,Qt::LeftButton);
        mouse(chart,QEvent::MouseButtonRelease,center,Qt::LeftButton,{});
        const double elapsed=actionClock.nsecsElapsed()/1e6;
        if(elapsed>10) slowActions.append(QJsonObject{{"elapsedMs",now-phaseStart},{"cpuMs",elapsed},{"step",step}});
        ++step;
    }
    void begin(QString name) {
        phase=std::move(name); paints.clear(); geometryTimes.clear(); textTimes.clear(); dataTimes.clear();
        paintIntervals.clear(); intervals.clear(); longFrames={}; slowPaints={};
        slowActions={}; actionGaps={}; lastAction=0; lastFrame=0; lastPaint=0;
        phaseStart=clock.nsecsElapsed()/1e6; measuring=true;
    }
    void end() {
        measuring=false;
        phases.append(QJsonObject{{"name",phase},{"elapsedMs",clock.nsecsElapsed()/1e6-phaseStart},
            {"paintCpu",distribution(paints)},{"frameIntervals",distribution(intervals)},
            {"paintIntervals",distribution(paintIntervals)},
            {"geometryCpu",distribution(geometryTimes)},{"textCpu",distribution(textTimes)},{"dataCpu",distribution(dataTimes)},
            {"longFrameThresholdMs",25},{"longFrames",longFrames},{"paintsOver10Ms",slowPaints},
            {"interactionsOver10Ms",slowActions},{"interactionTimerGapsOver25Ms",actionGaps}});
        check(!paints.isEmpty() && !intervals.isEmpty(),phase+" has real paint and frame-swap observations");
    }
    void finish() {
        chart->paintMeasured={};
        const auto& rendering=chart->renderDiagnostics();
        check(rendering.textCacheRebuilds>0 && rendering.textFallbackFrames==0,
              "coordinate text uses the GPU cache without painter fallback");
        QJsonObject report{{"passed",okay},{"performanceIsObservation",true},{"environment",environment},{"functionalChecks",checks},{"phases",phases},{"feed",feedCounts},{"generationRateChecks",rateObservations},{"steadyViewChanges",steadyViewChanges},{"nativeInputsDuringMeasurement",nativeInputs},
            {"textCache",QJsonObject{{"rebuilds",qint64(rendering.textCacheRebuilds)},{"fallbackFrames",qint64(rendering.textFallbackFrames)}}},
            {"viewportPrimitives",viewportCounts},
            {"notes","CPU paint duration excludes asynchronous GPU execution; frameIntervals are Qt frameSwapped/composition callbacks, not physical monitor presentations; paintIntervals measure completed GL paints. Long frame threshold is 25 ms. Random full raw frames with frame-swap backpressure, no pending-frame replacement. Measured phases use unlimited source rate; subsequent generationRateChecks exercise the optional example-side rate cap. No pixel readback during measured phases."}};
        QFile file(reportPath);
        if(!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(report).toJson())<0) { qCritical()<<"Cannot write"<<reportPath; okay=false; }
        qInfo().noquote()<<"LineChart acceptance"<<(okay?"PASS":"FAIL")<<reportPath;
        QApplication::exit(okay?0:1);
    }
    QWidget* window; DemoLineChart* chart=nullptr; QCheckBox* running=nullptr;
    QString reportPath,phase; QElapsedTimer clock; QTimer interactions;
    bool okay=true,measuring=false; double lastFrame=0,lastPaint=0,lastAction=0,phaseStart=0; int step=0;
    Chart::Range preservedX,preservedY;
    quint64 baselineUploads=0,continuousUploads=0;
    QVector<double> paints,paintIntervals,intervals,geometryTimes,textTimes,dataTimes;
    QJsonArray checks,phases,longFrames,slowPaints,slowActions,actionGaps,rateObservations,steadyViewChanges,nativeInputs; QJsonObject environment,feedCounts,viewportCounts;
};
}
void startLineChartAcceptance(QWidget* demo,const QString& reportPath) { new Acceptance(demo,reportPath); }
