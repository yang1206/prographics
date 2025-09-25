#include "mainwindow.h"

#include "src/01_textured_rect/TexturedRectWidget.h"
#include "src/02_textured_unit/TexturedUnitWidget.h"
#include "src/03_matrix/MatrixWidget.h"
#include "src/04_coordinate/CoordinateWidget.h"
#include "src/05_control_camera/ControlCameraWidget.h"
#include "src/06_3D_coordinate/3DCoordinate.h"
#include "src/coordinate/TestCoordinate2d.h"
#include "src/coordinate/TestCoordinate3d.h"
#include "src/playground/PlayGround.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QtCore/qdatetime.h>
#include <QSplitter>

#include <cmath>
#include <random>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    m_tabWidget = new QTabWidget(this);
    setCentralWidget(m_tabWidget);

    // 创建融合了PRPD和PRPS的组合Tab
    setupCombinedTab();

    // 添加其他现有的Tab
    m_tabWidget->addTab(new TestCoordinate3d(this), "测试3d坐标系统");
    m_tabWidget->addTab(new TestCoordinate2d(this), "测试2d坐标系统");

    // 设置窗口大小
    resize(1200, 800);

    // 设置数据生成定时器
    connect(&m_dataTimer, &QTimer::timeout, this, &MainWindow::generateTestData);
    m_dataTimer.setInterval(20);
    m_dataTimer.start();
}

void MainWindow::setupCombinedTab() {
    // 创建组合Tab
    m_combinedTab = new QWidget(this);
    m_combinedLayout = new QGridLayout(m_combinedTab);

    // 创建左侧控制面板
    QWidget *controlPanel = new QWidget(m_combinedTab);
    QVBoxLayout *controlLayout = new QVBoxLayout(controlPanel);

    // 添加模式选择器
    QGroupBox *modeGroup = new QGroupBox("数据生成模式", controlPanel);
    QVBoxLayout *modeLayout = new QVBoxLayout(modeGroup);

    QComboBox *modeSelector = new QComboBox(modeGroup);
    modeSelector->addItem("手动设定范围", static_cast<int>(DataGenerationMode::MANUAL_RANGE));
    modeSelector->addItem("随机幅值模式", static_cast<int>(DataGenerationMode::RANDOM_AMPLITUDE));
    modeSelector->addItem("平滑变化模式", static_cast<int>(DataGenerationMode::SMOOTH_CHANGING));
    modeSelector->addItem("标准放电模式", static_cast<int>(DataGenerationMode::STANDARD_PD));

    connect(modeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onModeChanged);
    modeLayout->addWidget(modeSelector);

    // 添加数据范围控制
    setupDataRangeControls();

    // 添加动态量程配置控制
    setupConfigControls();
    setupInitialRangeControls();
    setupHardLimitsControls();

    setupTestControls();
    // 将控制组添加到控制面板
    controlLayout->addWidget(modeGroup);
    controlLayout->addWidget(m_dataRangeGroup);
    controlLayout->addWidget(m_configGroup);
    controlLayout->addWidget(m_initialRangeGroup);
    controlLayout->addWidget(m_hardLimitsGroup);
    controlLayout->addWidget(m_testGroup);
    controlLayout->addStretch();

    // 创建图表
    m_prpsChart = new ProGraphics::PRPSChart(m_combinedTab);
    m_prpdChart = new ProGraphics::PRPDChart(m_combinedTab);

    m_prpdChart->setBackgroundColor(QColor(251, 251, 251));
    m_prpdChart->setAxisColor('x', QColor("#6e7079"));
    ProGraphics::TextRenderer::TextStyle axisNameStyle;
    axisNameStyle.color = QColor("#6e7079");
    m_prpdChart->setAxisNameStyle('x', axisNameStyle);
    m_prpdChart->setTicksStyle('x', axisNameStyle);
    m_prpdChart->setAxisColor('y', QColor("#6e7079"));
    m_prpdChart->setAxisNameStyle('y', axisNameStyle);
    m_prpdChart->setTicksStyle('y', axisNameStyle);
    m_prpdChart->setGridColors(QColor("#e0e6f1"), QColor("#e7e6e6"));
    ProGraphics::Grid::SineWaveConfig sineWave;
    sineWave.visible = true;
    sineWave.color = QVector4D(QColor(0x6e7079).redF(), QColor("#6e7079").greenF(), QColor("#6e7079").blueF(), 1.0);
    m_prpdChart->setGridSineWaveConfig(sineWave);
    m_prpsChart->setBackgroundColor(QColor(251, 251, 251));
    m_prpsChart->setAxisNameStyle('x', axisNameStyle);
    m_prpsChart->setTicksStyle('x', axisNameStyle);
    m_prpsChart->setAxisNameStyle('y', axisNameStyle);
    m_prpsChart->setTicksStyle('y', axisNameStyle);
    m_prpsChart->setAxisNameStyle('z', axisNameStyle);
    m_prpsChart->setTicksStyle('z', axisNameStyle);

    m_prpsChart->setAxisAndGridColor('x', QColor("#6e7079"), QColor("#e0e6f1"), QColor("#e7e6e6"));
    m_prpsChart->setAxisAndGridColor('y', QColor("#6e7079"), QColor("#e0e6f1"), QColor("#e7e6e6"));
    m_prpsChart->setAxisAndGridColor('z', QColor("#6e7079"), QColor("#e0e6f1"), QColor("#e7e6e6"));

    m_prpsChart->setGridSineWaveConfig(QString("xy"), sineWave);


    // 创建图表分割器
    QSplitter *chartSplitter = new QSplitter(Qt::Vertical, m_combinedTab);
    chartSplitter->addWidget(m_prpsChart);
    chartSplitter->addWidget(m_prpdChart);
    chartSplitter->setStretchFactor(0, 1);
    chartSplitter->setStretchFactor(1, 1);

    // 将控制面板和图表添加到布局
    m_combinedLayout->addWidget(controlPanel, 0, 0);
    m_combinedLayout->addWidget(chartSplitter, 0, 1);
    m_combinedLayout->setColumnStretch(0, 1);
    m_combinedLayout->setColumnStretch(1, 3);

    // 将组合Tab添加到Tab栏
    m_tabWidget->addTab(m_combinedTab, "动态量程测试");
}

