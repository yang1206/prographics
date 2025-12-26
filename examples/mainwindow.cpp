#include "mainwindow.h"
#include <QSplitter>
#include <QScrollArea>
#include <random>
#include <cmath>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_batchRemaining(0) {
    setupUI();

    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::updateStatus);
    m_statusTimer->start(100);

    m_batchTimer = new QTimer(this);
    connect(m_batchTimer, &QTimer::timeout, this, [this]() {
        if (m_batchRemaining > 0) {
            onGenerateData();
            m_batchRemaining--;
            if (m_batchRemaining == 0) {
                m_batchTimer->stop();
                m_generateButton->setEnabled(true);
            }
        }
    });

    onResetAll();
}

void MainWindow::setupUI() {
    QWidget *central = new QWidget();
    setCentralWidget(central);

    QHBoxLayout *mainLayout = new QHBoxLayout(central);

    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setMaximumWidth(350);

    QWidget *controlWidget = new QWidget();
    QVBoxLayout *controlLayout = new QVBoxLayout(controlWidget);

    QGroupBox *dataGroup = new QGroupBox("数据生成");
    QGridLayout *dataLayout = new QGridLayout(dataGroup);
    dataLayout->addWidget(new QLabel("最小值:"), 0, 0);
    m_dataMinSpin = new QDoubleSpinBox();
    m_dataMinSpin->setRange(-10000, 10000);
    m_dataMinSpin->setValue(-60);
    dataLayout->addWidget(m_dataMinSpin, 0, 1);

    dataLayout->addWidget(new QLabel("最大值:"), 1, 0);
    m_dataMaxSpin = new QDoubleSpinBox();
    m_dataMaxSpin->setRange(-10000, 10000);
    m_dataMaxSpin->setValue(-40);
    dataLayout->addWidget(m_dataMaxSpin, 1, 1);

    m_generateButton = new QPushButton("生成一帧数据");
    dataLayout->addWidget(m_generateButton, 2, 0, 1, 2);
    connect(m_generateButton, &QPushButton::clicked, this, &MainWindow::onGenerateData);

    QPushButton *gen10Button = new QPushButton("生成 10 帧");
    dataLayout->addWidget(gen10Button, 3, 0);
    connect(gen10Button, &QPushButton::clicked, this, [this]() { onGenerateBatch(10); });

    QPushButton *gen50Button = new QPushButton("生成 50 帧");
    dataLayout->addWidget(gen50Button, 3, 1);
    connect(gen50Button, &QPushButton::clicked, this, [this]() { onGenerateBatch(50); });

    QPushButton *gen100Button = new QPushButton("生成 100 帧");
    dataLayout->addWidget(gen100Button, 4, 0, 1, 2);
    connect(gen100Button, &QPushButton::clicked, this, [this]() { onGenerateBatch(100); });

    QGroupBox *configGroup = new QGroupBox("动态量程配置");
    QGridLayout *configLayout = new QGridLayout(configGroup);

    configLayout->addWidget(new QLabel("缓冲区比例:"), 0, 0);
    m_bufferRatioSpin = new QDoubleSpinBox();
    m_bufferRatioSpin->setRange(0.0, 1.0);
    m_bufferRatioSpin->setValue(0.3);
    m_bufferRatioSpin->setSingleStep(0.05);
    m_bufferRatioSpin->setDecimals(2);
    configLayout->addWidget(m_bufferRatioSpin, 0, 1);

    configLayout->addWidget(new QLabel("响应速度:"), 1, 0);
    m_responseSpeedSpin = new QDoubleSpinBox();
    m_responseSpeedSpin->setRange(0.1, 1.0);
    m_responseSpeedSpin->setValue(0.7);
    m_responseSpeedSpin->setSingleStep(0.1);
    m_responseSpeedSpin->setDecimals(1);
    configLayout->addWidget(m_responseSpeedSpin, 1, 1);

    configLayout->addWidget(new QLabel("恢复帧数:"), 2, 0);
    m_recoveryFramesSpin = new QSpinBox();
    m_recoveryFramesSpin->setRange(1, 100);
    m_recoveryFramesSpin->setValue(20);
    configLayout->addWidget(m_recoveryFramesSpin, 2, 1);

    configLayout->addWidget(new QLabel("恢复比例:"), 3, 0);
    m_recoveryRatioSpin = new QDoubleSpinBox();
    m_recoveryRatioSpin->setRange(0.1, 1.0);
    m_recoveryRatioSpin->setValue(0.8);
    m_recoveryRatioSpin->setSingleStep(0.1);
    m_recoveryRatioSpin->setDecimals(1);
    configLayout->addWidget(m_recoveryRatioSpin, 3, 1);

    m_smartAdjustCheck = new QCheckBox("智能调整");
    m_smartAdjustCheck->setChecked(true);
    configLayout->addWidget(m_smartAdjustCheck, 4, 0, 1, 2);

    m_enableRecoveryCheck = new QCheckBox("启用范围恢复");
    m_enableRecoveryCheck->setChecked(true);
    configLayout->addWidget(m_enableRecoveryCheck, 5, 0, 1, 2);

    QPushButton *applyConfigBtn = new QPushButton("应用配置");
    configLayout->addWidget(applyConfigBtn, 6, 0, 1, 2);
    connect(applyConfigBtn, &QPushButton::clicked, this, &MainWindow::onApplyConfig);

    QGroupBox *rangeGroup = new QGroupBox("范围控制");
    QGridLayout *rangeLayout = new QGridLayout(rangeGroup);

    rangeLayout->addWidget(new QLabel("初始最小值:"), 0, 0);
    m_initialMinSpin = new QDoubleSpinBox();
    m_initialMinSpin->setRange(-10000, 10000);
    m_initialMinSpin->setValue(-75);
    rangeLayout->addWidget(m_initialMinSpin, 0, 1);

    rangeLayout->addWidget(new QLabel("初始最大值:"), 1, 0);
    m_initialMaxSpin = new QDoubleSpinBox();
    m_initialMaxSpin->setRange(-10000, 10000);
    m_initialMaxSpin->setValue(-30);
    rangeLayout->addWidget(m_initialMaxSpin, 1, 1);

    QPushButton *applyInitialBtn = new QPushButton("应用初始范围");
    rangeLayout->addWidget(applyInitialBtn, 2, 0, 1, 2);
    connect(applyInitialBtn, &QPushButton::clicked, this, [this]() {
        float min = m_initialMinSpin->value();
        float max = m_initialMaxSpin->value();
        m_prpdChart->setInitialRange(min, max);
        m_prpsChart->setInitialRange(min, max);
    });

    m_enableHardLimitsCheck = new QCheckBox("启用硬限制");
    m_enableHardLimitsCheck->setChecked(false);
    rangeLayout->addWidget(m_enableHardLimitsCheck, 3, 0, 1, 2);
    connect(m_enableHardLimitsCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_hardLimitMinSpin->setEnabled(checked);
        m_hardLimitMaxSpin->setEnabled(checked);
        m_prpdChart->enableHardLimits(checked);
        m_prpsChart->enableHardLimits(checked);
    });

    rangeLayout->addWidget(new QLabel("硬限制最小值:"), 4, 0);
    m_hardLimitMinSpin = new QDoubleSpinBox();
    m_hardLimitMinSpin->setRange(-10000, 10000);
    m_hardLimitMinSpin->setValue(-200);
    m_hardLimitMinSpin->setEnabled(false);
    rangeLayout->addWidget(m_hardLimitMinSpin, 4, 1);

    rangeLayout->addWidget(new QLabel("硬限制最大值:"), 5, 0);
    m_hardLimitMaxSpin = new QDoubleSpinBox();
    m_hardLimitMaxSpin->setRange(-10000, 10000);
    m_hardLimitMaxSpin->setValue(100);
    m_hardLimitMaxSpin->setEnabled(false);
    rangeLayout->addWidget(m_hardLimitMaxSpin, 5, 1);

    QPushButton *applyHardBtn = new QPushButton("应用硬限制");
    rangeLayout->addWidget(applyHardBtn, 6, 0, 1, 2);
    connect(applyHardBtn, &QPushButton::clicked, this, [this]() {
        float min = m_hardLimitMinSpin->value();
        float max = m_hardLimitMaxSpin->value();
        bool enabled = m_enableHardLimitsCheck->isChecked();
        m_prpdChart->setHardLimits(min, max, enabled);
        m_prpsChart->setHardLimits(min, max, enabled);
    });

    m_enableDynamicCheck = new QCheckBox("启用动态量程");
    m_enableDynamicCheck->setChecked(true);
    rangeLayout->addWidget(m_enableDynamicCheck, 7, 0, 1, 2);
    connect(m_enableDynamicCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_prpdChart->setDynamicRangeEnabled(checked);
        m_prpsChart->setDynamicRangeEnabled(checked);
    });

    QPushButton *resetBtn = new QPushButton("重置所有");
    rangeLayout->addWidget(resetBtn, 8, 0, 1, 2);
    connect(resetBtn, &QPushButton::clicked, this, &MainWindow::onResetAll);

    QPushButton *clearBtn = new QPushButton("清空数据");
    rangeLayout->addWidget(clearBtn, 9, 0, 1, 2);
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        m_prpdChart->resetData();
        m_prpsChart->resetData();
    });

    QGroupBox *statusGroup = new QGroupBox("状态");
    QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);
    m_statusLabel = new QLabel();
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet("QLabel { font-family: monospace; font-size: 10px; }");
    statusLayout->addWidget(m_statusLabel);

    controlLayout->addWidget(dataGroup);
    controlLayout->addWidget(configGroup);
    controlLayout->addWidget(rangeGroup);
    controlLayout->addWidget(statusGroup);
    controlLayout->addStretch();

    scrollArea->setWidget(controlWidget);

    m_prpsChart = new ProGraphics::PRPSChart(central);
    m_prpdChart = new ProGraphics::PRPDChart(central);

    QSplitter *chartSplitter = new QSplitter(Qt::Vertical);
    chartSplitter->addWidget(m_prpsChart);
    chartSplitter->addWidget(m_prpdChart);

    mainLayout->addWidget(scrollArea);
    mainLayout->addWidget(chartSplitter, 1);

    resize(1200, 800);
}

