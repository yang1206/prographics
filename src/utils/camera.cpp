#include "prographics/utils/camera.h"
#include <QQuaternion>

namespace ProGraphics {
  Camera::Camera(CameraType type, ProjectionType projType)
    : m_type(type), m_position(0.0f, 0.0f, 3.0f), m_worldUp(0.0f, 1.0f, 0.0f),
      m_front(0.0f, 0.0f, -1.0f), m_projection(projType) {
    updateCameraVectors();
  }

  QMatrix4x4 Camera::getViewMatrix() const {
    QMatrix4x4 view;
    switch (m_type) {
      case CameraType::Orbit:
        view.lookAt(m_position, m_pivotPoint, m_worldUp);
        break;
      case CameraType::Follow:
        view.lookAt(m_position, m_targetPosition, m_worldUp);
        break;
      default: // FPS和Free相机
        view.lookAt(m_position, m_position + m_front, m_up);
        break;
    }
    return view;
  }

  QQuaternion Camera::getRotation() const {
    // 根据相机类型返回适当的旋转
    switch (m_type) {
      case CameraType::Orbit: {
        // 对于轨道相机，使用轨道角度
        QQuaternion pitchRotation =
            QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), m_orbitPitch);
        QQuaternion yawRotation =
            QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), m_orbitYaw);
        return yawRotation * pitchRotation;
      }

      default: {
        // 对于其他类型相机，使用欧拉角
        QQuaternion pitchRotation =
            QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), m_pitch);
        QQuaternion yawRotation =
            QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), m_yaw + 90.0f);
        return yawRotation * pitchRotation;
      }
    }
  }

  void Camera::setPivotPoint(const QVector3D &point) {
    if (m_type != CameraType::Orbit)
      return;

    // 保持当前与pivot point的距离
    m_pivotPoint = point;
    m_orbitRadius = (m_position - m_pivotPoint).length();

    // 更新相机位置，保持当前的观察角度
    updateOrbitPosition();
  }

  void Camera::setTarget(const QVector3D &target) {
    if (m_type != CameraType::Follow)
      return;

    m_targetPosition = target;
    updateFollowPosition();
  }

  void Camera::setFollowDistance(float distance) {
    if (m_type != CameraType::Follow)
      return;

    m_followDistance = qMax(0.1f, distance); // 确保距离不会太小
    updateFollowPosition();
  }

  void Camera::setFollowHeight(float height) {
    if (m_type != CameraType::Follow)
      return;

    m_followHeight = height;
    updateFollowPosition();
  }

  void Camera::processKeyboard(Movement direction, float deltaTime) {
    float velocity = m_movementSpeed * deltaTime;

    switch (m_type) {
      case CameraType::FPS: {
        // FPS相机限制垂直移动
        QVector3D frontNoY = m_front;
        frontNoY.setY(0);
        frontNoY.normalize();

        switch (direction) {
          case FORWARD:
            m_position += frontNoY * velocity;
            break;
          case BACKWARD:
            m_position -= frontNoY * velocity;
            break;
          case LEFT:
            m_position -= m_right * velocity;
            break;
          case RIGHT:
            m_position += m_right * velocity;
            break;
          case UP:
            m_position += m_worldUp * velocity;
            break;
          case DOWN:
            m_position -= m_worldUp * velocity;
            break;
          default:
            break;
        }
        break;
      }
      case CameraType::Free: {
        // 自由相机可以任意移动
        switch (direction) {
          case FORWARD:
            m_position += m_front * velocity;
            break;
          case BACKWARD:
            m_position -= m_front * velocity;
            break;
          case LEFT:
            m_position -= m_right * velocity;
            break;
          case RIGHT:
            m_position += m_right * velocity;
            break;
          case UP:
            m_position += m_up * velocity;
            break;
          case DOWN:
            m_position -= m_up * velocity;
            break;
        }
        break;
      }
      case CameraType::Orbit:
        // 轨道相机通过orbit()和zoom()控制
        break;
      case CameraType::Follow:
        // 跟随相机自动更新位置
        break;
    }
  }

  void Camera::processMouseMovement(float xoffset, float yoffset,
                                    bool constrainPitch) {
    xoffset *= m_mouseSensitivity;
    yoffset *= m_mouseSensitivity;

    switch (m_type) {
      case CameraType::Orbit:
        // 轨道相机使用orbit方法处理鼠标移动
        orbit(xoffset, yoffset);
        break;

      default: {
        // FPS和Free相机使用欧拉角
        m_yaw += xoffset;
        m_pitch += yoffset;

        if (constrainPitch) {
          if (m_pitch > 89.0f)
            m_pitch = 89.0f;
          if (m_pitch < -89.0f)
            m_pitch = -89.0f;
        }

        updateCameraVectors();
        break;
      }
    }
  }

  void Camera::processMouseScroll(float yoffset) {
    switch (m_type) {
      case CameraType::Orbit:
        zoom(yoffset * 0.5f);
        break;

      default:
        // 修改FOV
        float newFov = getFov() - yoffset;
        if (newFov >= 1.0f && newFov <= 179.0f) {
          setFov(newFov);
        }
        break;
    }
  }

  void Camera::orbit(float xoffset, float yoffset) {
    if (m_type != CameraType::Orbit)
      return;

    m_orbitYaw += xoffset * m_mouseSensitivity;
    m_orbitPitch += yoffset * m_mouseSensitivity;

    // 限制俯仰角
    if (m_orbitPitch > 89.0f)
      m_orbitPitch = 89.0f;
    if (m_orbitPitch < -89.0f)
      m_orbitPitch = -89.0f;

    updateOrbitPosition();
  }

  void Camera::zoom(float factor) {
    if (m_type != CameraType::Orbit)
      return;

    m_orbitRadius -= factor;
    if (m_orbitRadius < 1.0f)
      m_orbitRadius = 1.0f;
    if (m_orbitRadius > 100.0f)
      m_orbitRadius = 100.0f;

    updateOrbitPosition();
  }

  void Camera::updateOrbitPosition() {
    float x = m_orbitRadius * qCos(qDegreesToRadians(m_orbitPitch)) *
              qCos(qDegreesToRadians(m_orbitYaw));
    float y = m_orbitRadius * qSin(qDegreesToRadians(m_orbitPitch));
    float z = m_orbitRadius * qCos(qDegreesToRadians(m_orbitPitch)) *
              qSin(qDegreesToRadians(m_orbitYaw));

    m_position = m_pivotPoint + QVector3D(x, y, z);
    m_front = (m_pivotPoint - m_position).normalized();
    updateCameraVectors();
  }

  void Camera::updateFollowPosition() {
    // 计算目标后方位置
    QVector3D targetBack = -m_front.normalized();
    QVector3D desiredPosition = m_targetPosition -
                                (targetBack * m_followDistance) +
                                QVector3D(0, m_followHeight, 0);

    // 平滑插值到新位置
    m_position = m_position + (desiredPosition - m_position) * m_followSmoothing;
    m_front = (m_targetPosition - m_position).normalized();
    updateCameraVectors();
  }

  void Camera::setType(CameraType type) {
    if (m_type == type)
      return;

    m_type = type;

    // 重置相关参数
    switch (type) {
      case CameraType::Orbit:
        m_orbitRadius = (m_position - m_pivotPoint).length();
        m_orbitYaw = -90.0f;
        m_orbitPitch = 0.0f;
        updateOrbitPosition();
        break;
      case CameraType::Follow:
        m_targetPosition = m_position + m_front * m_followDistance;
        updateFollowPosition();
        break;
      default:
        break;
    }
  }

  void Camera::updateCameraVectors() {
    // 计算新的前向量
    QVector3D front;
    front.setX(qCos(qDegreesToRadians(m_yaw)) * qCos(qDegreesToRadians(m_pitch)));
    front.setY(qSin(qDegreesToRadians(m_pitch)));
    front.setZ(qSin(qDegreesToRadians(m_yaw)) * qCos(qDegreesToRadians(m_pitch)));
    m_front = front.normalized();

    // 重新计算右向量和上向量
    m_right = QVector3D::crossProduct(m_front, m_worldUp).normalized();
    m_up = QVector3D::crossProduct(m_right, m_front).normalized();
  }
} // namespace ProGraphics
