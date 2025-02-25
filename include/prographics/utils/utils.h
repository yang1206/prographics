//
// Created by Yang1206 on 2025/2/23.
//
#pragma once
#include <QVector4D>

namespace ProGraphics {
  inline QVector4D hsvToRgb(float h, float s, float v, float a) {
    if (s <= 0.0f) {
      return QVector4D(v, v, v, a);
    }

    h = std::fmod(h, 360.0f);
    if (h < 0.0f)
      h += 360.0f;

    h /= 60.0f;
    int i = static_cast<int>(h);
    float f = h - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - (s * f));
    float t = v * (1.0f - (s * (1.0f - f)));

    float r, g, b;
    switch (i) {
      case 0:
        r = v;
        g = t;
        b = p;
        break;
      case 1:
        r = q;
        g = v;
        b = p;
        break;
      case 2:
        r = p;
        g = v;
        b = t;
        break;
      case 3:
        r = p;
        g = q;
        b = v;
        break;
      case 4:
        r = t;
        g = p;
        b = v;
        break;
      default:
        r = v;
        g = p;
        b = q;
        break;
    }

    return QVector4D(r, g, b, a);
  }

  inline QVector4D calculateColor(float intensity) {
    // 使用 HSV 颜色空间生成渐变色
    // 从蓝色(240°)渐变到红色(0°)
    float hue = 240.0f - intensity * 240.0f; // 240° -> 0°
    float saturation = 1.0f; // 保持完全饱和
    float value = 1.0f; // 保持最大亮度
    float alpha = 1.0f; // 完全不透明

    // 可以根据intensity调整透明度，使得低强度的点更透明
    if (intensity < 0.3f) {
      alpha = intensity / 0.3f * 0.7f + 0.3f;
    }

    return hsvToRgb(hue, saturation, value, alpha);
  };

  inline float calculateTickStep(float range) {
    if (range <= 10.0f)
      return 1.0f;
    if (range <= 20.0f)
      return 2.0f;
    if (range <= 50.0f)
      return 5.0f;
    if (range <= 100.0f)
      return 8.0f;
    return std::pow(10.0f, std::floor(std::log10(range / 10.0f)));
  }
} // namespace ProGraphics