void MainWindow::onGenerateData() {
    std::vector<float> data = generateRandomData();
    m_prpdChart->addCycleData(data);
    m_prpsChart->addCycleData(data);
}

void MainWindow::onGenerateBatch(int count) {
    if (m_batchTimer->isActive()) {
        return;
    }

    m_batchRemaining = count;
    m_generateButton->setEnabled(false);
    m_batchTimer->start(20);
}

void MainWindow::onApplyConfig() {
    ProGraphics::DynamicRange::DynamicRangeConfig config;
    config.bufferRatio = m_bufferRatioSpin->value();
    config.responseSpeed = m_responseSpeedSpin->value();
    config.recoveryFrameThreshold = m_recoveryFramesSpin->value();
    config.recoveryRangeRatio = m_recoveryRatioSpin->value();
    config.smartAdjustment = m_smartAdjustCheck->isChecked();
    config.enableRangeRecovery = m_enableRecoveryCheck->isChecked();

    m_prpdChart->setDynamicRangeConfig(config);
    m_prpsChart->setDynamicRangeConfig(config);
}

void MainWindow::onResetAll() {
    m_initialMinSpin->setValue(-75);
    m_initialMaxSpin->setValue(-30);
    m_prpdChart->setInitialRange(-75, -30);
    m_prpsChart->setInitialRange(-75, -30);

    m_enableHardLimitsCheck->setChecked(false);
    m_hardLimitMinSpin->setValue(-200);
    m_hardLimitMaxSpin->setValue(100);

    m_bufferRatioSpin->setValue(0.3);
    m_responseSpeedSpin->setValue(0.7);
    m_recoveryFramesSpin->setValue(20);
    m_recoveryRatioSpin->setValue(0.8);
    m_smartAdjustCheck->setChecked(true);
    m_enableRecoveryCheck->setChecked(true);
    onApplyConfig();

    m_enableDynamicCheck->setChecked(true);

    m_dataMinSpin->setValue(-60);
    m_dataMaxSpin->setValue(-40);
}

