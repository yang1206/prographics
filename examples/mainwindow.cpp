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

    // 将控制组添加到控制面板
    controlLayout->addWidget(modeGroup);
    controlLayout->addWidget(m_dataRangeGroup);
    controlLayout->addWidget(m_configGroup);
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
    m_maxRangeSlider->setValue(5);
    m_maxRangeSlider->setTickPosition(QSlider::TicksBelow);

    m_maxRangeInput = new QDoubleSpinBox(m_dataRangeGroup);
    m_maxRangeInput->setRange(-100.0, 1000.0);
    m_maxRangeInput->setValue(5.0);
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
            m_maxRangeInput->setValue(m_minRangeInput->value() + 1.0);
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
            m_minRangeInput->setValue(m_maxRangeInput->value() - 1.0);
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

    // 添加最小量程控制组
    QGroupBox *minRangeGroup = new QGroupBox("最小量程控制", m_configGroup);
    QGridLayout *minRangeLayout = new QGridLayout(minRangeGroup);

    // 启用最小量程
    m_enableMinRangeCheckbox = new QCheckBox("启用最小量程", minRangeGroup);
    m_enableMinRangeCheckbox->setChecked(false);
    minRangeLayout->addWidget(m_enableMinRangeCheckbox, 0, 0, 1, 2);

    // 最小量程上限
    minRangeLayout->addWidget(new QLabel("最小量程上限:"), 1, 0);
    m_minRangeMaxInput = new QDoubleSpinBox(minRangeGroup);
    m_minRangeMaxInput->setRange(0.1, 100.0);
    m_minRangeMaxInput->setValue(5.0);
    m_minRangeMaxInput->setSingleStep(0.5);
    minRangeLayout->addWidget(m_minRangeMaxInput, 1, 1);

    // 启用阈值
    minRangeLayout->addWidget(new QLabel("启用阈值:"), 2, 0);
    m_activationThresholdInput = new QDoubleSpinBox(minRangeGroup);
    m_activationThresholdInput->setRange(0.1, 50.0);
    m_activationThresholdInput->setValue(2.0);
    m_activationThresholdInput->setSingleStep(0.1);
    minRangeLayout->addWidget(m_activationThresholdInput, 2, 1);

    // 使用固定量程
    m_useFixedRangeCheckbox = new QCheckBox("使用固定量程", minRangeGroup);
    m_useFixedRangeCheckbox->setChecked(false);
    minRangeLayout->addWidget(m_useFixedRangeCheckbox, 3, 0, 1, 2);

    configLayout->addWidget(minRangeGroup, 4, 0, 1, 2);

    // 连接信号
    connect(m_enableMinRangeCheckbox, &QCheckBox::toggled, this, [=](bool checked) {
        m_minRangeMaxInput->setEnabled(checked);
        m_activationThresholdInput->setEnabled(checked);
    });

    // 应用按钮
    m_applyConfigButton = new QPushButton("应用配置", m_configGroup);
    configLayout->addWidget(m_applyConfigButton, 5, 0, 1, 2);

    // 连接信号
    connect(m_applyConfigButton, &QPushButton::clicked, this, &MainWindow::updateDynamicRangeConfig);
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

    // 更新最小量程配置
    prpsConfig.enforceMinimumRange = m_enableMinRangeCheckbox->isChecked();
    prpsConfig.minimumRangeMax = static_cast<float>(m_minRangeMaxInput->value());
    prpsConfig.minimumThreshold = static_cast<float>(m_activationThresholdInput->value());
    prpsConfig.useFixedRange = m_useFixedRangeCheckbox->isChecked();

    // 同样设置PRPD的配置
    prpdConfig = prpsConfig;

    // 应用配置

    m_prpsChart->setDynamicRangeConfig(prpsConfig);
    m_prpdChart->setDynamicRangeConfig(prpdConfig);

    qDebug() << "更新动态量程配置: bufferRatio=" << prpsConfig.bufferRatio
            << ", responseSpeed=" << prpsConfig.responseSpeed
            << ", minimumRangeMax=" << prpsConfig.minimumRangeMax;
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