void MainWindow::setupDataRangeControls() {
    m_dataRangeGroup = new QGroupBox("数据范围控制", m_combinedTab);
    QGridLayout *rangeLayout = new QGridLayout(m_dataRangeGroup);

    // 最小值控制
    rangeLayout->addWidget(new QLabel("最小值:"), 0, 0);

    m_minRangeSlider = new QSlider(Qt::Horizontal, m_dataRangeGroup);
    m_minRangeSlider->setRange(-100, 1000);
    m_minRangeSlider->setValue(0);
    m_minRangeSlider->setTickPosition(QSlider::TicksBelow);

    m_minRangeInput = new QDoubleSpinBox(m_dataRangeGroup);
    m_minRangeInput->setRange(-100.0, 1000.0);
    m_minRangeInput->setValue(0.0);
    m_minRangeInput->setDecimals(1);
    m_minRangeInput->setSingleStep(1.0);

    rangeLayout->addWidget(m_minRangeSlider, 0, 1);
    rangeLayout->addWidget(m_minRangeInput, 0, 2);

    // 最大值控制
    rangeLayout->addWidget(new QLabel("最大值:"), 1, 0);

    m_maxRangeSlider = new QSlider(Qt::Horizontal, m_dataRangeGroup);
    m_maxRangeSlider->setRange(-100, 1000);
    m_maxRangeSlider->setValue(2);
    m_maxRangeSlider->setTickPosition(QSlider::TicksBelow);

    m_maxRangeInput = new QDoubleSpinBox(m_dataRangeGroup);
    m_maxRangeInput->setRange(-100.0, 1000.0);
    m_maxRangeInput->setValue(2.0);
    m_maxRangeInput->setDecimals(1);
    m_maxRangeInput->setSingleStep(1.0);

    rangeLayout->addWidget(m_maxRangeSlider, 1, 1);
    rangeLayout->addWidget(m_maxRangeInput, 1, 2);

    // 控制按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout();

    m_resetRangeButton = new QPushButton("重置范围", m_dataRangeGroup);
    m_autoChangeCheckbox = new QCheckBox("自动变化", m_dataRangeGroup);

    buttonLayout->addWidget(m_resetRangeButton);
    buttonLayout->addWidget(m_autoChangeCheckbox);

    rangeLayout->addLayout(buttonLayout, 2, 0, 1, 3);

    // 连接信号
    connect(m_minRangeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_minRangeInput->setValue(value);
        // 确保最小值不超过最大值
        if (m_minRangeInput->value() >= m_maxRangeInput->value()) {
            m_maxRangeInput->setValue(m_minRangeInput->value());
            m_maxRangeSlider->setValue(static_cast<int>(m_maxRangeInput->value()));
        }
        // 直接应用范围变化
        m_currentMinRange = static_cast<float>(m_minRangeInput->value());
        m_currentMaxRange = static_cast<float>(m_maxRangeInput->value());
    });

    connect(m_maxRangeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_maxRangeInput->setValue(value);
        // 确保最大值不小于最小值
        if (m_maxRangeInput->value() <= m_minRangeInput->value()) {
            m_minRangeInput->setValue(m_maxRangeInput->value());
            m_minRangeSlider->setValue(static_cast<int>(m_minRangeInput->value()));
        }
        // 直接应用范围变化
        m_currentMinRange = static_cast<float>(m_minRangeInput->value());
        m_currentMaxRange = static_cast<float>(m_maxRangeInput->value());
    });

    connect(m_minRangeInput, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        m_minRangeSlider->setValue(static_cast<int>(value));
        // 直接应用范围变化
        m_currentMinRange = static_cast<float>(value);
    });

    connect(m_maxRangeInput, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        m_maxRangeSlider->setValue(static_cast<int>(value));
        // 直接应用范围变化
        m_currentMaxRange = static_cast<float>(value);
    });

    connect(m_resetRangeButton, &QPushButton::clicked, this, &MainWindow::onResetRangeButtonClicked);
    connect(m_autoChangeCheckbox, &QCheckBox::toggled, this, &MainWindow::onAutoChangeToggled);
}

