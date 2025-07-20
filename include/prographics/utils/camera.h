#pragma once

#include "projection.h"
#include <QMatrix4x4>

namespace ProGraphics {
  /**
   * @brief 相机类型枚举
   */
  enum class CameraType {
    FPS, ///< 第一人称相机，适用于漫游场景
    Orbit, ///< 轨道相机，围绕目标点旋转
    Free, ///< 自由相机，无限制的运动
    Follow ///< 跟随相机，跟随目标对象移动
  };

  /**
 * @brief 相机类
 *
 * 提供多种相机模式的统一接口，支持：
 * - 第一人称视角
 * - 轨道环绕
 * - 自由移动
 * - 目标跟随
 * - 透视/正交投影切换
 */
  class Camera {
  public:
    /**
   * @brief 相机移动方向枚举
   */
    enum Movement {
      FORWARD, ///< 前进
      BACKWARD, ///< 后退
      LEFT, ///< 左移
      RIGHT, ///< 右移
      UP, ///< 上升
      DOWN ///< 下降
    };

    /**
  * @brief 构造函数
  * @param type 相机类型
  * @param projType 投影类型
  */
    explicit Camera(CameraType type = CameraType::FPS,
                    ProjectionType projType = ProjectionType::Perspective);


    /**
  * @brief 获取视图矩阵
  * @return 4x4视图矩阵
  */
    QMatrix4x4 getViewMatrix() const;

    /**
  * @brief 获取投影矩阵
  * @return 4x4投影矩阵
  */
    QMatrix4x4 getProjectionMatrix() const { return m_projection.getMatrix(); };

    void setProjectionType(ProjectionType type) { m_projection.setType(type); };

    ProjectionType getProjectionType() const { return m_projection.getType(); };


    /**
  * @brief 设置透视投影参数
  * @param fov 视场角（度）
  * @param aspectRatio 宽高比
  * @param nearPlane 近平面距离
  * @param farPlane 远平面距离
  */
    void setPerspectiveParams(float fov, float aspectRatio, float nearPlane,
                              float farPlane) {
      m_projection.setPerspectiveParams(fov, aspectRatio, nearPlane, farPlane);
    };

    /**
   * @brief 设置正交投影参数
   * @param left 左边界
   * @param right 右边界
   * @param bottom 下边界
   * @param top 上边界
   * @param nearPlane 近平面距离
   * @param farPlane 远平面距离
   */
    void setOrthographicParams(float left, float right, float bottom, float top,
                               float nearPlane, float farPlane) {
      m_projection.setOrthographicParams(left, right, bottom, top, nearPlane,
                                         farPlane);
    };

    /**
  * @brief 设置正交投影参数（简化版）
  * @param width 视口宽度
  * @param height 视口高度
  * @param nearPlane 近平面距离
  * @param farPlane 远平面距离
  */
    void setOrthographicParams(float width, float height, float nearPlane,
                               float farPlane) {
      m_projection.setOrthographicParams(width, height, nearPlane, farPlane);
    };

    // 投影参数设置和获取
    void setAspectRatio(float ratio) { m_projection.setAspectRatio(ratio); };
    // 投影参数Getters
    float getFov() const { return m_projection.getFov(); };

    float getAspectRatio() const { return m_projection.getAspectRatio(); };

    float getNearPlane() const { return m_projection.getNearPlane(); };

    float getFarPlane() const { return m_projection.getFarPlane(); };

    // 投影参数Setters
    void setFov(float fov) { m_projection.setFov(fov); };

    void setNearPlane(float nearPlane) { m_projection.setNearPlane(nearPlane); };

    void setFarPlane(float farPlane) { m_projection.setFarPlane(farPlane); };


    /**
   * @brief 获取相机旋转四元数
   * @return 旋转四元数
   */
    QQuaternion getRotation() const;

    // 获取相机的上、右、前向量
    QVector3D getUp() const { return m_up; }
    QVector3D getRight() const { return m_right; }

