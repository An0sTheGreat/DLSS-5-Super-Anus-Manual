# Cyberpunk multipass shimmer: input trace 1

2026-09-12. Diagnostic only, not a shimmer fix or manager release.

Latest follow-up: see `CYBERPUNK_HISTORY_COMPARISON_1.md`. The unchanged official
addon reproduces the frozen-input variation too. A separate neutral-pass
transition defect is fixed locally; shimmer remains unresolved.

## Current evidence

- User confirms: one pass with FG off is clean; two passes shimmer with FG
  either on or off. Both sharpness controls were then set to zero and shimmer
  remained. BOTDW is reported working and is not the failing game.
- Live Cyberpunk log identifies preview 9. At 17:37:24 it switches to two
  passes; from 17:37:25 compact allocations are repeatedly denied under the
  existing VRAM reserve. This explains an interruption, not all visual shimmer.
- Managed pass 2+ already receives a zero-filled motion texture. The wrapper
  still forwards jitter from input offsets 0x4C/0x50 and motion scale from
  0x54/0x58. Verified in the pinned official binary's 0x5D880 wrapper at
  0x5DFA0/0x5DFC0/0x5DFE0/0x5E000. Inheriting those fields is not yet proof
  they are incorrect for this game or the cause of shimmer.
- Input reset is at 0x5D. The wrapper may additionally force reset through
  host byte 0x26D900 (0x5E058..0x5E079). Its value before entering the wrapper
  is only an observation, not proof of the final vendor reset value.
- Correction: trace 1 calls byte 0x5C `hdr`; disassembly confirms that byte is
  forwarded to `DLSSNR.DepthInverted`, NOT an HDR flag. Interpret existing logs
  accordingly. No HDR conclusion should be drawn from this diagnostic field.

## Candidate

Build with `scripts/build/build-dx11-experimental.cmd pass-input-trace`.
Output: `build/cp-pass-input-trace-1/renodx-dlss5-super-anus.addon64`.
Runtime ID: `1.0.3-pass-controls.9-input-trace.1`; PE version: `1.0.3.10`.
The distinct PE revision does not mean this is a rendering-fix preview 10.

Reuses the existing manual trace and its 4096-record bounded buffer. The
diagnostic compile flag adds pre-wrapper jitter, motion scale, reset/HDR flags,
motion/depth identities and four resource rectangles to each NR evaluation.
The capture lasts one second; existing overlay draining writes results later.
It adds no hooks, GPU readbacks, automatic capture, allocations on the observed
render thread, parameter edits, rendering changes or memory-limit changes.
Shader, pass settings, defaults, manager payload and installed games are unchanged.

This is CPU metadata, NOT texture contents, GPU execution timing, or evidence
that an NGX success code means temporally stable output. No live process was
patched or suspended. Inspecting logs from the running preview cannot supply
metadata it never recorded; using this candidate requires a game restart.

## Manual capture

1. Close Cyberpunk. Keep a copy of its current addon outside the game's addon
   search directory. Replace only that addon with this diagnostic candidate.
   Do not change ReShade, NVIDIA DLLs, the manager payload or BOTDW.
2. Launch the same scene, disable in-game FG, set NR resolution to 100%, and
   set sharpness to zero on the base and additional passes.
3. At one pass, use Debug > Capture FrameGen input trace (the existing button
   name also applies with FG off). Close the overlay for the one-second burst.
   Wait ten seconds for draining. Reopen it and wait longer if the log has not
   reached `NR V6.6 trace END`.
4. Change to two passes, let the image settle, and repeat the capture while
   shimmer is visible. Note whether any other settings changed.
5. Send the complete ReShade.log. It must contain the diagnostic BUILD ID,
   `NR pass metadata` lines and both END markers with dropped counts.

Next: compare per-pass metadata/discontinuities from those captures. Do not
zero jitter, force resets every frame, alter memory reserves, or advertise a
fix without evidence that identifies the failing path.

## Local validation

- Production wrapper test with mocked vendor call: exact metadata snapshot,
  unchanged input bytes and return code, capture drain and prior history checks.
- Existing WARP lifecycle/queue tests, DX11 isolation tests, nested-source guard,
  bounded trace tests and UI/configuration regressions pass.
- Normal non-diagnostic build and this diagnostic build both compile and pass
  static PE validation. No real-game visual acceptance or GPU input readback.
- Diagnostic addon SHA-256:
  `C9D19D9BA5988898F0CF293BE05A1FE04C2C694FF21439AF2FA050DF02B5F654`.
- Preview 9 addon and manager ZIP hashes remain unchanged. No game files,
  NVIDIA DLLs, manager payloads, GitHub branches or releases were changed.

## Follow-up: September 12 temporal reproduction

User log `b1698c81-d1c1-4581-a176-f5be45910f01` supplies 33 complete two-pass
groups and 45 one-pass evaluations at 100%, sharpness zero. Both bursts have
zero drops, every evaluation returns success, and input reset / host reset
before the wrapper remain zero. All 33 pairs inherit identical jitter. These
are CPU observations, not proof of GPU completion or valid temporal history.

