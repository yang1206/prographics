#include "charts/cycle_phase_p.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

namespace Phase = ProGraphics::CyclePhase;

#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << "line " << __LINE__ << ": " << #expression << '\n'; std::exit(1); \
} } while (false)

static bool near(double a, double b, double tolerance = 1e-6) {
    return std::abs(a - b) < tolerance;
}

using Peak = std::pair<std::size_t, float>;

static std::vector<Peak> peaks(const std::vector<float>& data, int count) {
    std::vector<Peak> result;
    Phase::forEachPeak(data, count, [&](std::size_t index, float amplitude) {
        result.emplace_back(index, amplitude);
    });
    return result;
}

static void cycleEndpoints() {
    // 四个采样点不同时包含 0 和 360；非零起点、负起点使用同一规则。
    for (const float min : {0.0f, 90.0f, -180.0f}) {
        const float max = min + 360.0f;
        for (std::size_t i = 0; i < 4; ++i) {
            const double phase = Phase::samplePhase(i, 4, min, max);
            CHECK(near(phase, min + i * 90.0));
            CHECK(near(Phase::toPosition(phase, min, max, 5), i * 1.25));
            CHECK(near(Phase::toPosition(phase, min, max, 6), i * 1.5));
        }
        CHECK(Phase::samplePhase(0, 1, min, max) == min);
        CHECK(near(Phase::samplePhase(199, 200, min, max), min + 358.2));
        CHECK(Phase::samplePhase(199, 200, min, max) < max);
        CHECK(Phase::toPosition(min, min, max, 6) == 0);
        CHECK(Phase::toPosition(max, min, max, 6) == 6);
        CHECK(Phase::fromPosition(0, min, max, 6) == min);
        CHECK(Phase::fromPosition(6, min, max, 6) == max);
        CHECK(Phase::tickStep(min, max) == 90);
    }
    CHECK(near(Phase::samplePhase(3, 4, 20, 100), 80));
    CHECK(Phase::tickStep(20, 100) == 20);

    const float limit = std::numeric_limits<float>::max();
    CHECK(near(Phase::samplePhase(1, 2, -limit, limit), 0));
    CHECK(Phase::toPosition(0, -limit, limit, 6) == 3);
    CHECK(Phase::fromPosition(3, -limit, limit, 6) == 0);
    CHECK(std::isfinite(Phase::tickStep(-limit, limit)));
    const float tiny = std::numeric_limits<float>::denorm_min();
    CHECK(Phase::tickStep(0, tiny) > 0);

    CHECK(Phase::validRange(-180, 180));
    CHECK(!Phase::validRange(90, 90));
    CHECK(!Phase::validRange(360, 0));
    CHECK(!Phase::validRange(0, std::numeric_limits<float>::infinity()));
    CHECK(!Phase::validRange(std::numeric_limits<float>::quiet_NaN(), 360));
}

static void peakLocations() {
    // 不能整除的 7 个点分为 3 桶：[0,2)、[2,4)、[4,7)。
    const std::vector<float> data{-70, -40, -30, -60, -75, -65, -20};
    const std::vector<Peak> expected{{1, -40}, {2, -30}, {6, -20}};
    CHECK(peaks(data, 3) == expected);
    for (const auto& [index, amplitude] : expected) {
        CHECK(amplitude == data[index]);
        const double phase = Phase::samplePhase(index, data.size(), 90, 450);
        CHECK(near(phase, 90.0 + index * 360.0 / 7));
    }

    // 首尾峰值保留其真实采样位置；相等峰值采用最早有效点。
    CHECK((peaks({10, 0, 0, 20}, 2) == std::vector<Peak>{{0, 10}, {3, 20}}));
    CHECK((peaks({5, 5, 9, 9}, 2) == std::vector<Peak>{{0, 5}, {2, 9}}));
    CHECK((peaks({-30}, 50) == std::vector<Peak>{{0, -30}}));
    CHECK(peaks({}, 50).empty());

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    CHECK((peaks({nan, -40, inf, -20}, 2) == std::vector<Peak>{{1, -40}, {3, -20}}));
    CHECK((peaks({nan, inf, -20, -30}, 2) == std::vector<Peak>{{2, -20}}));
    CHECK(peaks({nan, inf, -inf}, 1).empty());
    CHECK((peaks({nan, -40, inf, -20}, 0) == std::vector<Peak>{{1, -40}, {3, -20}}));
}

static void resolutionDoesNotShiftPeaks() {
    // 改变降采样数量后，任意位置的孤立峰仍保持原下标，不能移动到桶中心。
    for (const int count : {1, 2, 7, 200, 257}) {
        std::vector<float> data(static_cast<std::size_t>(count), -70.0f);
        for (int i = 0; i < count; ++i) {
            data[i] = -20;
            for (const int lines : {-1, 0, 1, 3, 50, count, count + 1}) {
                const auto selected = peaks(data, lines);
                bool found = false;
                for (const auto& [index, amplitude] : selected) {
                    if (amplitude == -20) {
                        CHECK(index == static_cast<std::size_t>(i));
                        found = true;
                    }
                }
                CHECK(found);
            }
            data[i] = -70;
        }
    }
}

int main() {
    cycleEndpoints();
    peakLocations();
    resolutionDoesNotShiftPeaks();
    std::cout << "Phase regression checks passed\n";
}
