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

#include <cmath>
#include <random>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    m_tabWidget = new QTabWidget(this);
    setCentralWidget(m_tabWidget);

    // 添加第一个示例
    m_prpsChart = new ProGraphics::PRPSChart(this);
    m_tabWidget->addTab(m_prpsChart, "PRPS");
    m_prpdChart = new ProGraphics::PRPDChart(this);
    m_tabWidget->addTab(m_prpdChart, "PRPD");

    m_tabWidget->addTab(new TestCoordinate3d(this), "测试3d坐标系统");
    m_tabWidget->addTab(new TestCoordinate2d(this), "测试2d坐标系统");

    // 添加数据生成模式控制
    QWidget *controlPanel = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(controlPanel);

    QComboBox *modeSelector = new QComboBox(controlPanel);
    modeSelector->addItem("随机幅值模式", static_cast<int>(DataGenerationMode::RANDOM_AMPLITUDE));
    modeSelector->addItem("平滑变化模式", static_cast<int>(DataGenerationMode::SMOOTH_CHANGING));
    modeSelector->addItem("标准放电模式", static_cast<int>(DataGenerationMode::STANDARD_PD));

    connect(modeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        QComboBox *comboBox = qobject_cast<QComboBox *>(sender());
        if (comboBox) {
            m_dataGenerationMode = static_cast<DataGenerationMode>(comboBox->itemData(index).toInt());
            qDebug() << "切换数据生成模式: " << index;
        }
    });

    layout->addWidget(new QLabel("数据生成模式:"));
    layout->addWidget(modeSelector);
    layout->addStretch();


    m_tabWidget->addTab(controlPanel, "控制面板");

    // 默认使用平滑变化模式
    m_dataGenerationMode = DataGenerationMode::SMOOTH_CHANGING;

    // 设置窗口大小
    resize(800, 600);

    // 设置数据生成定时器
    connect(&m_dataTimer, &QTimer::timeout, this, &MainWindow::generateTestData);
    m_dataTimer.setInterval(20);
    m_dataTimer.start();
}

void MainWindow::generateTestData() {
    // 生成测试数据
    std::vector<float> cycleData;

    // 使用选定的数据生成方法
    switch (m_dataGenerationMode) {
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