void MainWindow::setupConfigControls() {
    m_configGroup = new QGroupBox("动态量程配置", m_combinedTab);
    QGridLayout *configLayout = new QGridLayout(m_configGroup);

    // 缓冲区比例
    configLayout->addWidget(new QLabel("缓冲区比例:"), 0, 0);
    m_bufferRatioInput = new QDoubleSpinBox(m_configGroup);
    m_bufferRatioInput->setRange(0.1, 1.0);
    m_bufferRatioInput->setValue(0.4);
    m_bufferRatioInput->setSingleStep(0.05);
    configLayout->addWidget(m_bufferRatioInput, 0, 1);

    // 响应速度
    configLayout->addWidget(new QLabel("响应速度:"), 1, 0);
    m_responseSpeedInput = new QDoubleSpinBox(m_configGroup);
    m_responseSpeedInput->setRange(0.1, 1.0);
    m_responseSpeedInput->setValue(0.5);
    m_responseSpeedInput->setSingleStep(0.1);
    configLayout->addWidget(m_responseSpeedInput, 1, 1);

    // 智能调整
    m_smartAdjustmentCheckbox = new QCheckBox("智能调整", m_configGroup);
    m_smartAdjustmentCheckbox->setChecked(true);
    configLayout->addWidget(m_smartAdjustmentCheckbox, 2, 0, 1, 2);

    // 刻度数量
    configLayout->addWidget(new QLabel("刻度数量:"), 3, 0);
    m_targetTickCountInput = new QSpinBox(m_configGroup);
    m_targetTickCountInput->setRange(4, 10);
    m_targetTickCountInput->setValue(6);
    configLayout->addWidget(m_targetTickCountInput, 3, 1);


    // 应用按钮
    m_applyConfigButton = new QPushButton("应用配置", m_configGroup);
    configLayout->addWidget(m_applyConfigButton, 5, 0, 1, 2);

    // 连接信号
    connect(m_applyConfigButton, &QPushButton::clicked, this, &MainWindow::updateDynamicRangeConfig);
}

void MainWindow::setupInitialRangeControls() {
    m_initialRangeGroup = new QGroupBox("初始范围设置", m_combinedTab);
    QGridLayout *layout = new QGridLayout(m_initialRangeGroup);

    // 初始最小值
    layout->addWidget(new QLabel("初始最小值:"), 0, 0);
    m_initialMinInput = new QDoubleSpinBox(m_initialRangeGroup);
    m_initialMinInput->setRange(-1000.0, 1000.0);
    m_initialMinInput->setValue(-75.0);
    m_initialMinInput->setDecimals(1);
    layout->addWidget(m_initialMinInput, 0, 1);

    // 初始最大值
    layout->addWidget(new QLabel("初始最大值:"), 1, 0);
    m_initialMaxInput = new QDoubleSpinBox(m_initialRangeGroup);
    m_initialMaxInput->setRange(-1000.0, 1000.0);
    m_initialMaxInput->setValue(-30.0);
    m_initialMaxInput->setDecimals(1);
    layout->addWidget(m_initialMaxInput, 1, 1);

    // 应用按钮
    m_applyInitialRangeButton = new QPushButton("应用初始范围", m_initialRangeGroup);
    layout->addWidget(m_applyInitialRangeButton, 2, 0, 1, 2);

    // 当前初始范围显示
    m_currentInitialRangeLabel = new QLabel("当前: [-75.0, -30.0]", m_initialRangeGroup);
    m_currentInitialRangeLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; }");
    layout->addWidget(m_currentInitialRangeLabel, 3, 0, 1, 2);

    // 连接信号
    connect(m_applyInitialRangeButton, &QPushButton::clicked, this, &MainWindow::updateInitialRange);
}

void MainWindow::setupHardLimitsControls() {
    m_hardLimitsGroup = new QGroupBox("硬限制设置", m_combinedTab);
    QGridLayout *layout = new QGridLayout(m_hardLimitsGroup);

    // 启用硬限制复选框
    m_enableHardLimitsCheckbox = new QCheckBox("启用硬限制", m_hardLimitsGroup);
    m_enableHardLimitsCheckbox->setChecked(true);
    layout->addWidget(m_enableHardLimitsCheckbox, 0, 0, 1, 2);

    // 硬限制最小值
    layout->addWidget(new QLabel("硬限制最小值:"), 1, 0);
    m_hardLimitMinInput = new QDoubleSpinBox(m_hardLimitsGroup);
    m_hardLimitMinInput->setRange(-10000.0, 10000.0);
    m_hardLimitMinInput->setValue(-200.0);
    m_hardLimitMinInput->setDecimals(1);
    layout->addWidget(m_hardLimitMinInput, 1, 1);

    // 硬限制最大值
    layout->addWidget(new QLabel("硬限制最大值:"), 2, 0);
    m_hardLimitMaxInput = new QDoubleSpinBox(m_hardLimitsGroup);
    m_hardLimitMaxInput->setRange(-10000.0, 10000.0);
    m_hardLimitMaxInput->setValue(100.0);
    m_hardLimitMaxInput->setDecimals(1);
    layout->addWidget(m_hardLimitMaxInput, 2, 1);

    // 应用按钮
    m_applyHardLimitsButton = new QPushButton("应用硬限制", m_hardLimitsGroup);
    layout->addWidget(m_applyHardLimitsButton, 3, 0, 1, 2);

    // 硬限制状态显示
    m_hardLimitsStatusLabel = new QLabel("状态: 已启用 [-200.0, 100.0]", m_hardLimitsGroup);
    m_hardLimitsStatusLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; }");
    layout->addWidget(m_hardLimitsStatusLabel, 4, 0, 1, 2);

    // 连接信号
    connect(m_enableHardLimitsCheckbox, &QCheckBox::toggled, this, &MainWindow::onHardLimitsToggled);
    connect(m_applyHardLimitsButton, &QPushButton::clicked, this, &MainWindow::updateHardLimits);
}