void MainWindow::updateStatus() {
    auto [currentMin, currentMax] = m_prpdChart->getDisplayRange();
    auto [initialMin, initialMax] = m_prpdChart->getInitialRange();

    QString status;
    status += QString("当前范围: [%1, %2]\n").arg(currentMin, 0, 'f', 1).arg(currentMax, 0, 'f', 1);
    status += QString("初始范围: [%1, %2]\n").arg(initialMin, 0, 'f', 1).arg(initialMax, 0, 'f', 1);

    if (m_prpdChart->isHardLimitsEnabled()) {
        auto [hardMin, hardMax] = m_prpdChart->getHardLimits();
        status += QString("硬限制: [%1, %2]\n").arg(hardMin, 0, 'f', 1).arg(hardMax, 0, 'f', 1);
    } else {
        status += "硬限制: 未启用\n";
    }

    status += QString("动态量程: %1").arg(m_prpdChart->isDynamicRangeEnabled() ? "启用" : "禁用");

    m_statusLabel->setText(status);
}

std::vector<float> MainWindow::generateRandomData() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static int frameCount = 0;

    float min = m_dataMinSpin->value();
    float max = m_dataMaxSpin->value();

    std::vector<float> data(200);

    // 生成有规律的数据，让点重复出现以测试颜色渐变
    // 80% 的点在固定位置（会重复），20% 随机
    std::uniform_real_distribution<float> dist(min, max);
    std::uniform_real_distribution<float> noise(-1.0f, 1.0f);

    for (int i = 0; i < 200; ++i) {
        if (i < 160) {
            // 固定模式：正弦波 + 小噪声
            float phase = i / 200.0f * 2.0f * 3.14159f;
            float baseValue = (min + max) / 2.0f + (max - min) / 4.0f * std::sin(phase * 3.0f);
            data[i] = baseValue + noise(gen) * 0.5f;
        } else {
            // 完全随机
            data[i] = dist(gen);
        }
    }

    frameCount++;
    return data;
}

