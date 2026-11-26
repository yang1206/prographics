#include "prographics/charts/line/line_chart.h"
#include "linear_ticks_p.h"
#include <QKeyEvent>
#include <QGuiApplication>
#include <QLocale>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QPainter>
#include <QResizeEvent>
#include <QSet>
#include <QScopedValueRollback>
#include <QVector2D>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>

namespace ProGraphics {
namespace {
QString number(double value) { return QString::number(value, 'g', QLocale::FloatingPointShortest); }
LineChart::Range expanded(double lo,double hi,bool padding) {
    const double pad=hi>lo ? (padding ? (hi-lo)*0.05 : 0) : std::max(1.0,std::abs(lo)*0.05);
    const double min=std::isfinite(lo-pad)?lo-pad:lo;
    const double max=std::isfinite(hi+pad)?hi+pad:hi;
    if (!std::isfinite(max-min)) return {lo,hi};
    return {min,max};
}
QString reading(const LineChart::Sample& s) {
    return QStringLiteral("%1  [%2]  X %3   Y %4").arg(s.seriesId).arg(s.index).arg(number(s.x), number(s.y));
}
template<class XAt>
QPair<int,int> visibleSpan(qsizetype count,double min,double max,XAt xAt) {
    if(!count || xAt(0)>max || xAt(count-1)<min) return {};
    if(xAt(0)>=min && xAt(count-1)<=max) return {0,int(count)};
    const auto bound=[&](double value,bool upper) {
        qsizetype first=0,last=count;
        while(first<last) {
            const qsizetype middle=first+(last-first)/2;
            const double x=xAt(middle);
            if(x<value || (upper && x==value)) first=middle+1; else last=middle;
        }
        return first;
    };
    qsizetype first=bound(min,false),last=bound(max,true);
    // 保留两侧相邻点，让端点均在范围外的穿越线段也能由 GPU 正确裁剪。
    if(first>0) --first;
    if(last<count) ++last;
    return {int(first),int(last-first)};
}
}

// Qt 合成的普通覆盖层：鼠标移动和游标读数不需要重传或读回 GL 数据。
class LineChartOverlay final : public QWidget {
public:
    explicit LineChartOverlay(LineChart& chart) : QWidget(&chart), m_chart(chart) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF plot = m_chart.plotRect().intersected(QRectF(rect()));
        if (!plot.isValid()) return;
        p.setClipRect(plot);
        auto card = [&](QString text, QPointF at) {
            QRectF box = p.fontMetrics().boundingRect(QRect(0,0,10000,1000), Qt::TextExpandTabs, text);
            box.adjust(-7,-4,7,4);
            box.moveTopLeft(at);
            box.moveLeft(std::clamp(box.left(), plot.left(), std::max(plot.left(), plot.right()-box.width())));
            box.moveTop(std::clamp(box.top(), plot.top(), std::max(plot.top(), plot.bottom()-box.height())));
            p.setPen(QColor(180,200,220)); p.setBrush(QColor(16,26,42,235));
            p.drawRoundedRect(box,3,3); p.setPen(Qt::white);
            p.drawText(box.adjusted(7,4,-7,-4), Qt::AlignLeft | Qt::AlignVCenter, text);
        };
        QStringList rows;
        if (m_chart.m_cursorsEnabled) {
            for (int i=0;i<2;++i) if (auto s=m_chart.cursor(i)) {
                auto pos=m_chart.mapFromChart({s->x,s->y});
                if (!pos) continue;
                QColor color = i==0 ? QColor(255,214,92) : QColor(245,139,206);
                p.setPen(QPen(color,1,Qt::DashLine));
                p.drawLine(QPointF(pos->x(),plot.top()),QPointF(pos->x(),plot.bottom()));
                p.drawLine(QPointF(plot.left(),pos->y()),QPointF(plot.right(),pos->y()));
                p.setBrush(color); p.drawEllipse(*pos,4,4);
                rows << QString(i==0 ? "A  " : "B  ")+reading(*s);
            }
            const auto a=m_chart.cursor(0), b=m_chart.cursor(1);
            if (a && b) rows << QStringLiteral("ΔX %1   ΔY %2").arg(number(b->x-a->x),number(b->y-a->y));
        }
        if (!rows.isEmpty()) card(rows.join('\n'),plot.topLeft()+QPointF(10,10));
        if (m_chart.m_hoverEnabled && m_chart.m_hover) {
            const auto& s=*m_chart.m_hover;
            if (auto pos=m_chart.mapFromChart({s.x,s.y}); pos && plot.contains(*pos)) {
                p.setPen(QPen(Qt::white,1.5)); p.setBrush(Qt::NoBrush); p.drawEllipse(*pos,5,5);
                card(reading(s),*pos+QPointF(14,18));
            }
        }
    }
private:
    LineChart& m_chart;
};

