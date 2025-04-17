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

private:
    // 数据生成模式
    enum class DataGenerationMode {
        MANUAL_RANGE,       // 手动设定范围模式
        RANDOM_AMPLITUDE,   // 随机幅值模式
        SMOOTH_CHANGING,    // 平滑变化模式
        STANDARD_PD         // 标准放电模式
    };

    // UI组件
    QTabWidget *m_tabWidget;
    QWidget *m_combinedTab;
    QGridLayout *m_combinedLayout;
    
    // 图表
    ProGraphics::PRPSChart *m_prpsChart;
    ProGraphics::PRPDChart *m_prpdChart;
    
    // 数据控制
    QGroupBox *m_dataRangeGroup;
    QSlider *m_minRangeSlider;
    QSlider *m_maxRangeSlider;
    QDoubleSpinBox *m_minRangeInput;
    QDoubleSpinBox *m_maxRangeInput;
    QPushButton *m_applyRangeButton;
    QPushButton *m_resetRangeButton;
    QCheckBox *m_autoChangeCheckbox;
    
    // 动态量程配置控制
    QGroupBox *m_configGroup;
    QDoubleSpinBox *m_bufferRatioInput;
    QDoubleSpinBox *m_responseSpeedInput;
    QCheckBox *m_smartAdjustmentCheckbox;
    QSpinBox *m_targetTickCountInput;
    QPushButton *m_applyConfigButton;
    
    // 数据生成
    QTimer m_dataTimer;
    DataGenerationMode m_dataGenerationMode = DataGenerationMode::MANUAL_RANGE;
    float m_currentMinRange = 0.0f;
    float m_currentMaxRange = 100.0f;
    bool m_isAutoChanging = false;
    
    // 最小量程控制
    QCheckBox *m_enableMinRangeCheckbox;
    QDoubleSpinBox *m_minRangeMaxInput;
    QDoubleSpinBox *m_activationThresholdInput;
    QCheckBox *m_useFixedRangeCheckbox;
    
    // 辅助函数
    void setupCombinedTab();
    void setupDataRangeControls();
    void setupConfigControls();
    void updateSliderLimits(float newMin, float newMax);
    
    // 数据生成方法
    std::vector<float> generateManualRangePattern() const;
    std::vector<float> generateStandardPDPattern() const;
    std::vector<float> generateRandomAmplitudePattern() const;
    std::vector<float> generateSmoothlyChangingPattern() const;
};

#endif // MAINWINDOW_H
