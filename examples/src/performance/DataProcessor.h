#pragma once

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <atomic>
#include <vector>

// 数据生成线程
class DataGeneratorThread : public QThread {
    Q_OBJECT

public:
    explicit DataGeneratorThread(QObject *parent = nullptr);

    ~DataGeneratorThread() override;

    void stop();

    void setPaused(bool paused);

    void setUpdateInterval(int intervalMs);

    void markDataProcessed();

signals:
    void dataReady(const std::vector<float> &data);

protected:
    void run() override;

private:
    QMutex m_mutex;
    QWaitCondition m_condition;
    std::atomic<bool> m_abort{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_dataProcessed{true};
    int m_updateInterval{20}; // 默认约30fps

    // 数据生成
    std::vector<float> generateRandomAmplitudePattern() const;

    std::vector<float> generateStandardPDPattern() const;
};

class DataProcessor : public QObject {
    Q_OBJECT

public:
    explicit DataProcessor(QObject *parent = nullptr);

    ~DataProcessor() override;

public slots:
    void startProcessing();

    void stopProcessing();

    void markDataProcessed();

    void setUpdateInterval(int intervalMs);

signals:
    void dataReady(const std::vector<float> &data);

private:
    DataGeneratorThread *m_generatorThread = nullptr;
};
