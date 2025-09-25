#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTabWidget>
#include <QSplitter>
#include "prographics/charts/prpd/prpd.h"
#include "prographics/charts/prps/prps.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void generateTestData();

    void onResetRangeButtonClicked();

    void onAutoChangeToggled(bool checked);

    void onModeChanged(int index);

    void updateDynamicRangeConfig();

    // ========== 新增：初始范围和硬限制相关槽函数 ==========
    void updateInitialRange();

    void updateHardLimits();

    void onHardLimitsToggled(bool enabled);

    void resetToDefaults();

private:
    // 数据生成模式
    enum class DataGenerationMode {
        MANUAL_RANGE,
        RANDOM_AMPLITUDE,
        SMOOTH_CHANGING,
        STANDARD_PD
    };

    // UI组件
    QTabWidget *m_tabWidget;
    QWidget *m_combinedTab;
    QGridLayout *m_combinedLayout;

    // 图表
    ProGraphics::PRPSChart *m_prpsChart;
    ProGraphics::PRPDChart *m_prpdChart;

    // 现有控制组
    QGroupBox *m_dataRangeGroup;
    QSlider *m_minRangeSlider;
    QSlider *m_maxRangeSlider;
    QDoubleSpinBox *m_minRangeInput;
    QDoubleSpinBox *m_maxRangeInput;
    QPushButton *m_applyRangeButton;
    QPushButton *m_resetRangeButton;
    QCheckBox *m_autoChangeCheckbox;

    QGroupBox *m_configGroup;
    QDoubleSpinBox *m_bufferRatioInput;
    QDoubleSpinBox *m_responseSpeedInput;
    QCheckBox *m_smartAdjustmentCheckbox;
    QSpinBox *m_targetTickCountInput;
    QPushButton *m_applyConfigButton;

    // ========== 新增：初始范围控制组 ==========
    QGroupBox *m_initialRangeGroup;
    QDoubleSpinBox *m_initialMinInput;
    QDoubleSpinBox *m_initialMaxInput;
    QPushButton *m_applyInitialRangeButton;
    QLabel *m_currentInitialRangeLabel;

    // ========== 新增：硬限制控制组 ==========
    QGroupBox *m_hardLimitsGroup;
    QCheckBox *m_enableHardLimitsCheckbox;
    QDoubleSpinBox *m_hardLimitMinInput;
    QDoubleSpinBox *m_hardLimitMaxInput;
    QPushButton *m_applyHardLimitsButton;
    QLabel *m_hardLimitsStatusLabel;

    // ========== 新增：测试控制组 ==========
    QGroupBox *m_testGroup;
    QPushButton *m_resetDefaultsButton;
    QPushButton *m_testOutlierDataButton;
    QPushButton *m_testRangeRecoveryButton;
    QLabel *m_testStatusLabel;

    // 数据生成
    QTimer m_dataTimer;
    DataGenerationMode m_dataGenerationMode = DataGenerationMode::MANUAL_RANGE;
    float m_currentMinRange = 0.0f;
    float m_currentMaxRange = 2.0f;
    bool m_isAutoChanging = false;

    // ========== 新增：测试状态 ==========
    bool m_isTestingOutliers = false;
    bool m_isTestingRecovery = false;

    // 辅助函数
    void setupCombinedTab();

    void setupDataRangeControls();

    void setupConfigControls();

    // ========== 新增：设置函数 ==========
    void setupInitialRangeControls();

    void setupHardLimitsControls();

    void setupTestControls();

    void updateSliderLimits(float newMin, float newMax);

    void updateStatusLabels();

    // 数据生成方法
    std::vector<float> generateManualRangePattern() const;

    std::vector<float> generateStandardPDPattern() const;

    std::vector<float> generateRandomAmplitudePattern() const;

    std::vector<float> generateSmoothlyChangingPattern() const;

    // ========== 新增：测试数据生成方法 ==========
    std::vector<float> generateOutlierTestData() const;

    std::vector<float> generateRecoveryTestData() const;
};

#endif // MAINWINDOW_H
