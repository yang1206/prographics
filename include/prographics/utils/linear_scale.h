#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

// 刻度间距与向外对齐算法参考 d3-array 3.2.4 的 ticks.js / nice.js。
// https://github.com/d3/d3-array/tree/v3.2.4/src
// 以下版权及许可适用于这些算法的改写部分：
// Copyright 2010-2023 Mike Bostock
// Permission to use, copy, modify, and/or distribute this software for any purpose
// with or without fee is hereby granted, provided that the above copyright notice
// and this permission notice appear in all copies.
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
// OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
// TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
// THIS SOFTWARE.

namespace ProGraphics {

// 无状态的线性量程算法；时间滞回与图表缓存由上层负责。
class LinearScale {
public:
    using Range = std::pair<float, float>;

    static double span(const Range& range) {
        return static_cast<double>(range.second) - range.first;
    }

    static bool contains(const Range& outer, const Range& inner) {
        return inner.first >= outer.first && inner.second <= outer.second;
    }

    // 非有限端点拒绝；反向端点归一化；退化区间在浮点精度内扩展。
    static bool normalize(float min, float max, Range& out, double minimumSpan = 0.001) {
        if (!std::isfinite(min) || !std::isfinite(max)) return false;
        if (min > max) std::swap(min, max);
        const double center = (static_cast<double>(min) + max) * 0.5;
        const double width = static_cast<double>(max) - min;
        minimumSpan = std::isfinite(minimumSpan) ? std::max(0.0, minimumSpan) : 0.001;
        if (width > 0.0 && width >= minimumSpan) {
            out = {min, max};
            return true;
        }
        const double precisionSpan = std::abs(center) * std::numeric_limits<float>::epsilon() * 4;
        const double half = std::max({minimumSpan, precisionSpan, 0.001}) * 0.5;
        out = fromDouble(center - half, center + half);
        return out.first < out.second;
    }

    // D3 的 1、2、5 × 10^n 间距选择；小数步长用负倒数表示，减少对齐误差。
    static double tickIncrement(double width, int targetTicks = 6) {
        if (!(width > 0.0) || !std::isfinite(width)) return 0.0;
        const double rough = width / (std::clamp(targetTicks, 2, 1000) - 1);
        const double power = std::floor(std::log10(rough));
        const double error = rough / std::pow(10.0, power);
        const double factor = error >= std::sqrt(50.0) ? 10.0 :
                              error >= std::sqrt(10.0) ? 5.0 :
                              error >= std::sqrt(2.0) ? 2.0 : 1.0;
        return power < 0 ? -std::pow(10.0, -power) / factor : std::pow(10.0, power) * factor;
    }

    static double tickStep(double width, int targetTicks = 6) {
        const double increment = tickIncrement(width, targetTicks);
        return increment < 0 ? -1.0 / increment : increment;
    }

    // 旧固定量程的向上选档规则，保留已有固定量程的刻度布局。
    static double ceilingTickStep(double width, int targetTicks = 6) {
        if (!(width > 0.0) || !std::isfinite(width)) return 1.0;
        const double rough = width / (std::clamp(targetTicks, 2, 1000) - 1);
        const double magnitude = std::pow(10.0, std::floor(std::log10(rough)));
        const double normalized = rough / magnitude;
        return (normalized <= 1 ? 1 : normalized <= 2 ? 2 : normalized <= 5 ? 5 : 10) * magnitude;
    }

    static Range nice(const Range& range, int targetTicks = 6) {
        Range normalized;
        if (!normalize(range.first, range.second, normalized, 0.0)) return {0.0f, 1.0f};
        double min = normalized.first;
        double max = normalized.second;
        double previous = 0.0;
        // D3 迭代至步长稳定；增加次数上限防止异常输入造成无界循环。
        for (int i = 0; i < 16; ++i) {
            const double increment = tickIncrement(max - min, targetTicks);
            if (increment == previous || increment == 0.0 || !std::isfinite(increment)) break;
            if (increment > 0) {
                min = std::floor(min / increment) * increment;
                max = std::ceil(max / increment) * increment;
            } else {
                min = std::ceil(min * increment) / increment;
                max = std::floor(max * increment) / increment;
            }
            previous = increment;
        }
        // float 转换也必须向外，不能因舍入裁掉边界数据。
        auto result = fromDouble(min, max);
        result.first = std::min(result.first, normalized.first);
        result.second = std::max(result.second, normalized.second);
        return result;
    }

