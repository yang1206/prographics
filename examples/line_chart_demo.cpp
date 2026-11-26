#include "line_chart_demo.h"
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>

namespace {
QVector<ProGraphics::LineChart::Series> seriesLayout(bool irregular) {
    const QColor colors[]={QColor("#59d8ff"),QColor("#f4c95d"),QColor("#a6e39d"),QColor("#f69dbb"),
                           QColor("#acb6ff"),QColor("#ffac75"),QColor("#62dfbf"),QColor("#e2b5f4")};
    QVector<ProGraphics::LineChart::Series> series;
    series.reserve(8);
    for(int c=0;c<8;++c) {
        ProGraphics::LineChart::Series s;
        s.id=QStringLiteral("CH%1").arg(c+1); s.name=s.id; s.color=colors[c]; s.xStep=0.01;
        s.y.resize(2048); if(irregular) s.x.resize(2048);
        series.append(std::move(s));
    }
    return series;
}

QVector<ProGraphics::LineChart::Series> randomSeries(bool irregular,QRandomGenerator& generator) {
    auto series=seriesLayout(irregular);
    auto* random=&generator;
    for(qsizetype c=0;c<series.size();++c) {
        auto& s=series[c];
        const double amplitude=0.15+0.45*random->generateDouble();
        const double offset=c*2.5+(random->generateDouble()-0.5)*0.3;
        double x=0;
        for(qsizetype i=0;i<s.y.size();++i) {
            if(irregular) {
                if(i) x+=0.005+0.01*random->generateDouble();
                s.x[i]=x;
            }
            double y=offset+(random->generateDouble()*2-1)*amplitude;
            if(random->bounded(180u)==0)
                y+=(random->bounded(2u)?1:-1)*(0.8+random->generateDouble());
            s.y[i]=y;
        }
    }
    return series;
}

// Follow the ImGui/ImPlot frame loop: submit complete data, render/swap, then the next frame.
// Qt frameSwapped supplies backpressure. The optional example-side rate cap does not replace pending frames.
class LineDemoFeed final : public QObject {
public:
    LineDemoFeed(ProGraphics::LineChart* target,std::function<bool()> load,QObject* parent)
        : QObject(parent),chart(target),submit(std::move(load)) {
        setObjectName("lineFeed"); next.setSingleShot(true); next.setTimerType(Qt::PreciseTimer);
        sourceClock.start();
        connect(&next,&QTimer::timeout,this,[this] {
            if (!active || stats.pending) return;
            beforeUpload=chart->dataUploadCount();
            lastSubmissionNs=sourceClock.nsecsElapsed();
            if(submit()) { stats.pending=true; ++stats.submitted; }
            else { ++stats.rejected; active=false; }
        });
        connect(chart,&QOpenGLWidget::frameSwapped,this,[this] {
            if (stats.pending && chart->dataUploadCount()>beforeUpload) {
                stats.pending=false; ++stats.displayed;
            }
            schedule();
        });
    }
    void setRunning(bool on) {
        active=on;
        if(!on) next.stop();
        else schedule();
    }
    void setRateHz(int hz) {
        rateHz=std::clamp(hz,0,1000);
        next.stop(); schedule();
    }
    LineDemoFeedStats stats;
private:
    void schedule() {
        if(!active || stats.pending || next.isActive()) return;
        qint64 remaining=0;
        if(rateHz>0 && lastSubmissionNs>=0)
            remaining=std::max(qint64(0),1000000000LL/rateHz-(sourceClock.nsecsElapsed()-lastSubmissionNs));
        next.start(int((remaining+999999)/1000000));
    }
    ProGraphics::LineChart* chart;
    std::function<bool()> submit;
    QTimer next;
    QElapsedTimer sourceClock;
    qint64 lastSubmissionNs=-1;
    int rateHz=0;
    bool active=false;
    quint64 beforeUpload=0;
};
}
LineDemoFeedStats lineDemoFeedStats(QWidget* demo) {
    auto* feed=dynamic_cast<LineDemoFeed*>(demo->findChild<QObject*>("lineFeed"));
    return feed?feed->stats:LineDemoFeedStats{};
}

QVector<ProGraphics::LineChart::Series> makeLineDemoSeries(bool irregular,double phase) {
    auto series=seriesLayout(irregular);
    for (int c=0;c<8;++c) {
        auto& s=series[c];
        for (int i=0;i<2048;++i) {
            const double x=i*0.01+(irregular?0.002*std::sin(i*0.17):0);
            if (irregular) s.x[i]=x;
            s.y[i]=c*2.5+0.45*std::sin(x*(2+c*0.13)+phase)+0.12*std::cos(x*13+phase*0.7);
        }
        // 单个采样点的窄尖峰，位置固定，换帧时仍可逐点读数。
        s.y[256+c*173]=c*2.5+1.8;
    }
    return series;
}

QWidget* createLineChartDemo() {
    return createLineChartDemo(QRandomGenerator::global()->generate());
}

