#include "mainwindow.h"

#include "src/coordinate/TestCoordinate3d.h"
#include "src/coordinate/TestCoordinate3d.h"
#include "src/playground/PlayGround.h"
#include "src/01_textured_rect/TexturedRectWidget.h"
#include "src/02_textured_unit/TexturedUnitWidget.h"
#include "src/03_matrix/MatrixWidget.h"
#include "src/04_coordinate/CoordinateWidget.h"
#include "src/05_control_camera/ControlCameraWidget.h"
#include "src/06_3D_coordinate/3DCoordinate.h"
#include "src/coordinate/TestCoordinate2d.h"

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
  // m_tabWidget->addTab(new PlayGround(this), "示例游乐场");
  // m_tabWidget->addTab(new ThreeDCoordinate(this), "3维坐标系");
  // m_tabWidget->addTab(new TexturedRectWidget(this), "纹理矩形");
  // m_tabWidget->addTab(new TexturedUnitWidget(this), "纹理单元");
  // m_tabWidget->addTab(new MatrixWidget(this), "矩阵");
  // m_tabWidget->addTab(new CoordinateWidget(this), "坐标");
  // m_tabWidget->addTab(new ControlCameraWidget(this), "控制相机");

  // 设置窗口大小
  resize(800, 600);
  // 设置数据生成定时器
  connect(&m_dataTimer, &QTimer::timeout, this, &MainWindow::generateTestData);
  m_dataTimer.setInterval(20); // 每100ms生成一次数据
  m_dataTimer.start();
}

void MainWindow::generateTestData() {
  static float time = 0.0f;

  // 生成标准的局部放电数据
  auto cycleData = generateStandardPDPattern();

  // 添加到图表
  m_prpsChart->addCycleData(cycleData);
  m_prpdChart->addCycleData(cycleData);

  time += 0.02f; // 50Hz对应的周期时间
}

std::vector<float> MainWindow::generateStandardPDPattern() const {
  std::vector<float> cycleData(ProGraphics::PRPSConstants::PHASE_POINTS, -90.0f); // 初始化为背景噪声水平

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
