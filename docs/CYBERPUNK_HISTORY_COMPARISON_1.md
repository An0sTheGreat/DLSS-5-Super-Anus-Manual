# Cyberpunk NR history comparison 1

2026-09-12. Approved investigation completed through the official comparison,
transition fix, and local validation. Shimmer is NOT resolved. No shimmer-fix
ZIP or manager release was produced; no game installation was changed.

## Findings

The unchanged official addon and our addon reproduce the same single-pass
temporal variation with a frozen NR colour input, zero motion/jitter, 100%
resolution, and no custom resolve. Both runs use copies of Cyberpunk's exact
SR and NR DLLs. Each retains one feature handle/parameter object through all
90 frames, with reset on the first evaluation only.

The original patched diagnostic and official runs both measure mean absolute
RGB delta **0.001424761** across 59 adjacent-frame pairs. All 59 printed deltas
match. After the transition fix, a repeat using full-output FNV64 hashes also
matches the official addon on all 60 readback frames, with the same delta.

This rules out our custom resolve or divergent feature reuse as the cause of
this particular frozen-input reproduction. It does not prove a vendor defect,
that all temporal variation is objectionable, or that the synthetic pattern
has exactly the same cause as the game's shimmer. The official comparison
still uses the same test-only wrapper shim for frozen colour and zero jitter;
the official file on disk is unchanged, not a patched production addon.

An official control with frozen input plus reset every evaluation produces
zero variation. This is diagnostic evidence of history dependence, NOT a fix
to ship: constant reset would discard temporal information.

## Verified separate failure and minimal fix

At 100% with neutral first-pass controls (transfer/colour 100%, sharpness 0),
`scaled_evaluate_body` returned through the native fast path before calling
`transition_uses_native_pass`. That helper waits for every configured pass bit.
Pass 1 never supplied its bit, so later passes stayed native indefinitely after
a pass-count transition. This can disable their custom controls.

The fix moves that fast return after transition observation. The unavailable-
lifetime-events guard remains first. No NR input, motion vector, shader,
feature-retirement, fence, memory limit, reset policy, or UI/default changes.

The new test in `tests/lifetime_integration_v64.cpp` calls the actual production
entry path, with only the vendor call mocked. Before the fix it fails at
`g_transition_pass_mask == 1`; afterward the full first group is counted and
the transition clears on the next group's pass 1.

Real-GPU frozen-input 1→2→1 comparison:

- Original official addon: 120 evaluations; one stable handle per pass.
- Prior patched diagnostic: stopped at frame 43 with fixture exit 26,
  “compact admission fence drain did not complete within 12 frames.” This
  failure was not dismissed by loosening the bound or adding delay.
- Fixed candidate: 120 evaluations, 28 second-pass resolves, 5 transition-native
  calls, 20,000 transparent synthetic FG callbacks; completes all 90 frames.
- Fixed pass 1 retains its handle and reset only at frame 0. Pass 2 retains its
  own handle, with reset at initial creation (frame 30) and managed recovery
  after the transition (frame 33), then no repeated resets through frame 59.
- Whole-run temporal delta remains nonzero (0.002108371). This includes a
  pass-count change and restored processing, so it is not a comparable
  steady-state shimmer score against the stuck native path.

The transition regression is fixed; the acceptance requirement of a proven
shimmer improvement is not met. Do not label this a Cyberpunk shimmer fix.

## Artifacts and repeatability

Local candidate, not distributed:
`build/cp-pass-history-fix-1/renodx-dlss5-super-anus.addon64`

Runtime ID: `1.0.3-pass-controls.10-history.1`.
Windows file version: `1.0.3.11` (prerelease).
About: `Build: V1.0.3 | D: 2026-09-12 | T: 18:35:19`.
SHA-256: `7974BBCA8290C1E537EFD2D14101B5FD1DDE34EBDB6CC6CAC9D8273CA019B273`.

Official reference SHA-256:
`1D855CF226857DCE890CFFBF7206BA9B6497CE1D471B217C1C8B44B6CD5D27E9`.
Shared SR DLL SHA-256:
`C85F971CE023C9F3492FC7455F0B01A24BA18EA39636407A846902C4360B0B7E`.
Shared NR DLL SHA-256:
`E16BCF15E16E13F527491CDF7845B2FE6521A738D8F7C9C721866A8496E1FC8E`.

