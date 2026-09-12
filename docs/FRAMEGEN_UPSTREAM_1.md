# FrameGen upstream routing — v1.0.3 candidate 1

## Problem

Sustained multipass NR work inside the observed DLSSG callback could make
FrameGen submissions stop. Changing hook mode or losing and regaining focus
could temporarily restart the route, which indicated callback lifetime and
timing sensitivity rather than an NGX evaluation failure.

## Change

- FrameGen callbacks are observation-only and perform no NR GPU work.
- Manual and Auto FrameGen selections route NR through the existing native-SR
  descriptor and frame gate immediately before FrameGen consumes that output.
- The native frame gate still permits one NR group per real source frame.
- Pass 1 keeps the game's motion vectors. Pass 2 and later keep the existing
  explicit zero-motion-vector path.
- Invalid or non-FrameGen callbacks retain the original RenoDX handler.
- Auto source ownership becomes native immediately when FrameGen is selected,
  preventing a late callback from starting a competing NR route.

Vendor DLSSG arguments, resources, cadence, and evaluation are not modified.
If the validated native-SR input is unavailable, the existing native/fallback
behavior remains in force; no generated-frame NR work is synthesized.

## Verification

- A 10,000-callback manual-FrameGen stress window completed with zero NR
  evaluations inside callbacks and no changes to color, motion, depth, or MFG
  index parameters.
- Native-SR routing completed 579 NR evaluations through scale, pass-count,
  preset, Auto/manual hook, and off/on transitions. The manual FrameGen window
  used two passes while callbacks remained NR-free.
- MFG cadence exercised 1,199 callbacks across indices 1–4 with zero callback
  NR evaluations.
- Full DX11/DX12/Vulkan build validation, 433 GPU scaler resolves, DX11
  transport/lifecycle, UI, capture, keybinding, and screenshot regressions pass.

These fixtures use synthetic FrameGen parameters. A real Dawnwalker acceptance
run is still required to confirm the game's Streamline callback remains active.