void MainWindow::setupTestControls() {
    m_testGroup = new QGroupBox("测试功能", m_combinedTab);
    QVBoxLayout *layout = new QVBoxLayout(m_testGroup);

    // 重置默认值按钮
    m_resetDefaultsButton = new QPushButton("重置为默认配置", m_testGroup);
    layout->addWidget(m_resetDefaultsButton);

    // 测试异常数据按钮
    m_testOutlierDataButton = new QPushButton("测试异常数据处理", m_testGroup);
    layout->addWidget(m_testOutlierDataButton);

    // 测试范围恢复按钮
    m_testRangeRecoveryButton = new QPushButton("测试范围恢复功能", m_testGroup);
    layout->addWidget(m_testRangeRecoveryButton);

    // 测试状态显示
    m_testStatusLabel = new QLabel("状态: 就绪", m_testGroup);
    m_testStatusLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; }");
    layout->addWidget(m_testStatusLabel);

    // 连接信号
    connect(m_resetDefaultsButton, &QPushButton::clicked, this, &MainWindow::resetToDefaults);
    connect(m_testOutlierDataButton, &QPushButton::clicked, this, [this]() {
        m_isTestingOutliers = !m_isTestingOutliers;
        m_testOutlierDataButton->setText(m_isTestingOutliers ? "停止异常数据测试" : "测试异常数据处理");
        updateStatusLabels();
    });
    connect(m_testRangeRecoveryButton, &QPushButton::clicked, this, [this]() {
        m_isTestingRecovery = !m_isTestingRecovery;
        m_testRangeRecoveryButton->setText(m_isTestingRecovery ? "停止恢复测试" : "测试范围恢复功能");
        updateStatusLabels();
    });
}


void MainWindow::onResetRangeButtonClicked() {
    // 重置为默认范围 0-5
    m_currentMinRange = 0.0f;
    m_currentMaxRange = 5.0f;

    m_minRangeInput->setValue(m_currentMinRange);
    m_maxRangeInput->setValue(m_currentMaxRange);

    m_minRangeSlider->setValue(static_cast<int>(m_currentMinRange));
    m_maxRangeSlider->setValue(static_cast<int>(m_currentMaxRange));

    qDebug() << "重置数据范围: " << m_currentMinRange << " - " << m_currentMaxRange;
}

void MainWindow::onAutoChangeToggled(bool checked) {
    m_isAutoChanging = checked;

    // 禁用或启用手动控制
    m_minRangeSlider->setEnabled(!checked);
    m_maxRangeSlider->setEnabled(!checked);
    m_minRangeInput->setEnabled(!checked);
    m_maxRangeInput->setEnabled(!checked);
    m_resetRangeButton->setEnabled(!checked);

    qDebug() << "自动变化模式: " << (checked ? "开启" : "关闭");
}

void MainWindow::onModeChanged(int index) {
    QComboBox *comboBox = qobject_cast<QComboBox *>(sender());
    if (comboBox) {
        m_dataGenerationMode = static_cast<DataGenerationMode>(comboBox->itemData(index).toInt());

        // 根据模式调整UI控件状态
        bool isManualMode = (m_dataGenerationMode == DataGenerationMode::MANUAL_RANGE);
        m_dataRangeGroup->setEnabled(isManualMode);

        qDebug() << "切换数据生成模式: " << index;
    }
}

void MainWindow::updateDynamicRangeConfig() {
    // 获取当前配置
    ProGraphics::DynamicRange::DynamicRangeConfig prpsConfig;
    ProGraphics::DynamicRange::DynamicRangeConfig prpdConfig;

    // 更新核心参数
    prpsConfig.bufferRatio = static_cast<float>(m_bufferRatioInput->value());
    prpsConfig.responseSpeed = static_cast<float>(m_responseSpeedInput->value());
    prpsConfig.smartAdjustment = m_smartAdjustmentCheckbox->isChecked();
    prpsConfig.targetTickCount = m_targetTickCountInput->value();


    // 同样设置PRPD的配置
    prpdConfig = prpsConfig;

    // 应用配置

    m_prpsChart->setDynamicRangeConfig(prpsConfig);
    m_prpdChart->setDynamicRangeConfig(prpdConfig);

    qDebug() << "更新动态量程配置: bufferRatio=" << prpsConfig.bufferRatio
            << ", responseSpeed=" << prpsConfig.responseSpeed;
}