QWidget* createLineChartDemo(quint32 randomSeed) {
    auto* window=new QWidget;
    window->setWindowTitle(QStringLiteral("ProGraphics 折线图示例")); window->resize(1200,760);
    auto* layout=new QVBoxLayout(window);
    auto* toolbar=new QHBoxLayout;
    auto* source=new QComboBox; source->setObjectName("lineSource");
    source->addItems({QStringLiteral("等间隔 X · 8 × 2,048 点"),QStringLiteral("非等间隔 X · 8 × 2,048 点")});
    auto* axes=new QComboBox; axes->addItems({QStringLiteral("双轴"),QStringLiteral("仅 X"),QStringLiteral("仅 Y")});
    auto* presentation=new QComboBox; presentation->setObjectName("linePresentation");
    presentation->addItems({QStringLiteral("普通布局"),QStringLiteral("宽且较矮"),QStringLiteral("大字号宽图")});
    auto* running=new QCheckBox(QStringLiteral("持续供数")); running->setObjectName("lineRunning");
    auto* rate=new QSpinBox; rate->setObjectName("lineRate"); rate->setRange(0,1000);
    rate->setSpecialValueText(QStringLiteral("不限速")); rate->setSuffix(QStringLiteral(" 帧/秒"));
    rate->setSingleStep(5); rate->setAccelerated(true);
    rate->setToolTip(QStringLiteral("设置数据生成速度上限。0 为不限速，实际速度受当前帧显示完成时间约束。"));
    auto* hover=new QCheckBox(QStringLiteral("悬浮读数")); hover->setChecked(true);
    auto* cursors=new QCheckBox(QStringLiteral("双游标")); cursors->setObjectName("lineCursors");
    auto* fit=new QPushButton(QStringLiteral("恢复全幅"));
    auto* reload=new QPushButton(QStringLiteral("载入数据"));
    auto* clear=new QPushButton(QStringLiteral("清空"));
    toolbar->addWidget(source); toolbar->addWidget(axes); toolbar->addWidget(presentation); toolbar->addStretch();
    toolbar->addWidget(fit); toolbar->addWidget(reload); toolbar->addWidget(clear); layout->addLayout(toolbar);
    auto* feedToolbar=new QHBoxLayout;
    feedToolbar->addWidget(running); feedToolbar->addWidget(new QLabel(QStringLiteral("生成速度上限:")));
    feedToolbar->addWidget(rate); feedToolbar->addWidget(hover); feedToolbar->addWidget(cursors);
    feedToolbar->addStretch(); layout->addLayout(feedToolbar);
    auto* chart=new DemoLineChart; chart->setObjectName("lineChart"); chart->setMinimumSize(200,120);
    chart->setAxisName('x',QStringLiteral("时间"),QStringLiteral("μs"));
    chart->setAxisName('y',QStringLiteral("幅值"),QStringLiteral("a.u."));
    layout->addWidget(chart,1);
    auto* legend=new QHBoxLayout;
    for (const auto& s:seriesLayout(false)) {
        auto* visible=new QCheckBox(s.id); visible->setChecked(true); visible->setObjectName("visible"+s.id);
        visible->setStyleSheet(QStringLiteral("color: %1").arg(s.color.name())); legend->addWidget(visible);
        QObject::connect(visible,&QCheckBox::toggled,chart,[chart,id=s.id](bool on) { chart->setSeriesVisible(id,on); });
    }
    legend->addStretch(); layout->addLayout(legend);
    auto* instructions=new QLabel(QStringLiteral("滚轮：以鼠标为中心缩放 · 左键拖动：平移 · Shift：仅 X · Ctrl：仅 Y · 双击 / Home：全幅\n"
        "开启双游标后：Alt + 左键放置 / 拖动 A，Alt + 右键放置 / 拖动 B · Esc 清除游标 · 换帧保留当前视图"));
    instructions->setWordWrap(true); layout->addWidget(instructions);
    auto generator=std::make_shared<QRandomGenerator>(randomSeed);
    auto load=[window,chart,source,generator] {
        auto batch=randomSeries(source->currentIndex()==1,*generator);
        for (auto& s:batch) s.visible=window->findChild<QCheckBox*>("visible"+s.id)->isChecked();
        return chart->setSeries(std::move(batch));
    };
    auto* feed=new LineDemoFeed(chart,load,window);
    QObject::connect(presentation,&QComboBox::currentIndexChanged,window,[window,chart](int index) {
        const int pointSize=index==2?16:10;
        for(char axis:{'x','y'}) {
            auto ticks=axis=='x'?chart->config().ticks.x.style:chart->config().ticks.y.style;
            auto name=axis=='x'?chart->config().names.x.style:chart->config().names.y.style;
            ticks.fontSize=name.fontSize=pointSize; chart->setTicksStyle(axis,ticks); chart->setAxisNameStyle(axis,name);
        }
        window->resize(index?1240:1200,index==2?520:(index?420:760));
    });
    QObject::connect(rate,&QSpinBox::valueChanged,window,[feed](int hz) { feed->setRateHz(hz); });
    QObject::connect(running,&QCheckBox::toggled,window,[feed,source,reload](bool on) {
        source->setEnabled(!on); reload->setEnabled(!on); feed->setRunning(on);
    });
    QObject::connect(hover,&QCheckBox::toggled,chart,&ProGraphics::LineChart::setHoverEnabled);
    QObject::connect(cursors,&QCheckBox::toggled,chart,&ProGraphics::LineChart::setCursorsEnabled);
    QObject::connect(fit,&QPushButton::clicked,chart,[chart] { chart->fitToData(); });
    QObject::connect(clear,&QPushButton::clicked,chart,[chart,running] { running->setChecked(false); chart->clear(); });
    QObject::connect(reload,&QPushButton::clicked,window,load);
    QObject::connect(source,&QComboBox::currentIndexChanged,window,[chart,load] { load(); chart->fitToData(); });
    load();
    return window;
}
