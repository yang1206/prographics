#pragma once
#include <QMatrix4x4>

namespace ProGraphics {

/**
 * @brief 投影类型枚举
 */
enum class ProjectionType {
  Perspective,  ///< 透视投影
  Orthographic ///< 正交投影
};

/**
 * @brief 投影类
 * 
 * 提供透视投影和正交投影的统一接口，支持：
 * - 透视/正交投影切换
 * - 投影参数动态调整
 * - 标准投影矩阵生成
 */
class Projection {
public:
  /**
   * @brief 构造函数
   * @param type 投影类型，默认为透视投影
   */
  explicit Projection(ProjectionType type = ProjectionType::Perspective);

  /**
   * @brief 获取当前投影矩阵
   * @return 4x4投影矩阵
   */
  QMatrix4x4 getMatrix() const;

  /**
   * @brief 设置透视投影参数
   * @param fov 视场角（度）
   * @param aspectRatio 宽高比
   * @param nearPlane 近平面距离
   * @param farPlane 远平面距离
   */
  void setPerspectiveParams(float fov, float aspectRatio, float nearPlane,
                            float farPlane);

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
                             float nearPlane, float farPlane);

  /**
   * @brief 快速设置正交投影参数
   * @param width 视口宽度
   * @param height 视口高度
   * @param nearPlane 近平面距离
   * @param farPlane 远平面距离
   */
  void setOrthographicParams(float width, float height, float nearPlane,
                             float farPlane);

  /**
   * @brief 获取当前投影类型
   * @return 投影类型枚举值
   */
  ProjectionType getType() const { return m_type; }

  /**
   * @brief 获取视场角
   * @return 视场角（度）
   */
  float getFov() const { return m_fov; }

  /**
   * @brief 获取宽高比
   * @return 宽高比值
   */
  float getAspectRatio() const { return m_aspectRatio; }

  /**
   * @brief 获取近平面距离
   * @return 近平面距离
   */
  float getNearPlane() const { return m_nearPlane; }

  /**
   * @brief 获取远平面距离
   * @return 远平面距离
   */
  float getFarPlane() const { return m_farPlane; }

  /**
   * @brief 设置投影类型
   * @param type 新的投影类型
   */
  void setType(ProjectionType type);

  /**
   * @brief 设置视场角
   * @param fov 新的视场角（度）
   */
  void setFov(float fov) { m_fov = fov; }

  /**
   * @brief 设置宽高比
   * @param ratio 新的宽高比
   */
  void setAspectRatio(float ratio) { m_aspectRatio = ratio; }

  /**
   * @brief 设置近平面距离
   * @param near 新的近平面距离
   */
  void setNearPlane(float near) { m_nearPlane = near; }

  /**
   * @brief 设置远平面距离
   * @param far 新的远平面距离
   */
  void setFarPlane(float far) { m_farPlane = far; }

private:
  ProjectionType m_type;

  // 透视投影参数
  float m_fov{45.0f};        ///< 视场角
  float m_aspectRatio{1.0f}; ///< 宽高比
  float m_nearPlane{0.1f};   ///< 近平面
  float m_farPlane{100.0f};  ///< 远平面

  // 正交投影参数
  float m_left{-1.0f};   ///< 左边界
  float m_right{1.0f};   ///< 右边界
  float m_bottom{-1.0f}; ///< 下边界
  float m_top{1.0f};     ///< 上边界
};

} // namespace ProGraphics
