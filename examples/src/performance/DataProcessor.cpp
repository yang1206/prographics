#include "DataProcessor.h"
#include <QTime>
#include <random>
#include <QThread>
#include <QDebug>
#include "prographics/charts/prps/prps.h"

// DataGeneratorThread 实现
DataGeneratorThread::DataGeneratorThread(QObject *parent) : QThread(parent) {
}

DataGeneratorThread::~DataGeneratorThread() {
    stop();
    wait();
}

void DataGeneratorThread::stop() {
    QMutexLocker locker(&m_mutex);
    m_abort = true;
    m_condition.wakeAll();
}

void DataGeneratorThread::setPaused(bool paused) {
    QMutexLocker locker(&m_mutex);
    m_paused = paused;
    if (!paused) {
        m_condition.wakeAll();
    }
}

void DataGeneratorThread::setUpdateInterval(int intervalMs) {
    QMutexLocker locker(&m_mutex);
    m_updateInterval = qMax(1, intervalMs);
}

void DataGeneratorThread::markDataProcessed() {
    m_dataProcessed = true;
    m_condition.wakeAll();
}

void DataGeneratorThread::run() {
    qDebug() << "数据生成线程开始执行";
    
    // 设置线程优先级
    QThread::currentThread()->setPriority(QThread::HighPriority);
    
    while (!m_abort) {
        try {
            // 检查是否暂停
            if (m_paused) {
                QMutexLocker locker(&m_mutex);
                m_condition.wait(&m_mutex);
                continue;
            }
            
            // 检查上一次数据是否已处理
            if (!m_dataProcessed) {
                QMutexLocker locker(&m_mutex);
                m_condition.wait(&m_mutex, 100); // 最多等待100ms
                continue;
            }
            
            // 标记数据未处理
            m_dataProcessed = false;
            
            // 生成数据
            auto data = generateStandardPDPattern();
            
            // 发送数据
            emit dataReady(data);
            
            // 精确控制更新频率
            QThread::msleep(m_updateInterval);
        } catch (const std::exception &e) {
            qDebug() << "数据生成线程异常:" << e.what();
        } catch (...) {
            qDebug() << "数据生成线程未知异常";
        }
    }
    
    qDebug() << "数据生成线程结束执行";
}

// 从原DataProcessor复制的数据生成方法
std::vector<float> DataGeneratorThread::generateRandomAmplitudePattern() const {
    std::vector<float> cycleData(ProGraphics::PRPSConstants::PHASE_POINTS, 0.0f);
    
    // 创建随机数生成器
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    // 定义几种不同的数据范围
    static const struct Range {
        float min;
        float max;
    } ranges[] = {
        {0.0f, 1.5f}
    };
    
    // 跟踪时间和当前范围
    static QElapsedTimer rangeTimer;
    static int currentRangeIndex = 0;
    static bool initialized = false;
    
    if (!initialized) {
        rangeTimer.start();
        initialized = true;
    }
    
    // 每10秒切换一次范围
    const int rangeDurationMs = 10000;
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

std::vector<float> DataGeneratorThread::generateStandardPDPattern() const {
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

// DataProcessor 实现
DataProcessor::DataProcessor(QObject *parent) : QObject(parent) {
    m_generatorThread = new DataGeneratorThread(this);
    
    // 连接信号和槽
    connect(m_generatorThread, &DataGeneratorThread::dataReady, this, &DataProcessor::dataReady, Qt::QueuedConnection);
}

DataProcessor::~DataProcessor() {
    stopProcessing();
}

void DataProcessor::startProcessing() {
    if (!m_generatorThread->isRunning()) {
        qDebug() << "启动数据生成线程";
        m_generatorThread->start();
    } else {
        qDebug() << "数据生成线程已在运行";
        m_generatorThread->setPaused(false);
    }
}

void DataProcessor::stopProcessing() {
    if (m_generatorThread && m_generatorThread->isRunning()) {
        qDebug() << "停止数据生成线程";
        m_generatorThread->stop();
        m_generatorThread->wait();
    }
}

void DataProcessor::markDataProcessed() {
    if (m_generatorThread) {
        m_generatorThread->markDataProcessed();
    }
}

void DataProcessor::setUpdateInterval(int intervalMs) {
    if (m_generatorThread) {
        m_generatorThread->setUpdateInterval(intervalMs);
    }
}

