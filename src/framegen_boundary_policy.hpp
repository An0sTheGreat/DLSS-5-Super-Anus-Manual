#pragma once
#include <array>
#include <cstdint>

namespace nr {
// Identity observations only. Never use this registry to own or retire GPU work.
struct BoundaryIdentities {
    std::uint64_t capture = 0;
    std::array<std::uint64_t, 128> commands{};
    std::array<std::uint64_t, 16> queues{};
    void begin(std::uint64_t id) {
        if (capture != id) { capture = id; commands = {}; queues = {}; }
    }
    template<std::size_t N>
    static bool contains(const std::array<std::uint64_t, N>& items, std::uint64_t value) {
        if (!value) return false;
        for (auto item : items) if (item == value) return true;
        return false;
    }
    template<std::size_t N>
    static bool add(std::array<std::uint64_t, N>& items, std::uint64_t value) {
        if (!value) return false;
        for (auto &item : items) if (item == value || !item) { item = value; return true; }
        return false; // No eviction: disclose incomplete coverage instead.
    }
};
}