void MainWindow::updateInitialRange() {
    float minVal = static_cast<float>(m_initialMinInput->value());
    float maxVal = static_cast<float>(m_initialMaxInput->value());

    if (minVal >= maxVal) {
        qWarning() << "初始最小值必须小于最大值";
        return;
    }

    // 应用到两个图表
    m_prpdChart->setInitialRange(minVal, maxVal);
    m_prpsChart->setInitialRange(minVal, maxVal);

    updateStatusLabels();

    qDebug() << "更新初始范围:" << minVal << "到" << maxVal;
}

void MainWindow::updateHardLimits() {
    float minVal = static_cast<float>(m_hardLimitMinInput->value());
    float maxVal = static_cast<float>(m_hardLimitMaxInput->value());
    bool enabled = m_enableHardLimitsCheckbox->isChecked();

    if (minVal >= maxVal) {
        qWarning() << "硬限制最小值必须小于最大值";
        return;
    }

    // 应用到两个图表
    m_prpdChart->setHardLimits(minVal, maxVal, enabled);
    m_prpsChart->setHardLimits(minVal, maxVal, enabled);

    updateStatusLabels();

    qDebug() << "更新硬限制:" << (enabled ? "启用" : "禁用") << "[" << minVal << "," << maxVal << "]";
}

void MainWindow::onHardLimitsToggled(bool enabled) {
    // 启用/禁用硬限制输入控件
    m_hardLimitMinInput->setEnabled(enabled);
    m_hardLimitMaxInput->setEnabled(enabled);
    m_applyHardLimitsButton->setEnabled(enabled);

    // 立即应用状态变化
    m_prpdChart->enableHardLimits(enabled);
    m_prpsChart->enableHardLimits(enabled);

    updateStatusLabels();

    qDebug() << "硬限制" << (enabled ? "启用" : "禁用");
}

void MainWindow::resetToDefaults() {
    // 重置初始范围
    m_initialMinInput->setValue(-75.0);
    m_initialMaxInput->setValue(-30.0);
    updateInitialRange();

    // 重置硬限制
    m_enableHardLimitsCheckbox->setChecked(true);
    m_hardLimitMinInput->setValue(-200.0);
    m_hardLimitMaxInput->setValue(100.0);
    updateHardLimits();

    // 重置动态量程配置
    m_bufferRatioInput->setValue(0.2);
    m_responseSpeedInput->setValue(0.8);
    m_smartAdjustmentCheckbox->setChecked(true);
    m_targetTickCountInput->setValue(6);
    updateDynamicRangeConfig();

    // 停止测试
    m_isTestingOutliers = false;
    m_isTestingRecovery = false;
    m_testOutlierDataButton->setText("测试异常数据处理");
    m_testRangeRecoveryButton->setText("测试范围恢复功能");

    updateStatusLabels();
    qDebug() << "重置为默认配置";
}

void MainWindow::updateStatusLabels() {
    // 更新初始范围标签
    auto [prpdInitialMin, prpdInitialMax] = m_prpdChart->getInitialRange();
    m_currentInitialRangeLabel->setText(
        QString("当前: [%1, %2]").arg(prpdInitialMin, 0, 'f', 1).arg(prpdInitialMax, 0, 'f', 1));

    // 更新硬限制状态标签
    if (m_prpdChart->isHardLimitsEnabled()) {
        auto [hardMin, hardMax] = m_prpdChart->getHardLimits();
        m_hardLimitsStatusLabel->setText(
            QString("状态: 已启用 [%1, %2]").arg(hardMin, 0, 'f', 1).arg(hardMax, 0, 'f', 1));
    } else {
        m_hardLimitsStatusLabel->setText("状态: 已禁用");
    }

    // 更新测试状态标签
    QString testStatus = "状态: 就绪";
    if (m_isTestingOutliers) {
        testStatus = "状态: 正在测试异常数据处理";
    } else if (m_isTestingRecovery) {
        testStatus = "状态: 正在测试范围恢复";
    }
    m_testStatusLabel->setText(testStatus);
}


void MainWindow::updateSliderLimits(float newMin, float newMax) {
    // 当范围变化很大时，自动调整滑块的范围
    if (newMin < m_minRangeSlider->minimum() || newMax > m_maxRangeSlider->maximum()) {
        int newSliderMin = std::min(static_cast<int>(newMin) - 100, m_minRangeSlider->minimum());
        int newSliderMax = std::max(static_cast<int>(newMax) + 100, m_maxRangeSlider->maximum());

        m_minRangeSlider->setRange(newSliderMin, newSliderMax);
        m_maxRangeSlider->setRange(newSliderMin, newSliderMax);

        qDebug() << "调整滑块范围: " << newSliderMin << " - " << newSliderMax;
    }
}

