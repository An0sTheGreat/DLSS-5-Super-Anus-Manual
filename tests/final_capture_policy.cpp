#include "../src/final_capture_policy.hpp"
#include "../src/screenshot_pixels.hpp"
#include <cassert>
#include <cstdio>
int main()
{
    nr::FinalCaptureTiming p;
    assert(!p.ready(10,1,1));
    p.start(1000,10,20);
    assert(!p.ready(1000,11,20));
    assert(!p.ready(1016,10,20)); // a generated/repeated frame with no bypass
    assert(p.ready(1016,11,20));
    assert(p.ready(1499,11,20));
    assert(!p.ready(1500,11,20));
    assert(!p.ready(1016,11,21)); // interleaved successful NR, reject mixed pair
    assert(p.expired(1500));
    p.start(2000,20,30,2);
    assert(!p.ready(2016,21,30)); // Pass 1 bypassed; do not capture its retained output.
    assert(p.ready(2016,22,30));  // Entire two-pass group bypassed: true zero-pass frame.
    p.start(2500,25,35,3);
    assert(!p.ready(2516,27,35));
    assert(p.ready(2516,28,35));
    p.start(3000,~0u,40,2);
    assert(p.ready(3016,1,40));   // Counter rollover preserves the pass delta.
    using namespace nr::screenshots;
    const auto a = png_rgb({.18f,.18f,.18f},Encoding::scrgb,true,80.f);
    const auto b = png_rgb({.18f*203.f/80.f,.18f*203.f/80.f,.18f*203.f/80.f},Encoding::scrgb,true,203.f);
    assert(std::fabs(a.r-b.r)<1e-6f);
    const auto c = png_rgb({.18f,.18f,.18f},Encoding::linear709,true,203.f);
    assert(std::fabs(a.r-c.r)<1e-6f); // never apply display nits to scene-linear SR
    const auto s = png_rgb({.4f,.4f,.4f},Encoding::srgb,false,203.f);
    assert(s.r == .4f);
    std::puts("Final capture: <500 ms pair acceptance, fresh bypass, mixed-frame rejection, calibrated scRGB and untouched scene-linear/SDR passed.");
}
