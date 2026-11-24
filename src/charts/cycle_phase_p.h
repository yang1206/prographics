#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ProGraphics::CyclePhase {

inline bool validRange(float min, float max) {
    return std::isfinite(min) && std::isfinite(max) && min < max;
}

// 周期采样采用 [min, max)，末点不重复下一周期的起点。调用方保证 count > 0。
inline double samplePhase(std::size_t index, std::size_t count, float min, float max) {
    return static_cast<double>(min) +
        (static_cast<double>(max) - min) * (static_cast<double>(index) / count);
}

inline float toPosition(double phase, float min, float max, float axisLength) {
    return static_cast<float>((phase - min) / (static_cast<double>(max) - min) * axisLength);
}

inline float fromPosition(float position, float min, float max, float axisLength) {
    return static_cast<float>(static_cast<double>(min) +
        static_cast<double>(position) / axisLength * (static_cast<double>(max) - min));
}

// 相位刻度均分四段；极小区间的步长不得向下溢出为零。
inline float tickStep(float min, float max) {
    const float step = static_cast<float>((static_cast<double>(max) - min) / 4.0);
    return step > 0.0f ? step : max - min;
}

// 分桶只选择峰值，不改变峰值采样点的相位；同幅值时保留最早的有效点。
// visitor 接收原始采样下标及幅值，无效值不输出。
template<class Emit>
void forEachPeak(const std::vector<float>& samples, int requestedCount, Emit&& visitor) {
    const std::size_t count = samples.size();
    if (count == 0) return;
    if (requestedCount <= 0 || static_cast<std::size_t>(requestedCount) >= count) {
        for (std::size_t i = 0; i < count; ++i) {
            if (std::isfinite(samples[i])) visitor(i, samples[i]);
        }
        return;
    }

    const auto buckets = static_cast<std::size_t>(requestedCount);
    for (std::size_t b = 0; b < buckets; ++b) {
        // 每周期点数由图表的 int 配置给定，64 位乘法避免分桶边界计算溢出。
        const std::size_t begin = static_cast<std::uint64_t>(b) * count / buckets;
        const std::size_t end = static_cast<std::uint64_t>(b + 1) * count / buckets;
        std::size_t peakIndex = end;
        for (std::size_t i = begin; i < end; ++i) {
            if (std::isfinite(samples[i]) && (peakIndex == end || samples[i] > samples[peakIndex])) {
                peakIndex = i;
            }
        }
        if (peakIndex != end) visitor(peakIndex, samples[peakIndex]);
    }
}

} // namespace ProGraphics::CyclePhase
