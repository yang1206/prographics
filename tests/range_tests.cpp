#include "prographics/utils/dynamic_range.h"
#include <cstdlib>
#include <iostream>

using ProGraphics::DynamicRange;
using ProGraphics::LinearScale;
using ProGraphics::RangeWindow;
using Range = LinearScale::Range;

// Release 也执行检查，不依赖可能被 NDEBUG 移除的 assert。
#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << "line " << __LINE__ << ": " << #expression << '\n'; std::exit(1); \
} } while (false)

static DynamicRange::TimePoint at(int milliseconds) {
    return DynamicRange::TimePoint{} + std::chrono::milliseconds(milliseconds);
}

static bool valid(Range range) {
    return std::isfinite(range.first) && std::isfinite(range.second) && range.first < range.second;
}

static void numericRanges() {
    Range range;
    CHECK(LinearScale::normalize(-30, -75, range));
    CHECK((range == Range{-75, -30}));
    CHECK(LinearScale::normalize(-60, -60, range));
    CHECK(valid(range) && range.first < -60 && range.second > -60);
    CHECK(LinearScale::normalize(0, 0, range));
    CHECK(valid(range));
    const float limit = std::numeric_limits<float>::max();
    CHECK(LinearScale::normalize(limit, limit, range));
    CHECK(valid(range) && range.second == limit);
    CHECK(!LinearScale::normalize(std::numeric_limits<float>::quiet_NaN(), 10, range));
    CHECK(!LinearScale::normalize(0, std::numeric_limits<float>::infinity(), range));

    // D3 的典型向外对齐、负值及小数步长。
    CHECK(std::abs(LinearScale::tickStep(1.0, 6) - 0.2) < 1e-12);
    CHECK((LinearScale::nice({0.12f, 0.89f}) == Range{0.0f, 1.0f}));
    for (const Range input : {Range{-73.9f, -31.2f}, Range{-0.0031f, 0.00019f}, Range{91, 103}}) {
        const auto nice = LinearScale::nice(input);
        CHECK(valid(nice));
        CHECK(LinearScale::contains(nice, input));
        CHECK(LinearScale::tickStep(LinearScale::span(nice)) > 0);
    }
    const auto padded = LinearScale::padded({-74, -31}, 0, .3f, 1, .1f, 6);
    CHECK(padded.first <= -74 && padded.second >= -31 + 43 * .3f);

    const Range limits{-75, -30};
    CHECK((LinearScale::constrained({100, 110}, limits) == Range{-40, -30}));
    CHECK((LinearScale::constrained({-200, -190}, limits) == Range{-75, -65}));
    CHECK(LinearScale::constrained({-200, 200}, limits) == limits);
}

static void adaptiveAndCommit() {
    DynamicRange tiny(-0.0002f, -0.0001f);
    CHECK((tiny.getDisplayRange() == Range{-0.0002f, -0.0001f}));
    DynamicRange range(-75, -30);
    CHECK(!range.updateBounds(-70, -40, at(0)));
    // 旧实现仅比较跨度，这个区间虽窄却整体越过初始上界。
    CHECK(range.updateBounds(-20, -10, at(1)));
    CHECK(LinearScale::contains(range.getDisplayRange(), {-75, -10}));
    const auto before = range.getDisplayRange();
    CHECK(!range.updateBounds(-20, -10, at(2)));
    CHECK(range.getDisplayRange() == before);
    CHECK(!range.updateRange({std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}));
    CHECK(range.getDisplayRange() == before);
    range.reset();
    CHECK((range.getDisplayRange() == Range{-75, -30}));
    CHECK(!range.updateBounds(-70, -40, at(50000)));

    range.setUseInitialRange(false);
    range.updateBounds(100, 110, at(0));
    range.setHardLimits(-75, -30);
    CHECK(valid(range.getDisplayRange()));
    CHECK(LinearScale::contains({-75, -30}, range.getDisplayRange()));
    range.updateBounds(100, 110, at(1));
    CHECK(valid(range.getDisplayRange()));
    const auto constrained = range.getDisplayRange();
    range.setHardLimits(0, 0); // 零跨度限制拒绝，保留原限制及显示。
    CHECK((range.getHardLimits() == Range{-75, -30}));
    CHECK(range.getDisplayRange() == constrained);
}

static Range shrinkAtRate(int interval) {
    DynamicRange range(-75, -30);
    range.setUseInitialRange(false);
    CHECK(range.updateBounds(-75, 20, at(0)));
    const auto high = range.getDisplayRange();
    for (int t = interval; t < 3000 + interval; t += interval) {
        const auto before = range.getDisplayRange();
        const bool changed = range.updateBounds(-75, -65, at(t));
        CHECK(changed == (before != range.getDisplayRange()));
        CHECK(range.getDisplayRange() == high);
    }
    CHECK(range.updateBounds(-75, -65, at(3000 + interval)));
    CHECK(LinearScale::span(range.getDisplayRange()) < LinearScale::span(high));
    CHECK(LinearScale::contains(range.getDisplayRange(), {-75, -65}));
    return range.getDisplayRange();
}

static void hysteresis() {
    CHECK(shrinkAtRate(20) == shrinkAtRate(2)); // 50 与 500 周期/秒
    DynamicRange range(-75, -30);
    range.updateBounds(-75, 20, at(0));
    const auto high = range.getDisplayRange();
    for (int t = 1; t < 10000; t += 100) {
        range.updateBounds(-75, (t / 100) % 2 ? 20 : -65, at(t));
        CHECK(range.getDisplayRange() == high);
    }
    auto config = range.getConfig();
    config.enableRangeRecovery = false;
    range.setConfig(config);
    CHECK(!range.updateBounds(-75, -65, at(20000)));
    CHECK(!range.updateBounds(-75, -65, at(30000)));
    CHECK(range.getDisplayRange() == high);

    config.enableRangeRecovery = true;
    config.shrinkDelayMs = 0;
    range.setConfig(config);
    CHECK(range.updateBounds(-75, -65, at(30001)));
    CHECK((range.getDisplayRange() == Range{-75, -30}));
}

static void retainedWindow() {
    RangeWindow window(3);
    Range bounds;
    window.append({-75, 20});
    window.append({-74, -65});
    window.append({-73, -65});
    CHECK(window.bounds(bounds) && bounds.second == 20);
    DynamicRange range(-75, -30);
    range.setUseInitialRange(false);
    range.updateBounds(bounds.first, bounds.second, at(0));
    const auto high = range.getDisplayRange();
    CHECK(!range.updateBounds(bounds.first, bounds.second, at(10000)));
    CHECK(range.getDisplayRange() == high); // 仍在窗口中的高值不能被收缩裁掉
    window.append({-72, -65});
    CHECK(window.bounds(bounds) && bounds.first == -74 && bounds.second == -65);
    CHECK(!range.updateBounds(bounds.first, bounds.second, at(10001)));
    CHECK(range.updateBounds(bounds.first, bounds.second, at(13001)));
    window.removeOldest();
    CHECK(window.bounds(bounds) && bounds.first == -73);
    window.append({std::numeric_limits<float>::quiet_NaN()});
    window.removeOldest();
    window.removeOldest();
    CHECK(!window.bounds(bounds));
    window.clear();
    window.append({std::numeric_limits<float>::infinity(), -60, -55});
    CHECK(window.bounds(bounds) && bounds.first == -60 && bounds.second == -55);
}

int main() {
    numericRanges();
    adaptiveAndCommit();
    hysteresis();
    retainedWindow();
    std::cout << "Range regression checks passed\n";
}