void MainWindow::generateTestData() {
    // 生成测试数据
    std::vector<float> cycleData;

    // 使用选定的数据生成方法
    if (m_isTestingOutliers) {
        cycleData = generateOutlierTestData();
    } else if (m_isTestingRecovery) {
        cycleData = generateRecoveryTestData();
    } else {
        // 使用选定的数据生成方法
        switch (m_dataGenerationMode) {
            case DataGenerationMode::MANUAL_RANGE:
                cycleData = generateManualRangePattern();
                break;
            case DataGenerationMode::RANDOM_AMPLITUDE:
                cycleData = generateRandomAmplitudePattern();
                break;
            case DataGenerationMode::SMOOTH_CHANGING:
                cycleData = generateSmoothlyChangingPattern();
                break;
            case DataGenerationMode::STANDARD_PD:
                cycleData = generateStandardPDPattern();
                break;
        }
    }

    // 添加到图表
    m_prpsChart->addCycleData(cycleData);
    m_prpdChart->addCycleData(cycleData);

    // 如果是自动变化模式，更新输入值显示
    if (m_isAutoChanging && m_dataGenerationMode == DataGenerationMode::MANUAL_RANGE) {
        static int direction = 1;
        static float step = 0.5f;

        // 更新范围
        m_currentMaxRange += step * direction;

        // 当达到某个边界时改变方向
        if (m_currentMaxRange > 500.0f || m_currentMaxRange < 50.0f) {
            direction *= -1;
        }

        // 更新UI
        m_maxRangeInput->setValue(static_cast<double>(m_currentMaxRange));
        m_maxRangeSlider->setValue(static_cast<int>(m_currentMaxRange));
    }
}

std::vector<float> MainWindow::generateManualRangePattern() const {
    std::vector<float> cycleData(ProGraphics::PRPSConstants::PHASE_POINTS, 0.0f);

    // 创建随机数生成器
    static std::random_device rd;
    static std::mt19937 gen(rd());

    // 使用当前设定的范围生成随机数据
    std::uniform_real_distribution<float> amplitudeDist(m_currentMinRange, m_currentMaxRange);

    // 为每个相位点生成随机幅值
    for (int i = 0; i < ProGraphics::PRPSConstants::PHASE_POINTS; ++i) {
        cycleData[i] = amplitudeDist(gen);
    }

    return cycleData;
}

std::vector<float> MainWindow::generateStandardPDPattern() const {
    std::vector<float> cycleData(ProGraphics::PRPSConstants::PHASE_POINTS, -80.0f); // 初始化为背景噪声水平

    // 创建随机数生成器
    static std::random_device rd;
    static std::mt19937 gen(rd());

    // 背景噪声（较低水平）
    static std::normal_distribution<float> backgroundNoise(0.0f, 0.5f);

    // 放电幅值分布
    static std::weibull_distribution<float> dischargeAmplitudePos(2.0f, 15.0f); // 正半周期
    static std::weibull_distribution<float> dischargeAmplitudeNeg(2.0f, 20.0f); // 负半周期

    // 添加背景噪声
    for (int i = 0; i < ProGraphics::PRPSConstants::PHASE_POINTS; ++i) {
        cycleData[i] = -75.0f + backgroundNoise(gen);
    }

    // 计算相位对应的索引
    auto phaseToIndex = [](float phase) -> int {
        return static_cast<int>((phase / 360.0f) * ProGraphics::PRPSConstants::PHASE_POINTS) %
               ProGraphics::PRPSConstants::PHASE_POINTS;
    };

    // 添加内部放电 - 正半周期（集中在50°-130°）
    static std::normal_distribution<float> posPhaseDistribution(90.0f, 15.0f);
    int posDischargeCount = 20 + gen() % 10; // 20-30个放电点

    for (int i = 0; i < posDischargeCount; ++i) {
        float phase = posPhaseDistribution(gen);
        if (phase >= 0 && phase <= 180) {
            int idx = phaseToIndex(phase);
            float amplitude = -75.0f + dischargeAmplitudePos(gen);
            amplitude = std::min(amplitude, -40.0f); // 限制最大值
            cycleData[idx] = std::max(cycleData[idx], amplitude); // 取较大值
        }
    }

    // 添加内部放电 - 负半周期（集中在230°-310°）
    static std::normal_distribution<float> negPhaseDistribution(270.0f, 15.0f);
    int negDischargeCount = 15 + gen() % 10; // 15-25个放电点

    for (int i = 0; i < negDischargeCount; ++i) {
        float phase = negPhaseDistribution(gen);
        if (phase >= 180 && phase <= 360) {
            int idx = phaseToIndex(phase);
            float amplitude = -75.0f + dischargeAmplitudeNeg(gen);
            amplitude = std::min(amplitude, -35.0f); // 负半周放电略强
            cycleData[idx] = std::max(cycleData[idx], amplitude); // 取较大值
        }
    }

    return cycleData;
}