    static Range padded(const Range& data, float lowerRatio, float upperRatio,
                        float minimumSpan, float sameValueRatio, int targetTicks) {
        Range range;
        const double constantSpan = data.first == data.second
            ? std::max<double>(minimumSpan, std::abs(static_cast<double>(data.first)) * sameValueRatio * 2.0)
            : minimumSpan;
        normalize(data.first, data.second, range, constantSpan);
        const double width = span(range);
        return nice(fromDouble(range.first - width * lowerRatio,
                               range.second + width * upperRatio), targetTicks);
    }

    // 参考 ViewBox：先限制跨度，再平移到边界内，避免逐端夹取产生零跨度/反向区间。
    static Range constrained(const Range& range, const Range& limits) {
        if (span(range) >= span(limits)) return limits;
        double min = range.first;
        double max = range.second;
        if (min < limits.first) {
            max += limits.first - min;
            min = limits.first;
        }
        if (max > limits.second) {
            min -= max - limits.second;
            max = limits.second;
        }
        auto result = fromDouble(min, max);
        result.first = std::max(result.first, limits.first);
        result.second = std::min(result.second, limits.second);
        return result.first < result.second ? result : limits;
    }

    static Range united(const Range& a, const Range& b) {
        return {std::min(a.first, b.first), std::max(a.second, b.second)};
    }

private:
    static Range fromDouble(double min, double max) {
        const double limit = std::numeric_limits<float>::max();
        min = std::clamp(min, -limit, limit);
        max = std::clamp(max, -limit, limit);
        float lower = static_cast<float>(min);
        float upper = static_cast<float>(max);
        if (lower > min) lower = std::nextafter(lower, -std::numeric_limits<float>::infinity());
        if (upper < max) upper = std::nextafter(upper, std::numeric_limits<float>::infinity());
        return {lower, upper};
    }
};

// 保留原工具函数入口，统一委托给新的线性量程算法。
inline float calculateNiceTickStep(float range, int targetTicks = 6) {
    return static_cast<float>(LinearScale::ceilingTickStep(range, targetTicks));
}

inline float findNiceNumber(float value, bool ceiling = false) {
    if (!std::isfinite(value) || value == 0.0f) return value;
    const double magnitude = std::pow(10.0, std::floor(std::log10(std::abs(static_cast<double>(value)))));
    const double candidates[] = {1, 2, 2.5, 5, 10};
    double lower = magnitude;
    double upper = magnitude * 10;
    for (double factor : candidates) {
        const double candidate = factor * magnitude;
        if (candidate <= std::abs(value)) lower = candidate;
        if (candidate >= std::abs(value)) { upper = candidate; break; }
    }
    // ceiling/floor 是数值方向，对负数也成立。
    return static_cast<float>(value > 0 ? (ceiling ? upper : lower) : -(ceiling ? lower : upper));
}

inline std::pair<float, float> calculateNiceRange(float min, float max, int targetTickCount = 6,
    bool preserveBuffer = true, float dataMax = 0.0f, float bufferSize = 0.0f) {
    LinearScale::Range range;
    if (!LinearScale::normalize(min, max, range)) return {0.0f, 1.0f};
    if (preserveBuffer && std::isfinite(dataMax) && std::isfinite(bufferSize) && bufferSize > 0) {
        const double paddedMax = static_cast<double>(dataMax) + LinearScale::span(range) * bufferSize;
        range.second = std::max(range.second, static_cast<float>(std::min<double>(paddedMax, std::numeric_limits<float>::max())));
    }
    return LinearScale::nice(range, targetTickCount);
}

} // namespace ProGraphics