Added a test-only profile to the existing `framegen_nr_host`, selected with
`NR_TEST_TEMPORAL_INPUT` and `NR_TEST_TEMPORAL_PASSES`. It uses 2293x960 SR
input/motion/depth, 3440x1440 R11G11B10 output, static patterned geometry and
zero motion. Final revision uses a stable SR-output identity and an inverted,
finite-depth plane. It runs 90 frames, measures 59 adjacent-frame comparisons
after warmup, and samples decoded RGB every four pixels. The metric is mean
absolute RGB change, not a perceptual shimmer score.

The test's MinHook shim targets only the test process's hash-profiled original
NR wrapper at 0x5D880, verifies its prologue, and checks interception count
against NR evaluation count. It changes a private input copy, never game memory.
No production shader, NR input mapping, settings or runtime policy changed.

Modes: 1 = three captured jitter values; 2 = zero jitter; 3 = zero transfer
reference; 4 = full-resolution constant auxiliary textures; 5 = frozen NR
colour input/full transfer/full colour; 6 = matching live-colour control;
7 = frozen colour plus per-evaluation reset; 8 = live colour plus reset.
Modes 5-8 deliberately exercise the neutral first-pass path without the custom
resolve. Forced reset and frozen colour are diagnostic controls, NOT proposed
production fixes. Modes 1/2/4 retained the fixture's 75% base transfer and 50%
colour; these runs are not claimed to duplicate the game's 100% transfer setting.

Key results (fixture directories under `build/framegen-fixture-cp-temporal-*`):

| Comparison | Mean adjacent-frame RGB change |
| --- | ---: |
| `depth-jitter-1`: varying NR jitter, one pass | 0.001023826 |
| `depth-zero-1`: zero NR jitter, one pass | 0.001023826 |
| `stable-zero-2`: zero jitter, two passes | 0.002064566 |
| `stable-reference-1`: zero transfer | 0.000000000 |
| `depth-frozen-1`: frozen colour, one pass, no custom resolve | 0.001424761 |
| `depth-reset-1`: same frozen colour with NR reset | 0.000000000 |

Matching auxiliary resolutions also did not improve the early control results.
The `stable-*` runs preceded the finite inverted-depth refinement; subsequent
`depth-*` one-pass comparisons reproduced the same jitter/frozen-input results.
The test NR DLL SHA matches Cyberpunk exactly:
`E16BCF15E16E13F527491CDF7845B2FE6521A738D8F7C9C721866A8496E1FC8E`.
The fixture's SR DLL differs from Cyberpunk's, and the test is not a captured
game scene. Zero-transfer stability does NOT prove the upstream image remains
unchanged when NR is enabled; the frozen-colour control addresses that separately.

Discard early `jitter-1`, `zero-1`, `jitter-2`, `zero-2` results: their first
interception point could be inlined and did not verify interception counts.
`frozen-1` failed an incorrect resolve-count expectation. `frozen-final-2`
hit the existing fence-drain bound; it is not a successful image comparison.
All results are retained rather than silently overwritten.

Reproduce a final one-pass comparison after building the existing host:

```powershell
$env:NR_TEST_MULTIPASS_STRESS = '2'
$env:NR_TEST_TEMPORAL_INPUT = '1' # use 2, 5 or 7 for the other final controls
$env:NR_TEST_TEMPORAL_PASSES = '1'
# Supply a fresh build/framegen-fixture-* directory containing ReShade.ini.
./tools/run-framegen-fixture.ps1 -Directory build/framegen-fixture-NEW-NAME -Recovery -PassInputTrace -InitialScale 100
```

Conclusion: neither blindly zeroing jitter nor merely matching auxiliary extents
is supported as the fix. Temporal variation was reproduced even with frozen
colour in the original NR evaluation path, and per-frame reset removes it in
that control. This implicates history-dependent behavior in the reproduction;
it does not establish that the game has the identical cause or that the vendor
runtime alone is defective. Constant reset would discard temporal history and
is not a safe universal fix. A rendering-fix candidate remains deferred pending
feature/history ownership and vendor-input validation. BOTDW and published
manager packages remain unchanged.

## Local validation

- Production wrapper test with mocked vendor call: exact metadata snapshot,
  unchanged input bytes and return code, capture drain and prior history checks.
- Existing WARP lifecycle/queue tests, DX11 isolation tests, nested-source guard,
  bounded trace tests and UI/configuration regressions pass.
- Normal non-diagnostic build and this diagnostic build both compile and pass
  static PE validation. No real-game visual acceptance or GPU input readback.
- Diagnostic addon SHA-256:
  `C9D19D9BA5988898F0CF293BE05A1FE04C2C694FF21439AF2FA050DF02B5F654`.
- Preview 9 addon and manager ZIP hashes remain unchanged. No game files,
  NVIDIA DLLs, manager payloads, GitHub branches or releases were changed.