    /**
   * @brief 处理键盘输入
   * @param direction 移动方向
   * @param deltaTime 时间增量
   */
    void processKeyboard(Movement direction, float deltaTime);

    /**
     * @brief 处理鼠标移动
     * @param xoffset X轴偏移量
     * @param yoffset Y轴偏移量
     * @param constrainPitch 是否限制俯仰角
     */
    void processMouseMovement(float xoffset, float yoffset,
                              bool constrainPitch = true);

    /**
     * @brief 处理鼠标滚轮
     * @param yoffset Y轴偏移量
     */
    void processMouseScroll(float yoffset);

    // 轨道相机特有操作
    void orbit(float xoffset, float yoffset);

    // 轨道相机参数获取
    float getOrbitYaw() const { return m_orbitYaw; }
    float getOrbitPitch() const { return m_orbitPitch; }
    float getOrbitRadius() const { return m_orbitRadius; }
    // 轨道相机参数设置（可选，如果需要直接设置这些值）
    void setOrbitYaw(float yaw) {
      m_orbitYaw = yaw;
      updateOrbitPosition();
    }

    void setOrbitPitch(float pitch) {
      m_orbitPitch = pitch;
      updateOrbitPosition();
    }

    void setOrbitRadius(float radius) {
      m_orbitRadius = qBound(1.0f, radius, 100.0f);
      updateOrbitPosition();
    }

    void zoom(float factor);

    QVector3D getPivotPoint() const { return m_pivotPoint; }

    void setPivotPoint(const QVector3D &point);

    // 跟随相机特有操作
    void setTarget(const QVector3D &target);

    QVector3D getTarget() const {
      return m_targetPosition;
    };

    void setFollowDistance(float distance);

    void setFollowHeight(float height);

    // 通用属性获取
    CameraType getType() const { return m_type; }
    QVector3D getPosition() const { return m_position; }
    QVector3D getFront() const { return m_front; }

    // 通用属性设置
    void setType(CameraType type);

    void setPosition(const QVector3D &position) { m_position = position; }

  private:
    CameraType m_type; ///< 相机类型
    Projection m_projection; ///< 投影设置

    // 基础属性
    QVector3D m_position; ///< 相机位置
    QVector3D m_front; ///< 前向量
    QVector3D m_up; ///< 上向量
    QVector3D m_right; ///< 右向量
    QVector3D m_worldUp; ///< 世界上向量

    // 欧拉角
    float m_yaw{-90.0f}; ///< 偏航角
    float m_pitch{0.0f}; ///< 俯仰角

    // 轨道相机属性
    QVector3D m_pivotPoint{0.0f, 0.0f, 0.0f}; ///< 轨道中心点
    float m_orbitRadius{17.32f}; ///< 轨道半径
    float m_orbitYaw{-45.0f}; ///< 轨道偏航角
    float m_orbitPitch{35.264f}; ///< 轨道俯仰角

    // 跟随相机属性
    QVector3D m_targetPosition{0.0f, 0.0f, 0.0f}; ///< 目标位置
    float m_followDistance{5.0f}; ///< 跟随距离
    float m_followHeight{2.0f}; ///< 跟随高度
    float m_followSmoothing{0.1f}; ///< 跟随平滑度

    // 相机参数
    float m_movementSpeed{5.0f}; ///< 移动速度
    float m_mouseSensitivity{0.1f}; ///< 鼠标灵敏度

    /**
  * @brief 更新相机向量
  */
    void updateCameraVectors();

    /**
     * @brief 更新轨道位置
     */
    void updateOrbitPosition();

    /**
     * @brief 更新跟随位置
     */
    void updateFollowPosition();

  private:
    // 默认投影参数
    static constexpr float DEFAULT_FOV = 45.0f; ///< 默认视场角
    static constexpr float DEFAULT_NEAR_PLANE = 0.1f; ///< 默认近平面
    static constexpr float DEFAULT_FAR_PLANE = 100.0f; ///< 默认远平面
    static constexpr float DEFAULT_ORTHO_WIDTH = 10.0f; ///< 默认正交宽度
  };
} // namespace ProGraphics