std::vector<float> MainWindow::generateRandomAmplitudePattern() const {
    std::vector<float> cycleData(ProGraphics::PRPSConstants::PHASE_POINTS, 0.0f);

    // 创建随机数生成器
    static std::random_device rd;
    static std::mt19937 gen(rd());

    // 定义几种不同的数据范围
    static const struct Range {
        float min;
        float max;
    } ranges[] = {
                {-70.0f, 0.0f},
                {-2000.0f, 1000.0f}, // 范围1: 负值到正值
                {-2200.0f, 1200.0f}, // 范围2: 全正值较大范围
                {-1800, -1200}, // 范围4: 全正值较小范围
                {0.0, 2000}, // 范围5: 全正值较大范围
                {2000.0f, 3000.0f}, // 范围3: 全正值较小范围
                // {0.0f, 70.0f},
                {0.0f, 0.15f},
                {1.0f, 1.2f},
                // {0.0, 2000},
                {500, 3000}
            };

    // 跟踪时间和当前范围
    static QElapsedTimer rangeTimer;
    static int currentRangeIndex = 0;
    static bool initialized = false;

    if (!initialized) {
        rangeTimer.start();
        initialized = true;
    }

    const int rangeDurationMs = 5000;
    int elapsedSecs = rangeTimer.elapsed() / rangeDurationMs;
    int newRangeIndex = elapsedSecs % (sizeof(ranges) / sizeof(ranges[0]));

    // 如果范围改变，输出日志
    if (newRangeIndex != currentRangeIndex) {
        currentRangeIndex = newRangeIndex;
        // qDebug() << "切换数据范围: " << ranges[currentRangeIndex].min << " 到 " << ranges[currentRangeIndex].max
        //         << QTime::currentTime().toString();
    }

    // 使用当前范围生成随机数据
    std::uniform_real_distribution<float> amplitudeDist(ranges[currentRangeIndex].min, ranges[currentRangeIndex].max);

    // 为每个相位点生成随机幅值
    for (int i = 0; i < ProGraphics::PRPSConstants::PHASE_POINTS; ++i) {
        cycleData[i] = amplitudeDist(gen);
    }

    return cycleData;
}

std::vector<float> MainWindow::generateSmoothlyChangingPattern() const {
    std::vector<float> cycleData(ProGraphics::PRPSConstants::PHASE_POINTS, 0.0f);

    // 创建随机数生成器
    static std::random_device rd;
    static std::mt19937 gen(rd());

    // 定义平滑变化的状态和参数
    static enum RangeState {
        SHRINKING, // 范围正在缩小
        EXPANDING, // 范围正在扩大
        STABILIZING // 范围稳定期
    } currentState = EXPANDING;

    // 当前基准范围值
    static float currentBaseMax = 10.0f; // 初始值设为中等大小
    static float currentBaseMin = 0.0f;

    // 范围边界值
    static const float ABSOLUTE_MIN_VALUE = 0.0f; // 最小值下限
    static const float MIN_RANGE_VALUE = 10.0f; // 最小范围值
    static const float MAX_RANGE_VALUE = 500.0f; // 最大范围值

    // 速度和平滑控制参数
    static const float RANGE_CHANGE_SPEED = 1.0f; // 每次更新的基础变化量(大幅降低)
    static const float MIN_VALUE_CHANGE_RATIO = 0.2f; // 最小值变化速度比例

    // 稳定期控制
    static const int STABILIZE_DURATION = 400; // 稳定期持续时间(帧数)
    static int stabilizeCounter = 0;

    // 平滑过渡控制
    static float targetBaseMax = currentBaseMax; // 目标最大值
    static float targetBaseMin = currentBaseMin; // 目标最小值
    static const float SMOOTHING_FACTOR = 0.005f; // 平滑过渡因子(越小越平滑)

    // 状态变化控制
    static int cycleCounter = 0;
    cycleCounter++;

    // 状态机逻辑
    if (currentState == STABILIZING) {
        // 稳定期 - 保持当前范围一段时间
        stabilizeCounter++;
        if (stabilizeCounter >= STABILIZE_DURATION) {
            stabilizeCounter = 0;
            // 从稳定期结束后决定下一个状态
            if (currentBaseMax >= MAX_RANGE_VALUE * 0.95f) {
                currentState = SHRINKING;
                // qDebug() << "数据范围变化: 开始缩小范围";
            } else if (currentBaseMax <= MIN_RANGE_VALUE * 1.5f) {
                currentState = EXPANDING;
                // qDebug() << "数据范围变化: 开始扩大范围";
            } else {
                // 在中间范围随机决定下一步是扩大还是缩小
                currentState = (gen() % 2 == 0) ? SHRINKING : EXPANDING;
                // qDebug() << "数据范围变化: 随机" << (currentState == SHRINKING ? "缩小" : "扩大") << "范围";
            }
        }
    } else if (currentState == SHRINKING) {
        // 范围缩小阶段 - 设置目标值
        targetBaseMax = MIN_RANGE_VALUE;
        targetBaseMin = ABSOLUTE_MIN_VALUE;

        // 检查是否达到目标(接近最小范围)
        if (std::abs(currentBaseMax - MIN_RANGE_VALUE) < 5.0f) {
            currentState = STABILIZING;
            // qDebug() << "数据范围变化: 到达最小范围，开始稳定期";
        }
    } else if (currentState == EXPANDING) {
        // 范围扩大阶段 - 设置目标值
        targetBaseMax = MAX_RANGE_VALUE;
        targetBaseMin = ABSOLUTE_MIN_VALUE;

        // 检查是否达到目标(接近最大范围)
        if (std::abs(currentBaseMax - MAX_RANGE_VALUE) < 50.0f) {
            currentState = STABILIZING;
            // qDebug() << "数据范围变化: 到达最大范围，开始稳定期";
        }
    }

    // 使用平滑插值逐渐接近目标值(类似于弹簧效果)
    currentBaseMax += (targetBaseMax - currentBaseMax) * SMOOTHING_FACTOR;
    currentBaseMin += (targetBaseMin - currentBaseMin) * SMOOTHING_FACTOR * MIN_VALUE_CHANGE_RATIO;

    // 确保范围值在合理边界内
    currentBaseMax = std::max(MIN_RANGE_VALUE, std::min(MAX_RANGE_VALUE, currentBaseMax));
    currentBaseMin = std::max(ABSOLUTE_MIN_VALUE, std::min(currentBaseMax * 0.5f, currentBaseMin));

    // 计算当前范围大小，用于调试输出
    float currentRange = currentBaseMax - currentBaseMin;

    // 创建分布，加入轻微随机抖动使测试更真实
    float jitterFactor = 0.05f; // 5%的随机抖动
    float minWithJitter = currentBaseMin * (1.0f - jitterFactor + jitterFactor * 2.0f * static_cast<float>(gen()) /
                                            static_cast<float>(gen.max()));
    float maxWithJitter = currentBaseMax * (1.0f - jitterFactor + jitterFactor * 2.0f * static_cast<float>(gen()) /
                                            static_cast<float>(gen.max()));

    std::uniform_real_distribution<float> amplitudeDist(minWithJitter, maxWithJitter);

    // 为每个相位点生成平滑随机数据
    for (int i = 0; i < ProGraphics::PRPSConstants::PHASE_POINTS; ++i) {
        // 使用正弦函数创造更平滑的数据分布
        float phaseFactor = 0.5f + 0.5f * std::sin((i + cycleCounter * 0.1f) * 0.05f);
        float amplitude = minWithJitter + phaseFactor * (maxWithJitter - minWithJitter);

        // 添加随机性，但保持平滑
        float randomness = 0.2f; // 20%的随机性
        cycleData[i] = amplitude * (1.0f - randomness) + amplitudeDist(gen) * randomness;
    }

    // 每200次更新输出一次当前范围，帮助观察
    // if (cycleCounter % 200 == 0) {
    //     qDebug() << "当前数据范围: " << currentBaseMin << " 到 " << currentBaseMax
    //             << " (范围大小: " << currentRange << ")"
    //             << " 状态: " << (currentState == SHRINKING ? "缩小" : (currentState == EXPANDING ? "扩大" : "稳定"))
    //             << " 稳定计数: " << stabilizeCounter;
    // }

    return cycleData;
}


