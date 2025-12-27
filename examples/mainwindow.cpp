#include "mainwindow.h"
#include <QSplitter>
#include <QScrollArea>
#include <QFrame>
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
    scrollArea->setMaximumWidth(320);

    QWidget *controlWidget = new QWidget();
    QVBoxLayout *controlLayout = new QVBoxLayout(controlWidget);
    controlLayout->setSpacing(8);

    // ==================== 数据生成 ====================
    QGroupBox *dataGroup = new QGroupBox("数据生成");
    QGridLayout *dataLayout = new QGridLayout(dataGroup);

    dataLayout->addWidget(new QLabel("数据范围:"), 0, 0);
    QHBoxLayout *dataRangeLayout = new QHBoxLayout();
    dataRangeLayout->setSpacing(4);
    m_dataMinSpin = new QDoubleSpinBox();
    m_dataMinSpin->setRange(-10000, 10000);
    m_dataMinSpin->setValue(-60);
    dataRangeLayout->addWidget(m_dataMinSpin);
    dataRangeLayout->addWidget(new QLabel("~"));
    m_dataMaxSpin = new QDoubleSpinBox();
    m_dataMaxSpin->setRange(-10000, 10000);
    m_dataMaxSpin->setValue(-40);
    dataRangeLayout->addWidget(m_dataMaxSpin);
    dataLayout->addLayout(dataRangeLayout, 0, 1);

    m_generateButton = new QPushButton("1 帧");
    QPushButton *gen10Button = new QPushButton("10 帧");
    QPushButton *gen50Button = new QPushButton("50 帧");
    QPushButton *gen100Button = new QPushButton("100 帧");

    QHBoxLayout *genBtnLayout = new QHBoxLayout();
    genBtnLayout->setSpacing(4);
    genBtnLayout->addWidget(m_generateButton);
    genBtnLayout->addWidget(gen10Button);
    genBtnLayout->addWidget(gen50Button);
    genBtnLayout->addWidget(gen100Button);
    dataLayout->addLayout(genBtnLayout, 1, 0, 1, 2);

    connect(m_generateButton, &QPushButton::clicked, this, &MainWindow::onGenerateData);
    connect(gen10Button, &QPushButton::clicked, this, [this]() { onGenerateBatch(10); });
    connect(gen50Button, &QPushButton::clicked, this, [this]() { onGenerateBatch(50); });
    connect(gen100Button, &QPushButton::clicked, this, [this]() { onGenerateBatch(100); });

    // ==================== 量程模式 ====================
    QGroupBox *modeGroup = new QGroupBox("量程模式");
    QVBoxLayout *modeLayout = new QVBoxLayout(modeGroup);

    m_rangeModeCombo = new QComboBox();
    m_rangeModeCombo->addItem("固定模式 (Fixed)");
    m_rangeModeCombo->addItem("自动模式 (Auto)");
    m_rangeModeCombo->addItem("自适应模式 (Adaptive)");
    modeLayout->addWidget(m_rangeModeCombo);

    m_modeDescLabel = new QLabel();
    m_modeDescLabel->setWordWrap(true);
    m_modeDescLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; padding: 4px; }");
    modeLayout->addWidget(m_modeDescLabel);

    connect(m_rangeModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onRangeModeChanged);

    // ==================== 范围设置 ====================
    QGroupBox *rangeGroup = new QGroupBox("范围设置");
    QGridLayout *rangeLayout = new QGridLayout(rangeGroup);

    rangeLayout->addWidget(new QLabel("显示范围:"), 0, 0);
    QHBoxLayout *rangeInputLayout = new QHBoxLayout();
    rangeInputLayout->setSpacing(4);
    m_rangeMinSpin = new QDoubleSpinBox();
    m_rangeMinSpin->setRange(-10000, 10000);
    m_rangeMinSpin->setValue(-75);
    rangeInputLayout->addWidget(m_rangeMinSpin);
    rangeInputLayout->addWidget(new QLabel("~"));
    m_rangeMaxSpin = new QDoubleSpinBox();
    m_rangeMaxSpin->setRange(-10000, 10000);
    m_rangeMaxSpin->setValue(-30);
    rangeInputLayout->addWidget(m_rangeMaxSpin);
    rangeLayout->addLayout(rangeInputLayout, 0, 1);

    QPushButton *applyRangeBtn = new QPushButton("应用");
    rangeLayout->addWidget(applyRangeBtn, 1, 0, 1, 2);
    connect(applyRangeBtn, &QPushButton::clicked, this, &MainWindow::onApplyRange);

    // ==================== 动态量程配置 ====================
    m_dynamicConfigWidget = new QWidget();
    QVBoxLayout *dynamicLayout = new QVBoxLayout(m_dynamicConfigWidget);
    dynamicLayout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *dynamicGroup = new QGroupBox("动态量程参数");
    QGridLayout *configLayout = new QGridLayout(dynamicGroup);

    configLayout->addWidget(new QLabel("缓冲比例:"), 0, 0);
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

    dynamicLayout->addWidget(dynamicGroup);

    // ==================== 硬限制 ====================
    QGroupBox *hardLimitGroup = new QGroupBox("硬限制（防异常数据）");
    QGridLayout *hardLayout = new QGridLayout(hardLimitGroup);

    m_enableHardLimitsCheck = new QCheckBox("启用");
    hardLayout->addWidget(m_enableHardLimitsCheck, 0, 0);

    QHBoxLayout *hardRangeLayout = new QHBoxLayout();
    hardRangeLayout->setSpacing(4);
    m_hardLimitMinSpin = new QDoubleSpinBox();
    m_hardLimitMinSpin->setRange(-10000, 10000);
    m_hardLimitMinSpin->setValue(-200);
    m_hardLimitMinSpin->setEnabled(false);
    hardRangeLayout->addWidget(m_hardLimitMinSpin);
    hardRangeLayout->addWidget(new QLabel("~"));
    m_hardLimitMaxSpin = new QDoubleSpinBox();
    m_hardLimitMaxSpin->setRange(-10000, 10000);
    m_hardLimitMaxSpin->setValue(100);
    m_hardLimitMaxSpin->setEnabled(false);
    hardRangeLayout->addWidget(m_hardLimitMaxSpin);
    hardLayout->addLayout(hardRangeLayout, 0, 1);

    connect(m_enableHardLimitsCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_hardLimitMinSpin->setEnabled(checked);
        m_hardLimitMaxSpin->setEnabled(checked);
        m_prpdChart->setHardLimits(m_hardLimitMinSpin->value(), m_hardLimitMaxSpin->value(), checked);
        m_prpsChart->setHardLimits(m_hardLimitMinSpin->value(), m_hardLimitMaxSpin->value(), checked);
    });

    // ==================== 操作按钮 ====================
    QHBoxLayout *actionLayout = new QHBoxLayout();
    QPushButton *resetBtn = new QPushButton("重置");
    QPushButton *clearBtn = new QPushButton("清空数据");
    actionLayout->addWidget(resetBtn);
    actionLayout->addWidget(clearBtn);

    connect(resetBtn, &QPushButton::clicked, this, &MainWindow::onResetAll);
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        m_prpdChart->resetData();
        m_prpsChart->resetData();
    });

    // ==================== 状态显示 ====================
    QGroupBox *statusGroup = new QGroupBox("当前状态");
    QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);
    m_statusLabel = new QLabel();
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet("QLabel { font-family: monospace; font-size: 11px; }");
    statusLayout->addWidget(m_statusLabel);

    // 组装控制面板
    controlLayout->addWidget(dataGroup);
    controlLayout->addWidget(modeGroup);
    controlLayout->addWidget(rangeGroup);
    controlLayout->addWidget(m_dynamicConfigWidget);
    controlLayout->addWidget(hardLimitGroup);
    controlLayout->addLayout(actionLayout);
    controlLayout->addWidget(statusGroup);
    controlLayout->addStretch();

    scrollArea->setWidget(controlWidget);

    // 图表区域
    m_prpsChart = new ProGraphics::PRPSChart(central);
    m_prpdChart = new ProGraphics::PRPDChart(central);

    QSplitter *chartSplitter = new QSplitter(Qt::Vertical);
    chartSplitter->addWidget(m_prpsChart);
    chartSplitter->addWidget(m_prpdChart);

    mainLayout->addWidget(scrollArea);
    mainLayout->addWidget(chartSplitter, 1);

    resize(1200, 800);
}

