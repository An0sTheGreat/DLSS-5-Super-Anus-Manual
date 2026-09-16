#pragma once
#include <cstdint>
namespace nr {
struct FinalCaptureTiming {
    static constexpr std::uint64_t limit_ms = 500;
    std::uint64_t on_at = 0, deadline = 0;
    unsigned skipped_at = 0, success_at = 0, skips_needed = 1;
    void start(std::uint64_t now, unsigned skipped, unsigned success, unsigned passes = 1) {
        on_at = now; deadline = now + limit_ms; skipped_at = skipped; success_at = success;
        skips_needed = passes ? passes : 1;
    }
    bool expired(std::uint64_t now) const { return deadline && now >= deadline; }
    bool ready(std::uint64_t now, unsigned skipped, unsigned success) const {
        return deadline && now > on_at && !expired(now) &&
            skipped - skipped_at >= skips_needed && success == success_at;
    }
};
}
