#pragma once

#include <QObject>
#include <QTimer>
#include <vector>

class DataProcessor : public QObject {
    Q_OBJECT

public:
    explicit DataProcessor(QObject* parent = nullptr);
    ~DataProcessor() override {
        if (m_timer) {
            m_timer->stop();
            delete m_timer;
        }
    }

public slots:
    void startProcessing();
    void markDataProcessed();

signals:
    void dataReady(const std::vector<float>& data);

private:
    QTimer* m_timer = nullptr;
    bool m_dataProcessed = true;
    std::vector<float> generateRandomAmplitudePattern() const;
    std::vector<float> generateStandardPDPattern() const;
};