void MainWindow::onRangeModeChanged(int index) {
    updateUIForMode(index);
    onApplyRange();
}

void MainWindow::updateUIForMode(int modeIndex) {
    // 更新模式说明
    switch (modeIndex) {
        case 0: // Fixed
            m_modeDescLabel->setText("范围固定不变，不随数据调整");
            m_dynamicConfigWidget->setVisible(false);
            break;
        case 1: // Auto
            m_modeDescLabel->setText("完全根据数据自动调整范围");
            m_dynamicConfigWidget->setVisible(true);
            break;
        case 2: // Adaptive
            m_modeDescLabel->setText("在初始范围基础上智能扩展");
            m_dynamicConfigWidget->setVisible(true);
            break;
    }
}

void MainWindow::onApplyRange() {
    float min = m_rangeMinSpin->value();
    float max = m_rangeMaxSpin->value();

    ProGraphics::DynamicRange::DynamicRangeConfig config;
    config.bufferRatio = m_bufferRatioSpin->value();
    config.responseSpeed = m_responseSpeedSpin->value();
    config.recoveryFrameThreshold = m_recoveryFramesSpin->value();
    config.recoveryRangeRatio = m_recoveryRatioSpin->value();
    config.smartAdjustment = m_smartAdjustCheck->isChecked();
    config.enableRangeRecovery = m_enableRecoveryCheck->isChecked();

    switch (m_rangeModeCombo->currentIndex()) {
        case 0: // Fixed
            m_prpdChart->setFixedRange(min, max);
            m_prpsChart->setFixedRange(min, max);
            break;
        case 1: // Auto
            m_prpdChart->setAutoRange(config);
            m_prpsChart->setAutoRange(config);
            break;
        case 2: // Adaptive
            m_prpdChart->setAdaptiveRange(min, max, config);
            m_prpsChart->setAdaptiveRange(min, max, config);
            break;
    }
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

void MainWindow::onResetAll() {
    m_rangeModeCombo->setCurrentIndex(0);
    m_rangeMinSpin->setValue(-75);
    m_rangeMaxSpin->setValue(-30);

    m_bufferRatioSpin->setValue(0.3);
    m_responseSpeedSpin->setValue(0.7);
    m_recoveryFramesSpin->setValue(20);
    m_recoveryRatioSpin->setValue(0.8);
    m_smartAdjustCheck->setChecked(true);
    m_enableRecoveryCheck->setChecked(true);

    m_enableHardLimitsCheck->setChecked(false);
    m_hardLimitMinSpin->setValue(-200);
    m_hardLimitMaxSpin->setValue(100);

    m_dataMinSpin->setValue(-60);
    m_dataMaxSpin->setValue(-40);

    updateUIForMode(0);
    onApplyRange();
}

void MainWindow::updateStatus() {
    auto [currentMin, currentMax] = m_prpdChart->getCurrentRange();
    auto [configuredMin, configuredMax] = m_prpdChart->getConfiguredRange();

    QString modeStr;
    switch (m_prpdChart->getRangeMode()) {
        case ProGraphics::PRPDChart::RangeMode::Fixed:
            modeStr = "固定";
            break;
        case ProGraphics::PRPDChart::RangeMode::Auto:
            modeStr = "自动";
            break;
        case ProGraphics::PRPDChart::RangeMode::Adaptive:
            modeStr = "自适应";
            break;
    }

    QString status;
    status += QString("模式: %1\n").arg(modeStr);
    status += QString("当前: [%1, %2]\n").arg(currentMin, 0, 'f', 1).arg(currentMax, 0, 'f', 1);

    if (m_prpdChart->getRangeMode() != ProGraphics::PRPDChart::RangeMode::Fixed) {
        status += QString("配置: [%1, %2]\n").arg(configuredMin, 0, 'f', 1).arg(configuredMax, 0, 'f', 1);
    }

    if (m_prpdChart->isHardLimitsEnabled()) {
        auto [hardMin, hardMax] = m_prpdChart->getHardLimits();
        status += QString("硬限制: [%1, %2]").arg(hardMin, 0, 'f', 1).arg(hardMax, 0, 'f', 1);
    }

    m_statusLabel->setText(status);
}

std::vector<float> MainWindow::generateRandomData() {
    static std::random_device rd;
    static std::mt19937 gen(rd());

    float min = m_dataMinSpin->value();
    float max = m_dataMaxSpin->value();

    std::vector<float> data(200);
    std::uniform_real_distribution<float> dist(min, max);
    std::uniform_real_distribution<float> noise(-1.0f, 1.0f);

    for (int i = 0; i < 200; ++i) {
        if (i < 160) {
            float phase = i / 200.0f * 2.0f * 3.14159f;
            float baseValue = (min + max) / 2.0f + (max - min) / 4.0f * std::sin(phase * 3.0f);
            data[i] = baseValue + noise(gen) * 0.5f;
        } else {
            data[i] = dist(gen);
        }
    }

    return data;
}