std::vector<float> MainWindow::generateOutlierTestData() const {
    std::vector<float> cycleData(ProGraphics::PRPSConstants::PHASE_POINTS);

    static std::random_device rd;
    static std::mt19937 gen(rd());
    static int counter = 0;
    counter++;

    // 大部分时间生成正常数据
    std::uniform_real_distribution<float> normalDist(-60.0f, -40.0f);

    for (int i = 0; i < ProGraphics::PRPSConstants::PHASE_POINTS; ++i) {
        cycleData[i] = normalDist(gen);
    }

    // 每50帧插入一些异常数据
    if (counter % 50 == 0) {
        std::uniform_real_distribution<float> outlierDist(-5000.0f, 5000.0f);
        int outlierCount = 5 + gen() % 10; // 5-15个异常点

        for (int i = 0; i < outlierCount; ++i) {
            int idx = gen() % ProGraphics::PRPSConstants::PHASE_POINTS;
            cycleData[idx] = outlierDist(gen);
        }

        qDebug() << "插入" << outlierCount << "个异常数据点";
    }

    return cycleData;
}

std::vector<float> MainWindow::generateRecoveryTestData() const {
    std::vector<float> cycleData(ProGraphics::PRPSConstants::PHASE_POINTS);

    static std::random_device rd;
    static std::mt19937 gen(rd());
    static int phase = 0; // 0: 正常, 1: 超出, 2: 恢复
    static int phaseCounter = 0;

    phaseCounter++;

    // 每200帧切换一个阶段
    if (phaseCounter % 200 == 0) {
        phase = (phase + 1) % 3;
        switch (phase) {
            case 0:
                qDebug() << "范围恢复测试: 正常数据阶段";
                break;
            case 1:
                qDebug() << "范围恢复测试: 超出范围阶段";
                break;
            case 2:
                qDebug() << "范围恢复测试: 恢复到初始范围阶段";
                break;
        }
    }
    std::uniform_real_distribution<float> dist;

    switch (phase) {
        case 0: // 正常数据（在初始范围内）
            dist = std::uniform_real_distribution<float>(-70.0f, -35.0f);
            break;
        case 1: // 超出范围数据
            dist = std::uniform_real_distribution<float>(-150.0f, 50.0f);
            break;
        case 2: // 恢复到初始范围
            dist = std::uniform_real_distribution<float>(-65.0f, -40.0f);
            break;
    }

    for (int i = 0; i < ProGraphics::PRPSConstants::PHASE_POINTS; ++i) {
        cycleData[i] = dist(gen);
    }

    return cycleData;
}

