#include "../src/scale_history.hpp"
#include "../src/neural_scale_policy.hpp"
#include <cassert>
#include <cstdio>

static constinit nr::ScaleHistory embedded_initialization;
int main()
{
    using nr::MultipassMotionMode;
    for (auto mode : {MultipassMotionMode::reuse_game_motion,
                      MultipassMotionMode::zero_later_passes,
                      MultipassMotionMode::zero_and_reset_later_passes})
        assert(nr::motion_resample_filter(mode, 0) == 3 &&
            !nr::reset_later_pass_history(mode, 0));
    for (unsigned pass = 1; pass < 10; ++pass)
    {
        assert(nr::motion_resample_filter(MultipassMotionMode::reuse_game_motion, pass) == 3);
        assert(nr::motion_resample_filter(MultipassMotionMode::zero_later_passes, pass) == 5);
        assert(nr::motion_resample_filter(MultipassMotionMode::zero_and_reset_later_passes, pass) == 5);
        assert(!nr::reset_later_pass_history(MultipassMotionMode::reuse_game_motion, pass));
        assert(!nr::reset_later_pass_history(MultipassMotionMode::zero_later_passes, pass));
        assert(nr::reset_later_pass_history(MultipassMotionMode::zero_and_reset_later_passes, pass));
    }
    assert(nr::clamp_multipass_motion_mode(-1) == MultipassMotionMode::reuse_game_motion);
    assert(nr::clamp_multipass_motion_mode(99) == MultipassMotionMode::reuse_game_motion);
    auto &history = embedded_initialization;
    assert(!history.find(0, 0));
    unsigned resets = 0;
    for (unsigned generation = 1; generation <= 200; ++generation)
    {
        for (unsigned frame = 0; frame < 100; ++frame)
        {
            // Two passes alternate arbitrarily many texture slots. Slot identity
            // must never become a history key; exactly one reset per pass/scale.
            for (unsigned pass = 0; pass < 2; ++pass)
            {
                auto *entry = history.find(1, pass);
                assert(entry);
                const bool reset = entry->generation != generation;
                if (frame == 0) {
                    // A failed first record does not consume the reset request.
                    assert(reset);
                    assert(history.find(1, pass)->generation != generation);
                }
                resets += reset;
                entry->generation = generation;
            }
        }
    }
    assert(resets == 400);
    auto *first=history.find(1,0), *second=history.find(1,1), *third=history.find(1,2);
    first->generation=second->generation=third->generation=201;
    history.find(2,0)->generation=201;
    history.invalidate_from(1,1); // Later pass missed; preserve the first pass.
    assert(first->generation==201 && second->generation==0 && third->generation==0);
    assert(history.find(2,0)->generation==201);
    second->generation=third->generation=201; // Successful recovery consumes reset.
    history.invalidate_from(1,0); // First-pass bypass changes the whole chain.
    assert(first->generation==0 && second->generation==0 && third->generation==0);
    history.forget(2);
    assert(history.find(2, 0)->generation == 0);
    history.forget(1);
    assert(history.find(1, 0)->generation == 0);
    assert(history.find(2, 0));
    nr::ScaleHistory bounded;
    for (unsigned i = 0; i < nr::ScaleHistory::capacity; ++i) assert(bounded.find(1, i));
    assert(!bounded.find(2, 0));
    bounded.forget(1);
    assert(bounded.find(2, 0));
    puts("Scale history: 200 generations, 40,000 successful pass records; reset-once, failure retry, independent devices/passes, capacity/reuse passed.");
}
