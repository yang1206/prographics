#pragma once
#include "prographics/charts/offline_data.h"
#include "prographics/charts/coordinate/axis_ticks.h"
#include "prographics/charts/coordinate/axis_name.h"

namespace ProGraphics::OfflineView {
// 刻度几何统一用 [0,1]，格式化时还原 double 真坐标，序列轴不受 float 绝对精度限制。
inline void configureAxis(AxisTicks::TickConfig& ticks, AxisName::NameConfig& name, const StaticAxis& axis) {
    ticks.visible = true;
    ticks.range = AxisTicks::TickConfig::Range(0.0f, 1.0f, 0.25f);
    ticks.formatter = [axis](float t) {
        return QString::number(axis.minimum + static_cast<double>(t) * (axis.maximum - axis.minimum), 'g', 17);
    };
    name.visible = true;
    name.text = axis.name;
    name.unit = axis.unit;
}
} // namespace ProGraphics::OfflineView
