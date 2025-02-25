#pragma once
#include "camera.h"
#include <QTimer>

namespace ProGraphics {

/**
 * @brief 轨道控制器类
 * 
 * 提供相机轨道控制功能，支持：
 * - 鼠标左键旋转视角
 * - 鼠标右键平移视角
 * - 鼠标滚轮缩放
 * - 带惯性的平滑运动
 * - 可配置的视角限制
 */
class OrbitControls : public QObject {
  Q_OBJECT

public:
  /**
   * @brief 构造函数
   * @param camera 要控制的相机对象
   * @param parent 父QObject对象
   */
  explicit OrbitControls(Camera *camera, QObject *parent = nullptr);

  /**
   * @brief 析构函数
   */
  ~OrbitControls() override;

  /**
   * @brief 视角限制结构
   * 
   * 用于限制相机的旋转角度和缩放范围
   */
  struct ViewLimits {
    struct {
      float min = -360.0f; ///< 最小水平角度
      float max = 360.0f;  ///< 最大水平角度
    } yaw;

    struct {
      float min = -89.0f; ///< 最小俯仰角度
      float max = 89.0f;  ///< 最大俯仰角度
    } pitch;

    struct {
      float min = 1.0f;   ///< 最小缩放距离
      float max = 100.0f; ///< 最大缩放距离
    } distance;

    bool enabled = false; ///< 是否启用限制
  };

  /**
   * @brief 按键行为控制结构
   * 
   * 用于控制各个鼠标按键的功能启用状态
   */
  struct ButtonControls {
    bool leftEnabled = true;   ///< 左键旋转
    bool rightEnabled = true;  ///< 右键平移
    bool middleEnabled = true; ///< 中键行为（预留）
    bool wheelEnabled = true;  ///< 滚轮缩放
  };

  /**
   * @brief 运动状态结构
   * 
   * 记录当前的运动状态参数
   */
  struct State {
    QVector2D rotationVelocity{0.0f, 0.0f};  ///< 旋转速度
    float zoomVelocity{0.0f};                ///< 缩放速度
    QVector3D panVelocity{0.0f, 0.0f, 0.0f}; ///< 平移速度
    float damping{0.1f};                     ///< 阻尼系数
  };

  /**
   * @brief 控制参数结构
   * 
   * 用于调节控制器的响应灵敏度
   */
  struct Parameters {
    float rotationSpeed{0.5f};      ///< 旋转速度系数
    float zoomSpeed{0.5f};          ///< 缩放速度系数
    float panSpeed{0.01f};          ///< 平移速度系数
    float momentumMultiplier{1.5f}; ///< 释放时的动量倍数
  };

  /**
   * @brief 处理鼠标按下事件
   * @param pos 鼠标位置
   * @param button 按下的按键
   */
  void handleMousePress(const QPoint &pos, Qt::MouseButton button);

  /**
   * @brief 处理鼠标移动事件
   * @param pos 鼠标位置
   * @param buttons 当前按下的按键
   */
  void handleMouseMove(const QPoint &pos, Qt::MouseButtons buttons);

  /**
   * @brief 处理鼠标释放事件
   * @param button 释放的按键
   */
  void handleMouseRelease(Qt::MouseButton button);

  /**
   * @brief 处理鼠标滚轮事件
   * @param delta 滚轮增量
   */
  void handleWheel(float delta);

  // 状态控制
  void enable() { m_enabled = true; }
  void disable() { m_enabled = false; }
  bool isEnabled() const { return m_enabled; }

  // 参数设置
  void setParameters(const Parameters &params) { m_params = params; }
  const Parameters &parameters() const { return m_params; }

  // 状态获取
  const State &state() const { return m_state; }

  // 视角限制相关方法
  void setViewLimits(const ViewLimits &limits) { m_viewLimits = limits; }
  const ViewLimits &viewLimits() const { return m_viewLimits; }
  void enableViewLimits(bool enable) { m_viewLimits.enabled = enable; }

  // 按键行为控制方法
  void setButtonControls(const ButtonControls &controls) {
    m_buttonControls = controls;
  }
  const ButtonControls &buttonControls() const { return m_buttonControls; }

  void enableButton(Qt::MouseButton button, bool enable);

signals:
  void updated(); // 当相机位置更新时发出信号

private slots:
  void updateMotion();

private:
  Camera *m_camera;
  bool m_enabled{true};
  QTimer m_updateTimer;
  QPoint m_lastMousePos;
  State m_state;
  Parameters m_params;
  ViewLimits m_viewLimits;
  ButtonControls m_buttonControls;

  void startMotion();

  void stopMotion();

  bool isButtonEnabled(Qt::MouseButton button) const;

  void clampRotation(float &yaw, float &pitch) const;

  void clampDistance(float &distance) const;

  void updateTimerInterval();
};

} // namespace ProGraphics
