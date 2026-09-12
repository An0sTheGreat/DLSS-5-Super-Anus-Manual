# FrameGen upstream routing — v1.0.3 candidates

## Candidate 3: stable scaled multipass admission

Build `1.0.3-framegen-upstream.3` addresses the scaled/native oscillation in
Dawnwalker's September 11 log. At 70% and 110%, successful scaled evaluations
were followed by multipass reservation failures and repeated feature-dimension
changes between the requested resolution and 3440x1440. The user confirmed
candidate 2's multipass now works, and reported KCD2 working without flicker
above or below 100%; these are user reports, not automated game acceptance.

- Collect completed queue fences before counting reusable prewarm slots.
- Use the existing allocator's budget and safe eviction logic instead of a
  duplicate prewarm budget check that could reject before collection/eviction.
- If admission still fails, hold native 100% for that configuration rather than
  retrying and switching resolution every few frames. A resolution, pass-count,
  preset or hook change permits a new attempt.
- Keep the 512 MiB ceiling, adaptive headroom, command-list references, queue
  fences, motion-vector behavior and candidate 2's routing/startup fix intact.

This is not a promise that every resolution/pass combination fits the cache.
Native fallback retains NR but does not deliver the requested scaled resolution.
No game-specific exceptions or relaxed resource lifetime rules were added.

Verification: the new policy regression failed against the previous policy and
passes now. Two 10,000-frame pressure-free cases remain scaled; periodic-pressure
cases switch to native once and recover after a configuration change. A real
WARP queue-fence test rejects unfinished work and recovers two reusable slots
from a full 512 MiB synthetic cache. Existing lifetime, UI and scale-history
checks pass. The isolated GPU fixture completed 579 NR evaluations, including
501 scaled calls, across scale/pass/preset/hook changes with zero NR evaluations
inside FrameGen callbacks and no safe-100% fallback warnings. Its FrameGen
parameters are synthetic; Dawnwalker visual acceptance remains outstanding.

## Candidate 2: export-ordinal startup crash

Build `1.0.3-framegen-upstream.2` fixes the process-wide Vulkan discovery hook's
handling of `GetProcAddress` ordinal requests. Candidate 1 dereferenced a numeric
export ID as a string. KCD2 crash reports at 22:43 and 22:44 on September 11
identify the installed candidate-1 add-on at RVA `0x29D007`, the first byte read
from the export-name argument. Its installed SHA-256 was
`2969EF62FD26DC122B154F4E76FD7195F04A5185086808559A552E913B2D3DFA`.

The shared interceptor now returns the original lookup result for all ordinal
requests before inspecting any string. Named Vulkan interception and the NR
routing below are unchanged. This applies in DX11/DX12 games too because the
discovery hook intercepts process-wide export lookups.

`scripts/test/test-vulkan-export-lookup.cmd` invokes the actual production
interceptor. Before the fix, ordinal 2 reproduced `0xC0000005`; after the fix,
all 65,536 ordinal values pass, alongside missing exports, ordinary/DX12 names,
all five Vulkan wrappers, and a real Windows named lookup. The full Vulkan
release build now runs this check. This validates the reproduced crash path;
it does not establish real-game Dawnwalker multipass stability.

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
