#include "prographics/utils/orbit_controls.h"

#include <QtGui/qguiapplication.h>
#include <QtGui/qscreen.h>

namespace ProGraphics {

OrbitControls::OrbitControls(Camera *camera, QObject *parent)
    : QObject(parent), m_camera(camera) {
  // 确保相机是轨道相机模式
  if (m_camera->getType() != CameraType::Orbit) {
    m_camera->setType(CameraType::Orbit);
  }

  // 设置定时器
  updateTimerInterval();
  connect(&m_updateTimer, &QTimer::timeout, this, &OrbitControls::updateMotion);

  // 监听屏幕刷新率变化
  if (QGuiApplication::primaryScreen()) {
    connect(QGuiApplication::primaryScreen(), &QScreen::refreshRateChanged,
            this, &OrbitControls::updateTimerInterval);
  }
}

OrbitControls::~OrbitControls() { m_updateTimer.stop(); }

void OrbitControls::handleMousePress(const QPoint &pos,
                                     Qt::MouseButton button) {
  if (!m_enabled || !isButtonEnabled(button))
    return;
  m_lastMousePos = pos;
}

void OrbitControls::handleMouseMove(const QPoint &pos,
                                    Qt::MouseButtons buttons) {
  if (!m_enabled)
    return;

  QPoint delta = pos - m_lastMousePos;

  if (delta.manhattanLength() < 2) {
    return;
  }

  if ((buttons & Qt::LeftButton) && m_buttonControls.leftEnabled) {
    // 轨道旋转
    float dx = delta.x() * m_params.rotationSpeed;
    float dy = delta.y() * m_params.rotationSpeed;

    if (m_viewLimits.enabled) {
      float newYaw = m_camera->getOrbitYaw() + dx;
      float newPitch = m_camera->getOrbitPitch() + dy;
      clampRotation(newYaw, newPitch);

      // 计算实际的旋转增量
      dx = newYaw - m_camera->getOrbitYaw();
      dy = newPitch - m_camera->getOrbitPitch();
    }

    m_state.rotationVelocity = QVector2D(dx, dy);
    m_camera->orbit(dx, dy);
    emit updated();
  } else if ((buttons & Qt::RightButton) && m_buttonControls.rightEnabled) {
    // 平移
    float dx = -delta.x() * m_params.panSpeed;
    float dy = delta.y() * m_params.panSpeed;

    QVector3D right = m_camera->getRight();
    QVector3D up = m_camera->getUp();
    QVector3D panDelta = right * dx + up * dy;

    m_state.panVelocity = panDelta;

    // 更新pivot point
    QVector3D currentPivot = m_camera->getPivotPoint();
    m_camera->setPivotPoint(currentPivot + panDelta);
    emit updated();
  }

  m_lastMousePos = pos;
  startMotion();
}

void OrbitControls::handleMouseRelease(Qt::MouseButton button) {
  if (!m_enabled)
    return;

  if (button == Qt::LeftButton) {
    m_state.rotationVelocity *= m_params.momentumMultiplier;
  } else if (button == Qt::RightButton) {
    m_state.panVelocity *= m_params.momentumMultiplier;
  }
}

void OrbitControls::handleWheel(float delta) {
  if (!m_enabled || !m_buttonControls.wheelEnabled)
    return;

  float zoomDelta = delta * m_params.zoomSpeed * 0.1f;

  // 应用距离限制
  if (m_viewLimits.enabled) {
    float newDistance = m_camera->getOrbitRadius() - zoomDelta;
    clampDistance(newDistance);
    zoomDelta = m_camera->getOrbitRadius() - newDistance;
  }

  m_state.zoomVelocity = zoomDelta;
  m_camera->zoom(zoomDelta);
  emit updated();

  startMotion();
}

void OrbitControls::updateMotion() {
  if (!m_enabled)
    return;

  bool needsUpdate = false;

  // 更新旋转
  if (m_state.rotationVelocity.lengthSquared() > 0.0001f) {
    m_state.rotationVelocity *= (1.0f - m_state.damping);

    float dx = m_state.rotationVelocity.x();
    float dy = m_state.rotationVelocity.y();

    if (m_viewLimits.enabled) {
      float newYaw = m_camera->getOrbitYaw() + dx;
      float newPitch = m_camera->getOrbitPitch() + dy;
      clampRotation(newYaw, newPitch);
      dx = newYaw - m_camera->getOrbitYaw();
      dy = newPitch - m_camera->getOrbitPitch();
    }

    m_camera->orbit(dx, dy);
    needsUpdate = true;
  }

  // 更新平移
  if (m_state.panVelocity.lengthSquared() > 0.0001f) {
    m_state.panVelocity *= (1.0f - m_state.damping);
    QVector3D currentPivot = m_camera->getPivotPoint();
    m_camera->setPivotPoint(currentPivot + m_state.panVelocity);
    needsUpdate = true;
  }

  // 更新缩放
  if (qAbs(m_state.zoomVelocity) > 0.0001f) {
    m_state.zoomVelocity *= (1.0f - m_state.damping);
    m_camera->zoom(m_state.zoomVelocity);
    needsUpdate = true;
  }

  if (needsUpdate) {
    emit updated();
  } else {
    stopMotion();
  }
}

void OrbitControls::startMotion() {
  if (!m_updateTimer.isActive()) {
    m_updateTimer.start();
  }
}

void OrbitControls::stopMotion() { m_updateTimer.stop(); }

void OrbitControls::enableButton(Qt::MouseButton button, bool enable) {
  switch (button) {
  case Qt::LeftButton:
    m_buttonControls.leftEnabled = enable;
    break;
  case Qt::RightButton:
    m_buttonControls.rightEnabled = enable;
    break;
  case Qt::MiddleButton:
    m_buttonControls.middleEnabled = enable;
    break;
  default:
    break;
  }
}

bool OrbitControls::isButtonEnabled(Qt::MouseButton button) const {
  switch (button) {
  case Qt::LeftButton:
    return m_buttonControls.leftEnabled;
  case Qt::RightButton:
    return m_buttonControls.rightEnabled;
  case Qt::MiddleButton:
    return m_buttonControls.middleEnabled;
  default:
    return false;
  }
}

void OrbitControls::clampRotation(float &yaw, float &pitch) const {
  // 限制水平旋转角度
  if (yaw < m_viewLimits.yaw.min)
    yaw = m_viewLimits.yaw.min;
  if (yaw > m_viewLimits.yaw.max)
    yaw = m_viewLimits.yaw.max;

  // 限制垂直旋转角度
  if (pitch < m_viewLimits.pitch.min)
    pitch = m_viewLimits.pitch.min;
  if (pitch > m_viewLimits.pitch.max)
    pitch = m_viewLimits.pitch.max;
}

void OrbitControls::clampDistance(float &distance) const {
  if (distance < m_viewLimits.distance.min)
    distance = m_viewLimits.distance.min;
  if (distance > m_viewLimits.distance.max)
    distance = m_viewLimits.distance.max;
}

void OrbitControls::updateTimerInterval() {
  if (QGuiApplication::primaryScreen()) {
    float refreshRate = QGuiApplication::primaryScreen()->refreshRate();
    // 确保刷新率有效，如果无效则默认使用60
    if (refreshRate <= 0) {
      refreshRate = 60.0f;
    }
    // 计算每帧的间隔时间（毫秒）
    int interval = qRound(1000.0f / refreshRate);
    m_updateTimer.setInterval(interval);
  }
}

} // namespace ProGraphics
