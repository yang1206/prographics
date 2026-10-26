#pragma once

#include <QDialog>
#include <memory>

class StressTestDialog final : public QDialog {
public:
    explicit StressTestDialog(QWidget* parent = nullptr);
    ~StressTestDialog() override;
    void reject() override;

    // 自动运行使用同一个测试流程，便于重复测试和脚本收集报告。
    void startAutomatically(int warmupSeconds, int durationSeconds, const QString& csvPath, int presetIndex = 0);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
