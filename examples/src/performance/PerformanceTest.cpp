#include "PerformanceTest.h"
#include <QApplication>
#include <QThread>
#include <QElapsedTimer>
#include <QPushButton>
#include <QTime>
#include <random>
#include "DataProcessor.h"


PerformanceTest::PerformanceTest(QWidget *parent) : QWidget(parent) {
    setupUI();
    m_dataThread = new QThread(this);
    m_dataProcessor = new DataProcessor();
    m_dataProcessor->moveToThread(m_dataThread);

    // 连接信号和槽
    connect(m_dataThread, &QThread::started, m_dataProcessor, &DataProcessor::startProcessing);
    connect(m_dataProcessor, &DataProcessor::dataReady, this, &PerformanceTest::updateCharts, Qt::QueuedConnection);
    connect(this, &PerformanceTest::destroyed, m_dataThread, &QThread::quit);
    connect(m_dataThread, &QThread::finished, m_dataProcessor, &DataProcessor::deleteLater);
    connect(m_dataThread, &QThread::finished, m_dataThread, &QThread::deleteLater);

    m_dataThread->start();

    // 设置FPS计算定时器
    connect(&m_fpsTimer, &QTimer::timeout, this, &PerformanceTest::updateFPS);
    m_fpsTimer.setInterval(1000); // 每秒更新一次
    m_fpsTimer.start();

    // 初始化性能计时器
    m_performanceTimer.start();
    
    // 重置帧计数
    m_frameCount = 0;
}

void PerformanceTest::setupUI() {
    auto *mainLayout = new QVBoxLayout(this);

    // 添加FPS显示标签
    m_fpsLabel = new QLabel("FPS: 0 | 数据处理: 0ms | 渲染: 0ms", this);
    m_fpsLabel->setStyleSheet("QLabel { background-color: rgba(0, 0, 0, 128); color: white; padding: 5px; }");
    m_fpsLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_fpsLabel);

    // 添加验证按钮
    QPushButton *validateBtn = new QPushButton("验证共享上下文", this);
    connect(validateBtn, &QPushButton::clicked, this, &PerformanceTest::validateSharedContext);
    mainLayout->addWidget(validateBtn);

    // 创建4组图表
    for (int i = 0; i < 4; ++i) {
        auto *groupBox = new QGroupBox(QString("Chart Group %1").arg(i + 1), this);
        auto *groupLayout = new QHBoxLayout(groupBox);

        ChartPair pair;
        pair.prps = new ProGraphics::PRPSChart(this);
        pair.prpd = new ProGraphics::PRPDChart(this);
        pair.container = groupBox;

        // 设置每个图表的大小策略和最小大小
        pair.prps->setMinimumSize(300, 200);
        pair.prpd->setMinimumSize(300, 200);

        groupLayout->addWidget(pair.prps);
        groupLayout->addWidget(pair.prpd);

        mainLayout->addWidget(groupBox);
        m_chartPairs.push_back(std::move(pair));
    }
}

void PerformanceTest::updateFPS() {
    // 计算并显示FPS
    double fps = m_frameCount;
    m_frameCount = 0;

    // 更新FPS标签
    m_fpsLabel->setText(QString("FPS: %1 | 数据处理: %2ms | 渲染: %3ms")
        .arg(fps, 0, 'f', 1)
        .arg(m_lastDataProcessTime, 0, 'f', 2)
        .arg(m_lastRenderTime, 0, 'f', 2));
}

void PerformanceTest::updateCharts(const std::vector<float> &data) {
    QElapsedTimer timer;
    timer.start();

    // 更新所有图表
    for (auto &pair: m_chartPairs) {
        pair.prps->addCycleData(data);
        pair.prpd->addCycleData(data);
    }

    // 记录渲染时间
    m_lastRenderTime = timer.elapsed();

    // 增加帧计数
    m_frameCount++;
    
    // 标记数据已处理，允许生成新数据
    m_dataProcessor->markDataProcessed();
}

void PerformanceTest::validateSharedContext() {
    // 检查并打印所有图表的上下文和共享组
    qDebug() << "验证所有图表的上下文共享状态:";
    for (size_t i = 0; i < m_chartPairs.size(); ++i) {
        qDebug() << "图表组" << (i + 1) << ":";

        // 需要确保所有QOpenGLWidget都继承自BaseGLWidget，这样才能调用makeCurrent
        m_chartPairs[i].prps->makeCurrent();
        QOpenGLContext *prpsCtx = QOpenGLContext::currentContext();
        m_chartPairs[i].prps->doneCurrent();

        m_chartPairs[i].prpd->makeCurrent();
        QOpenGLContext *prpdCtx = QOpenGLContext::currentContext();
        m_chartPairs[i].prpd->doneCurrent();

        qDebug() << "  PRPS上下文:" << prpsCtx << "共享组:" << prpsCtx->shareGroup();
        qDebug() << "  PRPD上下文:" << prpdCtx << "共享组:" << prpdCtx->shareGroup();

        // 验证两个上下文是否共享
        bool isSharing = QOpenGLContext::areSharing(prpsCtx, prpdCtx);
        qDebug() << "  两个图表共享上下文:" << isSharing;
    }
}
