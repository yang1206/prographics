#include "prographics/utils/projection.h"

namespace ProGraphics {
Projection::Projection(ProjectionType type) : m_type(type) {}

QMatrix4x4 Projection::getMatrix() const {
  QMatrix4x4 projection;

  switch (m_type) {
  case ProjectionType::Perspective:
    projection.perspective(m_fov, m_aspectRatio, m_nearPlane, m_farPlane);
    break;

  case ProjectionType::Orthographic:
    projection.ortho(m_left, m_right, m_bottom, m_top, m_nearPlane, m_farPlane);
    break;
  }

  return projection;
}

void Projection::setPerspectiveParams(float fov, float aspectRatio,
                                      float nearPlane, float farPlane) {
  m_fov = fov;
  m_aspectRatio = aspectRatio;
  m_nearPlane = nearPlane;
  m_farPlane = farPlane;
}

void Projection::setOrthographicParams(float left, float right, float bottom,
                                       float top, float nearPlane,
                                       float farPlane) {
  m_left = left;
  m_right = right;
  m_bottom = bottom;
  m_top = top;
  m_nearPlane = nearPlane;
  m_farPlane = farPlane;
}

void Projection::setOrthographicParams(float width, float height,
                                       float nearPlane, float farPlane) {
  float halfWidth = width * 0.5f;
  float halfHeight = height * 0.5f;
  setOrthographicParams(-halfWidth, halfWidth, -halfHeight, halfHeight,
                        nearPlane, farPlane);
}

void Projection::setType(ProjectionType type) {
  if (m_type == type)
    return;

  m_type = type;
}
} // namespace ProGraphics
