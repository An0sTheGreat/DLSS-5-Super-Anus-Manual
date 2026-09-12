#pragma once
#include <algorithm>
#include <cstdint>

namespace nr
{
constexpr std::uint64_t mib = 1024ull * 1024ull;
constexpr std::uint64_t maximum_working_cache = 512ull * mib;
constexpr std::uint64_t maximum_minimum_headroom = 768ull * mib;

struct MemoryAdmission
{
    std::uint64_t cache_limit = maximum_working_cache;
    std::uint64_t reserved_headroom = 0;
    std::uint64_t available_headroom = 0;
    bool queried = false;
};

// DXGI's process usage includes the game, NGX, other mods and this add-on. Keep
// a real reserve outside our cache and never let the cache exceed 512 MiB.
inline MemoryAdmission adaptive_memory_admission(
    std::uint64_t cached_bytes,
    std::uint64_t current_usage,
    std::uint64_t process_budget,
    unsigned pass_count)
{
    if (process_budget == 0)
        return {};
    pass_count = std::clamp(pass_count, 1u, 10u);
    // Scale the floor down on WARP/small-memory adapters while retaining about
    // 1.2 GiB on a 14 GiB gaming budget for multipass NGX feature allocations.
    const std::uint64_t floor = std::min(maximum_minimum_headroom, process_budget / 16u);
    const std::uint64_t proportional = process_budget / (pass_count > 1 ? 12u : 16u);
    const std::uint64_t reserve = std::max(floor, proportional);
    const std::uint64_t safe_ceiling = process_budget > reserve ? process_budget - reserve : 0;
    const std::uint64_t headroom = safe_ceiling > current_usage ? safe_ceiling - current_usage : 0;
    MemoryAdmission result;
    result.cache_limit = std::min(maximum_working_cache, cached_bytes + headroom);
    result.reserved_headroom = reserve;
    result.available_headroom = headroom;
    result.queried = true;
    return result;
}

inline constexpr unsigned maximum_working_sets(unsigned pass_count)
{
    pass_count = std::clamp(pass_count, 1u, 10u);
    return std::clamp(pass_count * 3u, 4u, 12u);
}

inline bool prewarm_slot_available(bool pooled, bool retiring, bool sources_match)
{
    return !retiring && (pooled || sources_match);
}

// Latches pressure for the whole real-frame group. Transactional prewarm
// pressure is retried for the next group after fence retirement has had a
// chance to recycle working textures. A later-pass failure remains sticky for
// the configuration because an earlier scaled pass has already been recorded.
class MultipassGroupPolicy
{
public:
    bool use_native(unsigned generation, std::uint64_t token, unsigned pass,
                    unsigned pass_count, bool pressure)
    {
        pass_count = std::clamp(pass_count, 1u, 10u);
        if (token == 0)
        {
            if (generation != generation_ || pass == 0 || synthetic_token_ == 0)
                ++synthetic_token_;
            token = synthetic_token_;
        }
        if (generation != generation_ || token != token_)
        {
            generation_ = generation;
            token_ = token;
            native_ = pressure || blocked_generation_ == generation;
            seen_mask_ = 0;
        }
        if (pressure)
            native_ = true;
        if (pass < 31) seen_mask_ |= 1u << pass;
        expected_mask_ = (1u << pass_count) - 1u;
        return native_;
    }

    void allocation_failed(unsigned generation, std::uint64_t token, bool block_generation)
    {
        if (generation == generation_ && (token == 0 || token == token_))
            native_ = true;
        if (block_generation)
            blocked_generation_ = generation;
    }

    void reset()
    {
        generation_ = 0;
        token_ = 0;
        synthetic_token_ = 0;
        seen_mask_ = expected_mask_ = 0;
        native_ = false;
        blocked_generation_ = 0;
    }

    bool blocked(unsigned generation) const { return generation != 0 && blocked_generation_ == generation; }
    bool complete() const { return expected_mask_ != 0 && (seen_mask_ & expected_mask_) == expected_mask_; }

private:
    unsigned generation_ = 0;
    std::uint64_t token_ = 0, synthetic_token_ = 0;
    unsigned seen_mask_ = 0, expected_mask_ = 0, blocked_generation_ = 0;
    bool native_ = false;
};
} // namespace nr