Build candidate: `scripts/build/build-dx11-experimental.cmd pass-history-fix`.
Build host: `scripts/build/build-framegen-host.cmd`.
The current root embedded DLL/map belongs to the new candidate. Matching copies
are retained alongside it and alongside the old input diagnostic. The identity
helper now prefers matching sibling symbols when present, so running either
retained build does not require replacing the root map.

Use a fresh `build/framegen-fixture-*` directory containing a copy of
`build/framegen-fixture-pass-controls-5/ReShade.ini`. Never overwrite results.
Select `NR_TEST_MULTIPASS_STRESS=2`, `NR_TEST_TEMPORAL_INPUT=5`,
`NR_TEST_TEMPORAL_PASSES=1`, `NR_TEST_TEMPORAL_HISTORY=1`; add
`NR_TEST_TEMPORAL_TOGGLE=1` for 30 frames each at 1→2→1 passes.
Run `tools/run-framegen-fixture.ps1 -Directory <fixture> -Recovery
-InitialScale 100 -CyberpunkRuntime` with ONE of:

- `-OfficialBaseline` — hash-pinned unchanged official addon, original Upscaled
  source route and no custom addon callbacks/controls.
- `-PassInputTrace` — preserved prior diagnostic.
- `-PassHistoryFix` — new local transition fix.

The host intercepts the original wrapper only in its own process, verifies its
prologue, requires every call to succeed, and validates expected call counts.
History lines report the selected slot and its stored `DLSSNR.Reset` parameter
after the wrapper returns, not an independently captured GPU reset event.
All resource readbacks occur after a real fence. Clear test environment variables
in `finally`. Close the game before GPU tests; do not kill it or install files.

Retained fixture directories under `build/framegen-fixture-cp-*`:

| Suffix | Result |
| --- | --- |
| `official-frozen-1`, `patched-frozen-1` | 90 calls, 59 matching printed deltas |
| `official-toggle-1` | 120 calls, original pass history retained |
| `patched-toggle-1` | Expected failing reproduction; exit 26 |
| `fixed-toggle-1` | 120 calls, 28 resolves; transition recovered |
| `official-hash-1`, `fixed-hash-1` | All 60 output hashes match; shimmer metric unchanged |
| `official-reset-1` | Forced-reset diagnostic, zero variation |

## Validation completed

- `scripts/build/build-v66.cmd`: lifecycle/WARP queues, history, gates,
  configuration, independent UI controls and section persistence all passed,
  including the new failing-before/passing-after neutral transition assertion.
- Candidate build: DX11 lifecycle/isolation, nested-source forwarding,
  Vulkan export/ordinal lookup, PE preservation, and Windows version checks pass.
- Hardware and WARP shader tests: 495 resolves, 24 three-pass chains, 63
  independent final-image comparisons at 25/70/99/100/101/125/150%, plus retained
  temporal-chroma and shader tests. These use synthetic deltas, not the NR model.
- `build/framegen-fixture-history-botdw-1`: 120 actual NR evaluations, 114
  resolves at two passes/70%; 20,000 transparent FG callbacks.
- `build/framegen-fixture-history-pressure-1`: 171 evaluations, 138 resolves;
  six final-image comparisons change when each pass's transfer/colour/sharpness
  control changes after memory recovery.
- `build/framegen-fixture-history-pass-toggle-1`: 100 evaluations, 83 resolves
  over the existing 2→1→2 test.
- `build/framegen-fixture-history-scale-churn-1`: three passes, 90 MiB simulated
  headroom, 100→70→101→100%; 240 evaluations, 192 resolves, effective-scale checks pass.

These are local regressions, not new BOTDW/Cyberpunk game acceptance and not
real generated-frame testing. Prior preview 9 addon, diagnostic addon, diagnostic
ZIP, and manager local-9 ZIP hashes are unchanged. Manager source/payload,
installed games, NVIDIA DLLs, and GitHub were not modified.

## Next decision

The approved plan required reporting an official-path failure before a broader
workaround. That condition has now been reached. Preserve the verified transition
fix separately; further shimmer work needs a new scoped plan rather than silently
adding constant resets, blur, or temporal accumulation. One useful next direction
is testing existing NR feature settings against this reproducible baseline and
then validating any stable result against actual Cyberpunk input/content.
