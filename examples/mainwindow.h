#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QStackedWidget>
#include <QDialog>
#include "prographics/charts/prpd/prpd.h"
#include "prographics/charts/prps/prps.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onGenerateData();
    void onGenerateBatch(int count);
    void onRangeModeChanged(int index);
    void onApplyRange();
    void onResetAll();
    void updateStatus();
    void onTestDialog();

private:
    void setupUI();
    void updateUIForMode(int modeIndex);
    std::vector<float> generateRandomData();

    ProGraphics::PRPSChart *m_prpsChart;
    ProGraphics::PRPDChart *m_prpdChart;

    // 数据生成
    QDoubleSpinBox *m_dataMinSpin;
    QDoubleSpinBox *m_dataMaxSpin;
    QPushButton *m_generateButton;

    // 量程模式选择
    QComboBox *m_rangeModeCombo;
    QLabel *m_modeDescLabel;

    // 范围设置（所有模式共用）
    QDoubleSpinBox *m_rangeMinSpin;
    QDoubleSpinBox *m_rangeMaxSpin;

    // 动态量程配置（Auto/Adaptive 模式）
    QWidget *m_dynamicConfigWidget;
    QDoubleSpinBox *m_bufferRatioSpin;
    QDoubleSpinBox *m_responseSpeedSpin;
    QSpinBox *m_recoveryFramesSpin;
    QDoubleSpinBox *m_recoveryRatioSpin;
    QCheckBox *m_smartAdjustCheck;
    QCheckBox *m_enableRecoveryCheck;

    // 硬限制
    QCheckBox *m_enableHardLimitsCheck;
    QDoubleSpinBox *m_hardLimitMinSpin;
    QDoubleSpinBox *m_hardLimitMaxSpin;

    // 状态显示
    QLabel *m_statusLabel;

    QTimer *m_statusTimer;
    QTimer *m_batchTimer;
    int m_batchRemaining;
};

#endif
