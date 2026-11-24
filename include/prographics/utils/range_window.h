#pragma once

#include "linear_scale.h"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace ProGraphics {

// 只保存周期极值。单调队列支持摊销 O(1) 更新，容量与图表保留周期严格一致。
class RangeWindow {
public:
    explicit RangeWindow(std::size_t capacity) : m_capacity(std::max<std::size_t>(1, capacity)) {}

    static bool finiteBounds(const std::vector<float>& data, LinearScale::Range& out) {
        float min = std::numeric_limits<float>::infinity();
        float max = -std::numeric_limits<float>::infinity();
        for (float value : data) {
            if (!std::isfinite(value)) continue;
            min = std::min(min, value);
            max = std::max(max, value);
        }
        out = {min, max};
        return min <= max;
    }

    void append(const std::vector<float>& data) {
        if (m_next - m_first == m_capacity) removeOldest();
        LinearScale::Range range;
        const auto id = m_next++;
        if (!finiteBounds(data, range)) return;
        while (!m_min.empty() && m_min.back().value >= range.first) m_min.pop_back();
        while (!m_max.empty() && m_max.back().value <= range.second) m_max.pop_back();
        m_min.push_back({id, range.first});
        m_max.push_back({id, range.second});
    }

    void removeOldest() {
        if (m_first == m_next) return;
        ++m_first;
        if (!m_min.empty() && m_min.front().id < m_first) m_min.pop_front();
        if (!m_max.empty() && m_max.front().id < m_first) m_max.pop_front();
    }

    bool bounds(LinearScale::Range& out) const {
        if (m_min.empty() || m_max.empty()) return false;
        out = {m_min.front().value, m_max.front().value};
        return true;
    }

    void clear() { m_min.clear(); m_max.clear(); m_first = m_next = 0; }

private:
    struct Entry { uint64_t id; float value; };
    std::size_t m_capacity;
    uint64_t m_first = 0;
    uint64_t m_next = 0;
    std::deque<Entry> m_min;
    std::deque<Entry> m_max;
};

} // namespace ProGraphics