struct LineChart::Gpu {
    QOpenGLShaderProgram program;
    QOpenGLBuffer buffer{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject vao;
    QVector<QPair<int,int>> spans;
    QVector<QVector4D> vertices;
    int capacity = 0;
    GLfloat lineWidthRange[2]{1,1};
};

struct LineChart::Layout {
    Coordinate2D::Config config;
    QSize widgetSize;
    Range x,y;
    qreal dpr=0;
    int dpiX=0,dpiY=0;
    QFont appFont;
    QVector<TextRenderer::ScreenLabel> labels;
    bool valid=false,updating=false;
};

bool LineChart::valid(Range r) {
    return std::isfinite(r.min) && std::isfinite(r.max) && r.max>r.min && std::isfinite(r.max-r.min);
}
double LineChart::xAt(const Series& s, qsizetype i) {
    return s.x.isEmpty() ? std::fma(double(i),s.xStep,s.xStart) : s.x[i];
}
const LineChart::Series* LineChart::findSeries(const QString& id) const {
    for (const auto& s:m_series) if (s.id==id) return &s;
    return nullptr;
}

LineChart::LineChart(QWidget* parent) : Coordinate2D(parent) {
    m_layout=std::make_unique<Layout>();
    setFormat(chartSurfaceFormat());
    setMouseTracking(true); setFocusPolicy(Qt::StrongFocus);
    auto cfg=config();
    cfg.displayMode=DisplayMode::Stretch;
    cfg.grid.xy.sineWave.visible=false;
    cfg.grid.xy.spacing=1.25f;
    cfg.grid.xy.majorColor=QVector4D(0.45f,0.55f,0.68f,0.4f);
    cfg.grid.xy.minorColor=QVector4D(0.35f,0.43f,0.55f,0.2f);
    cfg.axis.x.color=cfg.axis.y.color=QVector4D(0.75f,0.8f,0.88f,1);
    cfg.ticks.x.range={0,1,0.25f}; cfg.ticks.y.range={0,1,0.25f};
    cfg.names.x.text="X"; cfg.names.y.text="Y";
    setConfig(cfg); updateTicks();
    m_overlay=new LineChartOverlay(*this); m_overlay->setGeometry(rect());
}
LineChart::~LineChart() { cleanupGL(); }
void LineChart::cleanupGL() {
    QObject::disconnect(m_contextConnection);
    if (m_gpu) { makeCurrent(); m_gpu.reset(); doneCurrent(); }
    m_dirty=true;
}

bool LineChart::setSeries(QVector<Series> series) {
    QSet<QString> ids;
    double xmin=std::numeric_limits<double>::infinity(), xmax=-xmin, ymin=xmin, ymax=-xmin;
    qsizetype count=0;
    for (const auto& s:series) {
        if (s.id.isEmpty() || ids.contains(s.id) || !s.color.isValid()) return false;
        ids.insert(s.id);
        if ((!s.x.isEmpty() && s.x.size()!=s.y.size()) ||
            (s.x.isEmpty() && (!std::isfinite(s.xStart) || !std::isfinite(s.xStep) || s.xStep<=0))) return false;
        if (s.y.size()>(std::numeric_limits<int>::max()/int(sizeof(QVector4D)))-count) return false;
        count+=s.y.size();
        double previous=-std::numeric_limits<double>::infinity();
        for (qsizetype i=0;i<s.y.size();++i) {
            double x=xAt(s,i), y=s.y[i];
            if (!std::isfinite(x) || !std::isfinite(y) || x<previous) return false;
            previous=x; xmin=std::min(xmin,x); xmax=std::max(xmax,x);
            ymin=std::min(ymin,y); ymax=std::max(ymax,y);
        }
    }
    if (count && (!std::isfinite(xmax-xmin) || !std::isfinite(ymax-ymin))) return false;
    m_series=std::move(series);
    m_dataX=count ? expanded(xmin,xmax,false) : Range{};
    m_dataY=count ? expanded(ymin,ymax,false) : Range{};
    m_dirty=true;
    if (m_initialFit && count) fitToData();
    m_hover=m_hoverEnabled && m_hoverPosition && !m_panning && m_dragCursor<0 ? sampleAt(*m_hoverPosition) : std::nullopt;
    update(); updateOverlay(); emit cursorsChanged();
    return true;
}
void LineChart::clear() { setSeries({}); clearCursors(); }
bool LineChart::setSeriesVisible(const QString& id,bool visible) {
    for (auto& s:m_series) if (s.id==id) {
        s.visible=visible; m_hover.reset(); update(); updateOverlay(); emit cursorsChanged(); return true;
    }
    return false;
}
bool LineChart::setSeriesColor(const QString& id,const QColor& color) {
    if (!color.isValid()) return false;
    for (auto& s:m_series) if (s.id==id) { s.color=color; update(); return true; }
    return false;
}
QRectF LineChart::chartDataRect() const { return QRectF(QPointF(m_x.min,m_y.min),QPointF(m_x.max,m_y.max)); }
bool LineChart::setViewRange(Range x,Range y) {
    return setViewRangeAt(x,y,{},{},{});
}
bool LineChart::setViewRangeAt(Range x,Range y,QPointF pixel,QPointF data,Qt::Orientations anchoredAxes) {
    if (!valid(x) || !valid(y)) return false;
    m_initialFit=false;
    if (!anchoredAxes && x.min==m_x.min && x.max==m_x.max && y.min==m_y.min && y.max==m_y.max) return true;
    const auto previousX=m_x,previousY=m_y;
    m_x=x; m_y=y;
    // 刻度字宽会改变绘图区；用最终绘图区校正锚点，只在收敛后发出一次范围变更。
    for(int pass=0;;++pass) {
        updateTicks();
        if(!anchoredAxes || pass==8) break;
        const auto mapped=mapToChart(pixel),projected=mapFromChart(data);
        if(!mapped || !projected) break;
        const bool horizontal=anchoredAxes.testFlag(Qt::Horizontal),vertical=anchoredAxes.testFlag(Qt::Vertical);
        if((!horizontal || std::abs(projected->x()-pixel.x())<0.00001) &&
           (!vertical || std::abs(projected->y()-pixel.y())<0.00001)) break;
        const double dx=horizontal?data.x()-mapped->x():0,dy=vertical?data.y()-mapped->y():0;
        const Range nextX{m_x.min+dx,m_x.max+dx},nextY{m_y.min+dy,m_y.max+dy};
        if(!valid(nextX) || !valid(nextY)) break;
        if(nextX.min==m_x.min && nextX.max==m_x.max && nextY.min==m_y.min && nextY.max==m_y.max) break;
        m_x=nextX; m_y=nextY;
    }
    if(m_x.min==previousX.min && m_x.max==previousX.max && m_y.min==previousY.min && m_y.max==previousY.max) return true;
    refreshSelectionProjection();
    m_hover.reset(); update(); updateOverlay(); emit viewRangeChanged(); return true;
}
void LineChart::fitToData(Qt::Orientations axes) {
    double xmin=std::numeric_limits<double>::infinity(),xmax=-xmin,ymin=xmin,ymax=-xmin;
    for (const auto& s:m_series) if (s.visible && !s.y.isEmpty()) {
        xmin=std::min(xmin,xAt(s,0)); xmax=std::max(xmax,xAt(s,s.y.size()-1));
        for (double v:s.y) { ymin=std::min(ymin,v); ymax=std::max(ymax,v); }
    }
    if (!std::isfinite(xmin)) return;
    setViewRange(axes.testFlag(Qt::Horizontal)?expanded(xmin,xmax,false):m_x,
                 axes.testFlag(Qt::Vertical)?expanded(ymin,ymax,true):m_y);
}
void LineChart::updateTicks() {
    updateCoordinateLayout();
}

void LineChart::updateCoordinateLayout() {
    if(!m_layout || m_layout->updating) return;
    auto& layout=*m_layout;
    const auto& current=config();
    const auto sameStyle=[](const TextRenderer::TextStyle& a,const TextRenderer::TextStyle& b) {
        return a.fontFamily==b.fontFamily && a.fontSize==b.fontSize && a.bold==b.bold && a.italic==b.italic && a.color==b.color;
    };
    const auto sameName=[&](const AxisName::NameConfig& a,const AxisName::NameConfig& b) {
        return a.visible==b.visible && a.text==b.text && a.unit==b.unit && sameStyle(a.style,b.style);
    };
    const auto& old=layout.config;
    const auto appFont=QGuiApplication::font();
    if(layout.valid && layout.widgetSize==size() && layout.dpr==devicePixelRatioF() &&
       layout.dpiX==logicalDpiX() && layout.dpiY==logicalDpiY() && layout.appFont==appFont &&
       layout.x.min==m_x.min && layout.x.max==m_x.max && layout.y.min==m_y.min && layout.y.max==m_y.max &&
       old.size==current.size && old.displayMode==current.displayMode &&
       old.margin.left==current.margin.left && old.margin.right==current.margin.right &&
       old.margin.top==current.margin.top && old.margin.bottom==current.margin.bottom &&
       old.axis.x.thickness==current.axis.x.thickness && old.axis.y.thickness==current.axis.y.thickness &&
       old.grid.xy.thickness==current.grid.xy.thickness && old.ticks.enabled==current.ticks.enabled &&
       old.ticks.x.visible==current.ticks.x.visible && old.ticks.y.visible==current.ticks.y.visible &&
       sameStyle(old.ticks.x.style,current.ticks.x.style) && sameStyle(old.ticks.y.style,current.ticks.y.style) &&
       old.names.enabled==current.names.enabled && sameName(old.names.x,current.names.x) && sameName(old.names.y,current.names.y)) return;
    QScopedValueRollback<bool> guard(layout.updating,true);
    const auto cfg=current;
    if(width()<=0 || height()<=0 || !std::isfinite(cfg.size) || cfg.size<=0) { layout.labels.clear(); layout.valid=false; return; }
    const auto fontOf=[](const TextRenderer::TextStyle& style) {
        QFont font(style.fontFamily,style.fontSize); font.setBold(style.bold); font.setItalic(style.italic);
        return font;
    };
    const auto xFont=fontOf(cfg.ticks.x.style),yFont=fontOf(cfg.ticks.y.style);
    const auto xNameFont=fontOf(cfg.names.x.style),yNameFont=fontOf(cfg.names.y.style);
    const QFontMetrics xMetrics(xFont,this),yMetrics(yFont,this),xNameMetrics(xNameFont,this),yNameMetrics(yNameFont,this);
    const double w=width(),h=height(),pad=std::min(8.0,std::min(w,h)/4);
    const double stroke=std::max({cfg.axis.x.thickness,cfg.axis.y.thickness,cfg.grid.xy.thickness});
    const double gap=6+stroke/2,nameGap=6;
    const auto title=[](const AxisName::NameConfig& name) { return name.text+(name.unit.isEmpty()?QString():" ("+name.unit+")"); };
    const QString xName=xNameMetrics.elidedText(title(cfg.names.x),Qt::ElideRight,int(w-2*pad));
    const QString yName=yNameMetrics.elidedText(title(cfg.names.y),Qt::ElideRight,int(w-2*pad));
    bool showX=cfg.ticks.enabled && cfg.ticks.x.visible,showY=cfg.ticks.enabled && cfg.ticks.y.visible;
    bool showXName=cfg.names.enabled && cfg.names.x.visible && !xName.isEmpty();
    bool showYName=cfg.names.enabled && cfg.names.y.visible && !yName.isEmpty();
    const double xNameWidth=xNameMetrics.boundingRect(xName).width(),xNameHeight=xNameMetrics.boundingRect(xName).height();
    const double yNameHeight=yNameMetrics.boundingRect(yName).height();
    showXName=showXName && xNameWidth<=w-2*pad;
    showYName=showYName && yNameMetrics.boundingRect(yName).width()<=w-2*pad;
    const auto convert=[](const detail::LinearTicks& ticks,Range range) {
        QVector<AxisTicks::Tick> result; result.reserve(ticks.values.size());
        for(const auto& tick:ticks.values) result.append({float((tick.value-range.min)/(range.max-range.min)),tick.text});
        return result;
    };
    const auto prune=[](QVector<AxisTicks::Tick> ticks,double pixels,const QFontMetrics& metrics,bool vertical) {
        while(ticks.size()>1) {
            double end=-std::numeric_limits<double>::infinity(); bool overlap=false;
            for(const auto& tick:ticks) {
                const auto bounds=metrics.boundingRect(tick.text);
                const double extent=vertical?bounds.height():bounds.width(),center=tick.position*pixels;
                if(center-extent/2<end+4) { overlap=true; break; }
                end=center+extent/2;
            }
            if(!overlap) break;
            QVector<AxisTicks::Tick> fewer; for(qsizetype i=0;i<ticks.size();i+=2) fewer.append(ticks[i]);
            ticks=std::move(fewer);
        }
        return ticks;
    };
    QVector<AxisTicks::Tick> xs,ys;
    const auto locate=[&](QRectF plot) {
        xs=showX?prune(convert(detail::linearTicks(m_x.min,m_x.max,plot.width(),false,[&](const QString& text) {
            return xMetrics.boundingRect(text).width()+4.0;
        }),m_x),plot.width(),xMetrics,false):QVector<AxisTicks::Tick>{};
        ys=showY?prune(convert(detail::linearTicks(m_y.min,m_y.max,plot.height(),true,[&](const QString&) {
            return yMetrics.height()+4.0;
        }),m_y),plot.height(),yMetrics,true):QVector<AxisTicks::Tick>{};
    };
    const auto applyMargins=[&](QMarginsF margins) {
        const double px=std::max(1.0,w-margins.left()-margins.right()),py=std::max(1.0,h-margins.top()-margins.bottom());
        // 只在本组件内把逻辑像素换成现有相机的边距约定，保持其它图谱的相机行为。
        const float left=float(cfg.size*margins.right()/px),right=float(cfg.size*margins.left()/px);
        const float top=float(cfg.size*margins.bottom()/py),bottom=float(cfg.size*margins.top()/py);
        const auto& m=config().margin;
        if(m.left!=left || m.right!=right || m.top!=top || m.bottom!=bottom) Coordinate2D::setMargin(left,right,top,bottom);
    };
    QMarginsF margins(pad,pad,pad,pad);
    double xWidth=0,xHeight=0,yWidth=0,yHeight=0,xRow=gap,xTitleRow=gap;
    for(int pass=0;pass<8;++pass) {
        applyMargins(margins); const auto plot=plotRect(); locate(plot);
        for(const auto& tick:xs) { const auto box=xMetrics.boundingRect(tick.text); xWidth=std::max(xWidth,double(box.width())); xHeight=std::max(xHeight,double(box.height())); }
        for(const auto& tick:ys) { const auto box=yMetrics.boundingRect(tick.text); yWidth=std::max(yWidth,double(box.width())); yHeight=std::max(yHeight,double(box.height())); }
        xRow=gap;
        if(showY) for(const auto& tick:xs)
            if(tick.position*plot.width()-xMetrics.boundingRect(tick.text).width()/2.0 < -gap)
                xRow=std::max(xRow,yHeight/2+4); // 两轴在左下角相遇时，也给文字外框留出间距。
        xTitleRow=showX?xRow+xHeight+nameGap:gap;
        if(showY && plot.width()<xNameWidth+2*gap) xTitleRow=std::max(xTitleRow,yHeight/2+4);
        const double yTitleGap=showY?std::max(gap,yHeight/2+nameGap):gap;
        QMarginsF needed(pad+std::max(showY?yWidth+gap:0.0,showX?xWidth/2:0.0),
            pad+std::max(showY?yHeight/2:0.0,showYName?yTitleGap+yNameHeight:0.0),
            pad+(showX?xWidth/2:0.0),
            pad+std::max({showY?yHeight/2:0.0,showX?xRow+xHeight:0.0,showXName?xTitleRow+xNameHeight:0.0}));
        if(h<=needed.top()+needed.bottom()+1) {
            if(showXName || showYName) { showXName=false; showYName=false; }
            else if(showX) showX=false;
            else showY=false;
            margins={pad,pad,pad,pad}; continue;
        }
        if(w<=needed.left()+needed.right()+1) {
            if(showY) showY=false; else showX=false;
            margins={pad,pad,pad,pad}; continue;
        }
        if(margins==needed) break;
        margins=needed;
    }
    applyMargins(margins); const auto plot=plotRect(); locate(plot);
    layout.labels.clear(); QVector<QRectF> occupied;
    const auto add=[&](const QString& text,const QFont& font,const QColor& color,QPointF anchor,Qt::Alignment alignment) {
        if(text.isEmpty()) return false;
        const auto label=TextRenderer::alignedScreenLabel(text,font,color,anchor,alignment,this);
        const auto ink=QRectF(label.bounds).translated(label.baseline);
        if(ink.isEmpty() || !QRectF(rect()).contains(ink) || ink.intersects(plot.adjusted(-stroke/2,-stroke/2,stroke/2,stroke/2))) return false;
        for(const auto& previous:occupied) if(previous.adjusted(-2,-2,2,2).intersects(ink)) return false;
        occupied.append(ink); layout.labels.append(label); return true;
    };
    if(showYName) add(yName,yNameFont,cfg.names.y.style.color,{pad,plot.top()-(showY?std::max(gap,yHeight/2+nameGap):gap)},Qt::AlignLeft|Qt::AlignBottom);
    if(showXName) {
        const double center=std::clamp(plot.center().x(),pad+xNameWidth/2,w-pad-xNameWidth/2);
        add(xName,xNameFont,cfg.names.x.style.color,{center,plot.bottom()+xTitleRow},Qt::AlignHCenter|Qt::AlignTop);
    }
    QVector<AxisTicks::Tick> visibleX,visibleY;
    for(const auto& tick:xs) if(add(tick.text,xFont,cfg.ticks.x.style.color,{plot.left()+tick.position*plot.width(),plot.bottom()+xRow},Qt::AlignHCenter|Qt::AlignTop)) visibleX.append(tick);
    for(const auto& tick:ys) if(add(tick.text,yFont,cfg.ticks.y.style.color,{plot.left()-gap,plot.bottom()-tick.position*plot.height()},Qt::AlignRight|Qt::AlignVCenter)) visibleY.append(tick);
    setCustomTicks(visibleX,visibleY);
    layout.config=config(); layout.widgetSize=size(); layout.dpr=devicePixelRatioF();
    layout.dpiX=logicalDpiX(); layout.dpiY=logicalDpiY(); layout.appFont=appFont; layout.x=m_x; layout.y=m_y; layout.valid=true;
}

void LineChart::collectCoordinateText(QPainter&,QVector<TextRenderer::ScreenLabel>& labels) {
    updateCoordinateLayout();
    if(m_layout) labels+=m_layout->labels;
}
void LineChart::updateOverlay() { if (m_overlay) m_overlay->update(); }
void LineChart::setHoverEnabled(bool enabled) { m_hoverEnabled=enabled; m_hover.reset(); updateOverlay(); }
void LineChart::setCursorsEnabled(bool enabled) {
    m_cursorsEnabled=enabled; m_dragCursor=-1; updateOverlay(); emit cursorsChanged();
}
bool LineChart::setCursor(int which,const QString& id,qsizetype index) {
    const auto* s=findSeries(id);
    if (which<0 || which>1 || !s || index<0 || index>=s->y.size()) return false;
    m_cursors[which]={id,index}; updateOverlay(); emit cursorsChanged(); return true;
}
std::optional<LineChart::Sample> LineChart::cursor(int which) const {
    if (which<0 || which>1) return {};
    const auto& c=m_cursors[which]; const auto* s=findSeries(c.id);
    if (!s || !s->visible || c.index<0 || c.index>=s->y.size()) return {};
    return Sample{s->id,c.index,xAt(*s,c.index),s->y[c.index]};
}
void LineChart::clearCursors() { m_cursors[0]={}; m_cursors[1]={}; updateOverlay(); emit cursorsChanged(); }

std::optional<LineChart::Sample> LineChart::nearestSample(QPointF pos,double radius) const {
    const auto plot=plotRect();
    if (!plot.contains(pos) || !std::isfinite(radius) || radius<0) return {};
    const auto value=mapToChart(pos); if (!value) return {};
    const double dx=(m_x.max-m_x.min)*radius/plot.width();
    double distance=radius*radius;
    std::optional<Sample> result;
    for (const auto& s:m_series) if (s.visible && !s.y.isEmpty()) {
        const double low=value->x()-dx, high=value->x()+dx;
        qsizetype first=0,last=s.y.size();
        if (!s.x.isEmpty()) {
            first=std::lower_bound(s.x.cbegin(),s.x.cend(),low)-s.x.cbegin();
            last=std::upper_bound(s.x.cbegin(),s.x.cend(),high)-s.x.cbegin();
        } else {
            // 二分实际计算出的 X，涵盖大起点、小步长造成的重复可表示坐标。
            const auto bound=[&](double target,bool upper) {
                qsizetype lo=0,hi=s.y.size();
                while(lo<hi) {
                    qsizetype mid=lo+(hi-lo)/2; double x=xAt(s,mid);
                    if(x<target || (upper && x==target)) lo=mid+1; else hi=mid;
                }
                return lo;
            };
            first=bound(low,false); last=bound(high,true);
        }
        for (qsizetype i=first;i<last;++i) {
            const QPointF point{xAt(s,i),s.y[i]};
            const QPointF pixel{plot.left()+(point.x()-m_x.min)/(m_x.max-m_x.min)*plot.width(),
                                plot.bottom()-(point.y()-m_y.min)/(m_y.max-m_y.min)*plot.height()};
            if (!plot.contains(pixel)) continue;
            auto delta=pixel-pos; double d=QPointF::dotProduct(delta,delta);
            if (d<=distance) { distance=d; result=Sample{s.id,i,point.x(),point.y()}; }
        }
    }
    return result;
}
std::optional<LineChart::Sample> LineChart::sampleAt(QPointF position,double radius) const {
    return nearestSample(position,radius);
}
void LineChart::moveCursor(int which,QPointF position) {
    // 放置游标允许在绘图区内选最近的实际点；悬浮读数仍严格限制命中半径。
    if (auto sample=nearestSample(position,std::hypot(width(),height())))
        setCursor(which,sample->seriesId,sample->index);
}
Qt::Orientations LineChart::axesFor(Qt::KeyboardModifiers mods) const {
    if (mods.testFlag(Qt::ShiftModifier)) return Qt::Horizontal;
    if (mods.testFlag(Qt::ControlModifier)) return Qt::Vertical;
    return m_axes;
}
void LineChart::mousePressEvent(QMouseEvent* e) {
    if (!plotRect().contains(e->position())) { Coordinate2D::mousePressEvent(e); return; }
    setFocus(Qt::MouseFocusReason);
    m_hoverPosition=e->position(); m_hover.reset(); updateOverlay();
    if (m_cursorsEnabled && e->modifiers().testFlag(Qt::AltModifier) &&
        (e->button()==Qt::LeftButton || e->button()==Qt::RightButton)) {
        m_dragCursor=e->button()==Qt::LeftButton?0:1; moveCursor(m_dragCursor,e->position()); e->accept(); return;
    }
    if (e->button()==Qt::LeftButton) {
        m_panning=true; m_pressData=mapToChart(e->position()).value();
        m_dragAxes=axesFor(e->modifiers()); QWidget::setCursor(Qt::ClosedHandCursor); e->accept(); return;
    }
    Coordinate2D::mousePressEvent(e);
}
void LineChart::mouseMoveEvent(QMouseEvent* e) {
    m_hoverPosition=e->position();
    if (m_dragCursor>=0) { moveCursor(m_dragCursor,e->position()); e->accept(); return; }
    if (m_panning) {
        setViewRangeAt(m_x,m_y,e->position(),m_pressData,m_dragAxes);
        e->accept(); return;
    }
    m_hover=m_hoverEnabled?sampleAt(*m_hoverPosition):std::nullopt;
    updateOverlay(); Coordinate2D::mouseMoveEvent(e);
}
void LineChart::mouseReleaseEvent(QMouseEvent* e) {
    if (m_panning || m_dragCursor>=0) { m_panning=false; m_dragCursor=-1; unsetCursor(); e->accept(); return; }
    Coordinate2D::mouseReleaseEvent(e);
}
void LineChart::mouseDoubleClickEvent(QMouseEvent* e) {
    if (e->button()==Qt::LeftButton && plotRect().contains(e->position())) { m_panning=false; fitToData(); e->accept(); return; }
    Coordinate2D::mouseDoubleClickEvent(e);
}
void LineChart::wheelEvent(QWheelEvent* e) {
    if (!plotRect().contains(e->position())) { e->ignore(); return; }
    const auto anchor=mapToChart(e->position()); if (!anchor) return;
    const auto axes=axesFor(e->modifiers());
    const double steps=e->angleDelta().y()!=0 ? e->angleDelta().y()/120.0 : e->pixelDelta().y()/40.0;
    const double factor=std::pow(1.2,-std::clamp(steps,-20.0,20.0));
    auto scale=[&](Range r,double a) { return Range{a+(r.min-a)*factor,a+(r.max-a)*factor}; };
    setViewRangeAt(axes.testFlag(Qt::Horizontal)?scale(m_x,anchor->x()):m_x,
                   axes.testFlag(Qt::Vertical)?scale(m_y,anchor->y()):m_y,e->position(),*anchor,axes);
    e->accept();
}
void LineChart::leaveEvent(QEvent* e) { m_hoverPosition.reset(); m_hover.reset(); updateOverlay(); Coordinate2D::leaveEvent(e); }
void LineChart::keyPressEvent(QKeyEvent* e) {
    if (e->key()==Qt::Key_Home) { fitToData(); e->accept(); return; }
    if (e->key()==Qt::Key_Escape) { m_panning=false; m_dragCursor=-1; unsetCursor(); clearCursors(); e->accept(); return; }
    Coordinate2D::keyPressEvent(e);
}
void LineChart::resizeEvent(QResizeEvent* e) {
    Coordinate2D::resizeEvent(e);
    updateTicks();
    if (m_overlay) m_overlay->setGeometry(rect());
}

void LineChart::initializeGLObjects() {
    Coordinate2D::initializeGLObjects();
    m_gpu=std::make_unique<Gpu>();
    auto& g=*m_gpu;
    glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE,g.lineWidthRange);
    GLint contextFlags=0; glGetIntegerv(GL_CONTEXT_FLAGS,&contextFlags);
    // 前向兼容上下文移除了宽线；部分驱动查询出的线宽范围仍包含已移除的值。
    if(contextFlags&GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT) g.lineWidthRange[0]=g.lineWidthRange[1]=1;
    const char* vertex=R"(#version 410 core
        layout(location=0) in vec4 position;
        uniform mat4 viewProjection;
        uniform vec2 scale, viewHigh, viewLow;
        uniform float pointSize;
        void main() {
            // 高低位补偿减法：先消去可视范围起点，再缩放，保留深度放大时的相邻点。
            precise vec2 high = position.xy - viewHigh;
            precise vec2 virtualB = high - position.xy;
            precise vec2 error = (position.xy - (high - virtualB)) - (viewHigh + virtualB);
            precise vec2 relative = high + ((position.zw - viewLow) + error);
            gl_Position=viewProjection*vec4(relative*scale,0,1); gl_PointSize=pointSize;
        }
    )";
    const char* fragment=R"(#version 410 core
        uniform vec4 color; out vec4 fragColor;
        void main() { fragColor=color; }
    )";
    if (!g.program.addShaderFromSourceCode(QOpenGLShader::Vertex,vertex) ||
        !g.program.addShaderFromSourceCode(QOpenGLShader::Fragment,fragment) || !g.program.link()) {
        qWarning() << "LineChart shader:" << g.program.log(); m_gpu.reset(); return;
    }
    g.vao.create(); g.vao.bind(); g.buffer.create(); g.buffer.bind();
    g.buffer.setUsagePattern(QOpenGLBuffer::StreamDraw);
    g.program.enableAttributeArray(0); g.program.setAttributeBuffer(0,GL_FLOAT,0,4,sizeof(QVector4D));
    g.buffer.release(); g.vao.release(); m_dirty=true;
    m_contextConnection=connect(context(),&QOpenGLContext::aboutToBeDestroyed,this,&LineChart::cleanupGL,Qt::DirectConnection);
}
void LineChart::paintGLObjects() {
    Coordinate2D::paintGLObjects();
    if (!m_gpu || !renderOptions().data) return;
    auto timing=measureRenderStage(RenderStage::Data);
    auto& g=*m_gpu;
    g.vao.bind(); g.buffer.bind();
    if (m_dirty) {
        auto& vertices=g.vertices;
        vertices.clear();
        qsizetype total=0; for (const auto& s:m_series) total+=s.y.size();
        vertices.reserve(total); g.spans.clear();
        for (const auto& s:m_series) {
            g.spans.append({int(vertices.size()),int(s.y.size())});
            for (qsizetype i=0;i<s.y.size();++i) {
                const double x=(xAt(s,i)-m_dataX.min)/(m_dataX.max-m_dataX.min);
                const double y=(s.y[i]-m_dataY.min)/(m_dataY.max-m_dataY.min);
                const float highX=float(x),highY=float(y);
                vertices.append(QVector4D(highX,highY,float(x-double(highX)),float(y-double(highY))));
            }
        }
        const int bytes=int(vertices.size()*sizeof(QVector4D));
        if (bytes>g.capacity) { g.buffer.allocate(bytes); g.capacity=bytes; }
        if (bytes) g.buffer.write(0,vertices.constData(),bytes);
        m_dirty=false; ++m_uploads;
    }
    g.buffer.release();
    const auto plot=plotRect().intersected(QRectF(rect()));
    const double dpr=devicePixelRatioF();
    // Scissor uses physical pixels; mapping and Qt overlays use logical pixels.
    const int left=int(std::ceil(plot.left()*dpr)), right=int(std::floor(plot.right()*dpr));
    const int bottom=int(std::ceil((height()-plot.bottom())*dpr)), top=int(std::floor((height()-plot.top())*dpr));
    glEnable(GL_SCISSOR_TEST); glScissor(left,bottom,std::max(0,right-left),std::max(0,top-bottom));
    glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    g.program.bind();
    g.program.setUniformValue("viewProjection",camera().getProjectionMatrix()*camera().getViewMatrix());
    const double size=config().size;
    g.program.setUniformValue("scale",QVector2D(float(size*(m_dataX.max-m_dataX.min)/(m_x.max-m_x.min)),
                                                float(size*(m_dataY.max-m_dataY.min)/(m_y.max-m_y.min))));
    const double viewX=(m_x.min-m_dataX.min)/(m_dataX.max-m_dataX.min);
    const double viewY=(m_y.min-m_dataY.min)/(m_dataY.max-m_dataY.min);
    const float highX=float(viewX),highY=float(viewY);
    g.program.setUniformValue("viewHigh",QVector2D(highX,highY));
    g.program.setUniformValue("viewLow",QVector2D(float(viewX-double(highX)),float(viewY-double(highY))));
    g.program.setUniformValue("pointSize",float(3*dpr));
    const float lineWidth=std::clamp(float(dpr),g.lineWidthRange[0],g.lineWidthRange[1]);
    glLineWidth(lineWidth);
    // 把线宽、单点大小与像素取整纳入边缘余量，保留中心在范围外但仍覆盖绘图区的笔画。
    const double padding=plot.width()>0 ? (m_x.max-m_x.min)/plot.width()*
        (std::max(3*dpr,double(lineWidth))*0.5+1)/dpr : 0;
    for (qsizetype i=0;i<m_series.size();++i) if (m_series[i].visible && g.spans[i].second) {
        const auto& s=m_series[i];
        const auto visible=visibleSpan(s.y.size(),m_x.min-padding,m_x.max+padding,
                                      [&](qsizetype index) { return xAt(s,index); });
        if(!visible.second) continue;
        const auto& c=m_series[i].color;
        g.program.setUniformValue("color",QVector4D(c.redF(),c.greenF(),c.blueF(),c.alphaF()));
        glDrawArrays(g.spans[i].second==1?GL_POINTS:GL_LINE_STRIP,g.spans[i].first+visible.first,visible.second);
    }
    g.program.release(); g.vao.release();
    glLineWidth(1); glDisable(GL_SCISSOR_TEST); glDepthMask(GL_TRUE);
}
} // namespace ProGraphics
