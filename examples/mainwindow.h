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
#include "prographics/charts/prpd/prpd.h"
#include "prographics/charts/prps/prps.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onGenerateData();
    void onGenerateBatch(int count);
    void onApplyConfig();
    void onResetAll();
    void updateStatus();

private:
    void setupUI();
    std::vector<float> generateRandomData();

    ProGraphics::PRPSChart *m_prpsChart;
    ProGraphics::PRPDChart *m_prpdChart;
    
    QDoubleSpinBox *m_dataMinSpin;
    QDoubleSpinBox *m_dataMaxSpin;
    QPushButton *m_generateButton;
    
    QDoubleSpinBox *m_bufferRatioSpin;
    QDoubleSpinBox *m_responseSpeedSpin;
    QSpinBox *m_recoveryFramesSpin;
    QDoubleSpinBox *m_recoveryRatioSpin;
    QCheckBox *m_smartAdjustCheck;
    QCheckBox *m_enableRecoveryCheck;
    
    QDoubleSpinBox *m_initialMinSpin;
    QDoubleSpinBox *m_initialMaxSpin;
    
    QCheckBox *m_enableHardLimitsCheck;
    QDoubleSpinBox *m_hardLimitMinSpin;
    QDoubleSpinBox *m_hardLimitMaxSpin;
    
    QCheckBox *m_enableDynamicCheck;
    
    QLabel *m_statusLabel;
    
    QTimer *m_statusTimer;
    QTimer *m_batchTimer;
    int m_batchRemaining;
};

#endif
