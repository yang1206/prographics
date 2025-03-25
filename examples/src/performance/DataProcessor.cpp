#include "DataProcessor.h"
#include <QTime>
#include <random>
#include <QThread>
#include "prographics/charts/prps/prps.h"

DataProcessor::DataProcessor(QObject *parent) : QObject(parent) {
}

void DataProcessor::startProcessing() {
    // 在工作线程中创建定时器
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        // 检查上一次数据是否已经被处理
        if (!m_dataProcessed) {
            // 如果上一次数据还未被处理，则跳过本次生成
            return;
        }

        // 标记数据未处理
        m_dataProcessed = false;

        // 生成数据
        auto data = generateStandardPDPattern();

        // 发送数据
        emit dataReady(data);
    });

    // 初始化为已处理状态，以便第一次能生成数据
    m_dataProcessed = true;

    m_timer->setInterval(20); // 约30fps，比20ms更合理
    m_timer->start();
}

// 添加新方法，用于标记数据已处理
void DataProcessor::markDataProcessed() {
    m_dataProcessed = true;
}

// 从PerformanceTest复制这两个方法
std::vector<float> DataProcessor::generateRandomAmplitudePattern() const {
    std::vector<float> cycleData(ProGraphics::PRPSConstants::PHASE_POINTS, 0.0f);

    // 创建随机数生成器
    static std::random_device rd;
    static std::mt19937 gen(rd());

    // 定义几种不同的数据范围
    static const struct Range {
        float min;
        float max;
    } ranges[] = {
                // {-2000.0f, 1000.0f}, // 范围1: 负值到正值
                {-2200.0f, 1200.0f}, // 范围2: 全正值较大范围
                {2000.0f, 3000.0f} // 范围3: 全正值较小范围
            };

    // 跟踪时间和当前范围
    static QElapsedTimer rangeTimer;
    static int currentRangeIndex = 0;
    static bool initialized = false;

    if (!initialized) {
        rangeTimer.start();
        initialized = true;
    }

    // 每20秒切换一次范围
    const int rangeDurationMs = 10000; // 20秒
    int elapsedSecs = rangeTimer.elapsed() / rangeDurationMs;
    int newRangeIndex = elapsedSecs % (sizeof(ranges) / sizeof(ranges[0]));

    // 如果范围改变，输出日志
    if (newRangeIndex != currentRangeIndex) {
        currentRangeIndex = newRangeIndex;
        qDebug() << "切换数据范围: " << ranges[currentRangeIndex].min << " 到 " << ranges[currentRangeIndex].max
                << QTime::currentTime().toString();
    }

    // 使用当前范围生成随机数据
    std::uniform_real_distribution<float> amplitudeDist(ranges[currentRangeIndex].min, ranges[currentRangeIndex].max);

    // 为每个相位点生成随机幅值
    for (int i = 0; i < ProGraphics::PRPSConstants::PHASE_POINTS; ++i) {
        cycleData[i] = amplitudeDist(gen);
    }

    return cycleData;
}

std::vector<float> DataProcessor::generateStandardPDPattern() const {
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
