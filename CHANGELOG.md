# Changelog

All notable public changes are documented here.

## [1.1.0.23] - 2026-09-19

- Enable Neural Resolution, detail, colour, edge, and per-pass controls for
  DX11 games after a tracked evaluation from the official external DLSS 5 Bridge.
- Keep same-generation Bridge working resources cached while Neural Rendering
  remains active, preventing live cleanup when reducing the pass count.
- Retain valid host-owned native features during active DX11 Bridge sessions,
  preventing NVIDIA feature release during live 1 -> 2 -> 1 pass transitions.

## [1.0.9.19] - 2026-09-17

- Add a persistent startup toggle that chooses whether Neural Rendering begins
  enabled with the last active preset or disabled when the game launches.
- Add **Chained Temporal History** as the recommended fourth Multipass Motion
  option and use it by default when no saved motion selection exists.
- Move the launch-state toggle above the preset controls and add a persistent
  master switch that completely bypasses multipass edge masking.

## [1.0.8.18] - 2026-09-15

- Make Encoding and the primary Neural Detail and Colour controls restore their
  own saved values when switching between presets.
- Add preset-scoped multipass edge protection, thickness, softness, inward/outward
  shift, and a live mask visualizer. These experimental controls apply only to
  Pass 2 and later and are disabled when one pass is active.
- Combine depth discontinuities with neural residuals for later-pass edge
  suppression while retaining adjustable strength, width, softness, and offset.
- Fix the edge shader's excessive sampling and root-constant mismatch, which
  could freeze the image or produce a mostly black frame when multipass enabled.
- Preserve the nested Frame Generation source guard that prevents the recurring
  TLOU2 flicker path.
- Fix NR ON/OFF capture with multiple passes: OFF now waits until the entire
  configured pass group is bypassed, producing a true zero-pass comparison.

## [1.0.6.17] - 2026-09-13

- Default unsaved Pass 1 and additional-pass Neural Color Strength to 100%.
- Expand Neural Color Strength to 0–200% and warn that values above 100% may
  oversaturate, leave the display gamut, or strengthen haloing.
- Add a themed right-click **Reset** action to every native RenoDX and custom
  Neural Rendering slider while retaining staged resolution Apply behavior.

## [1.0.5.16] - 2026-09-12

- Add the persistent Multipass Motion selector with Reuse Game Motion as the
  tested default, plus legacy zero-motion and diagnostic reset modes.
- Give Pass 1 and each additional pass independent transfer, color, and
  sharpness controls across supported DX12 Neural Rendering resolutions.
- Share sequential-pass scratch resources, retain real-fence ownership, and
  recover controls after temporary allocation pressure.

## [1.0.3.12] - 2026-09-12

- Prevent duplicate native-SR callbacks from applying Neural Rendering twice
  when application frame counters change inside one forwarded evaluation.
- Fix transition tracking so later passes recover their configured controls
  after a neutral native first pass.
- Keep sharpening independent of transfer and invalidate interrupted dependent
  history when managed processing resumes.

## Development history after 1.0.3

### Changed

- Future unsaved Pass 1 and additional-pass colour controls default to 100%.
  Colour now ranges from 0–200%, with an in-overlay hover warning that values
  above 100% may oversaturate, leave the display gamut, or strengthen haloing.
  Existing saved values remain unchanged.
- Add a persistent Multipass Motion selector below Hook Method with Reuse Game
  Motion, Zero Later-Pass Motion, and Zero Motion + Reset History. Reuse is the
  default after strong game testing in BOTDW, TLOU2 and Cyberpunk; changing modes
  uses the existing safe transition and resets pass history once. Cyberpunk's
  two-pass shadow shimmer remains unresolved.
- Local manager preview `1.0.2-local.9` bundles addon `1.0.3-pass-controls.9`:
  independent spatial sharpening without rejected-neural-noise reinjection;
  dependent history invalidation after interrupted passes. See
  `docs/PASS_CONTROLS_PREVIEW_9.md`. Cyberpunk visual acceptance is pending.
- Local manager preview `1.0.2-local.8` bundles addon `1.0.3-pass-controls.8`:
  remove permanent compact-allocation bypass, allow fence-safe reuse during
  backoff, and retry allocation when memory recovers. Control math and motion
  handling are unchanged. See `docs/PASS_CONTROLS_PREVIEW_8.md`.
- Local manager preview `1.0.2-local.7` bundles addon `1.0.3-pass-controls.7`:
  compact native-resolution controls and controlled scale-admission fallback.
  Effective resolution is shown when different; memory and fence protections
  remain. See `docs/PASS_CONTROLS_PREVIEW_7.md` for validation and limitations.
- Local manager preview `1.0.2-local.6` bundles addon `1.0.3-pass-controls.6`:
  compatible sequential passes share a fence-owned scratch working set instead
  of allocating a full texture set per pass. Sustained multipass allocation
  regression reproduced and tested; Cyberpunk acceptance remains outstanding.
  PLAY gains a soft green pulse when ReShade and the addon are installed.
  See `docs/PASS_CONTROLS_PREVIEW_6.md`. Published releases remain unchanged.
- Local manager preview `1.0.2-local.5` bundles addon `1.0.3-pass-controls.5`:
  independent Pass 1 / extra-pass transfer, colour and sharpness; default colour
  50% and sharpness 0%; retired per-pass detail/coupling ignored. Larger pass
  groups use fence-drained cache admission. Manager reinstall actions are enabled.
  Public v.1.0.2 is unchanged; Cyberpunk acceptance remains outstanding. See
  `docs/PASS_CONTROLS_PREVIEW_5.md` for evidence and limitations.
- Manager v1.0.2 final integration uses addon `1.0.3-pass-controls.4`: independent
  DX12 Neural Transfer Strength, Neural Color Strength and Neural Sharpness in
  Neural Detail and Colour at all NR resolutions, including 100%. Old global
  detail/coupling values are ignored; per-pass controls remain supported.
- Per-pass section states start expanded and persist in ReShade configuration.
- BOTDW's focus-change freeze remains deferred in `docs/BOTDW_FREEZE_PLAN.md`.
- Local pass-controls preview 3 places Performance before Advanced/pass count,
  followed immediately by Per-Pass Controls. The bottom sections are Debug,
  Runtime API, Links and About, initially collapsed. About now includes the addon
  version and current build timestamp; Windows Details exposes file build 1.0.3.3.
  Rendering and saved settings are unchanged.
- Local pass-controls preview 2 moves only **Neural Detail and Colour** above
  **Advanced/pass count** and **Neural Rendering Performance**. Per-Pass Controls,
  saved settings, defaults and rendering behaviour are unchanged.
- Renamed both the exported add-on name and independently constructed ReShade overlay tab to `RenoDX DLSS_A`.

### Added

- Add a themed right-click **Reset** context action to every native RenoDX and
  custom Neural Rendering slider. Only the selected slider is reset through its
  existing save path; resolution stages 100% until Apply is pressed.
- Local DX12 preview: collapsible controls for additional NR passes, inheriting
  existing reconstruction settings by default, plus hue-stable detail strength
  and colour coupling. Neutral defaults retain the existing output. This is an
  addon-only preview, not part of the unchanged v1.0.2 manager package; see
  `docs/PASS_CONTROLS_PREVIEW_1.md` for scope and validation limitations.
- Added an experimental native Vulkan path that appends Neural Rendering after
  successful DLSS Super Resolution in the same Vulkan command buffer. The first
  candidate is intentionally limited to 100% resolution and one NR pass.
- Added a bounded Dawnwalker diagnostic trace that records paired DLSSG callback
  entry/exit events, callback gaps, source frames, MFG indices, vendor returns,
  selected hook/pass count, and injected NR evaluation counts.
- Added a Dawnwalker A/B candidate that evaluates later FrameGen NR passes while
  suppressing only their caller-output copyback, isolating copyback from the cost
  of extra NR work inside the DLSSG callback.

### Fixed

- Prevent duplicate native-SR return callbacks from applying NR twice when the
  application frame counter changes inside one forwarded evaluation. The tester
  confirms this resolves the reported TLOU2 in-game Frame Generation flicker.
  Manager v1.0.2 bundles the guard without investigation-only probes.
- Zeroed motion vectors after the first pass of a multi-pass Neural Rendering
  group at every internal resolution, including native 100%, preventing later
  passes from reapplying motion that occurred only before the first evaluation.
  The first pass at 100% remains on the untouched native path.

## [1.0.3] - 2026-09-08

### Added

- Expanded internal Neural Rendering resolution from 25–100% to 25–150%, with
  100% retained as the exact native bypass and supersampled NR reconstructed to
  the game's unchanged output resolution.
- Added saved and rebindable pass-count controls, defaulting to =/+ to increase
  and -/_ to decrease, with 1–10 bounds and upper-right notifications.
- Added adaptive GPU-memory admission that preserves headroom for the game,
  Frame Generation, and NGX features.
- Added observed source-frame and MFG-index context plus synthetic 2x/3x/4x
  callback-cadence coverage.

### Fixes

- Separated stream configuration epochs from resource-allocation generations,
  preventing preset, pass-count, and hook changes from retiring live native NR
  features or leaving FrameGen stuck on `Waiting`.
- Removed periodic render-thread diagnostics and repeated DXGI factory creation;
  VRAM queries now reuse a cached adapter.
- Bounded background maintenance to one destructive retirement per interval,
  avoiding multi-resource cleanup spikes while NR and render locks are held.
- Reused compatible multipass working sets instead of continuously prewarming
  and retiring replacement resources.
- Prewarmed complete working-texture groups before scaled multipass begins,
  preventing a group from switching between scaled and native output midway.
- Latched unsafe scale/pass configurations to the native path until settings
  change, preventing repeated allocation and fallback flicker.
- Expanded the in-flight resource limit according to pass count while retaining
  GPU-memory headroom and a hard cache limit.
- Grouped FrameGen transitions using the observed source frame and MFG index.
- Restored a transparent upstream FrameGen call for manual hooks at native 100%.
- Rebuilt affected idle DX11 transport slots after safe pre-submission failures
  instead of permanently poisoning the session.

### Miscellaneous

- Renamed the visible ReShade tab to `RenoDX DLSS S_A` while retaining existing
  preset and configuration identifiers.
- Restricted detailed FrameGen diagnostics to explicit frame traces.
- Added a build ID, canonical filename warning, configuration schema,
  attachment-order report, and more precise stability diagnostics.
- Added runtime-lifetime, multipass, scaling, controls, capture, and FrameGen
  cadence regression coverage.

## [1.0.2] - 2026-09-07

- Fixed reduced Neural Rendering scale remaining on the native path after a
  scale or hook transition in manual FrameGen mode.
- Added a pass-aware transition fallback for Present and other final-color
  routes that do not expose an advancing native-DLSS frame identifier.
- Added FrameGen-scaled, transition-native, and scale-fallback diagnostics
  without changing the selected hook method's color encoding.

## [1.0.1] - 2026-09-07

- Reused fence-drained DX12 working textures when games rotate FrameGen input
  resources, preventing continuous large texture allocation and retirement.
- Limited each reduced-resolution evaluation stream to four working sets and
  retained the native path when every safe slot is busy.
- Moved Neural Rendering reset history from individual texture sets to the
  device/pass stream, preventing periodic history resets and reduced-scale
  flicker as source resources rotate.
- Added a native transition frame for scale, preset, pass-count, and hook-method
  changes so stale reduced-resolution output is never presented.
- Added real-fence rotation, stream-history, and transition regressions while
  preserving DX11, UI, screenshot, preset, and keybinding behavior.

## [1.0.0] - 2026-09-07

- Unified RenoDX DLSS controls and Neural Rendering performance controls into a
  single ReShade add-on.
- Replaced the previous full-image scaling path with Neural Rendering-only cost
  scaling from 25% through 100%.
- Added staged resolution changes with an Apply button and a native 100% bypass.
- Added Matched Residual and Direct Reconstruction modes.
- Added neural transfer, color-strength, and reconstruction-sharpness controls.
- Added automatic native-input recovery and guarded multi-pass handling.
- Added an experimental integrated DX11 Neural Rendering bridge.
- Added saved presets and rebindable F5/F6/F7 controls.
- Added timed upper-right preset and NR ON/OFF notifications.
- Added NR ON/OFF PNG screenshot pairs and HDR-aware display capture.
- Reorganized the ReShade interface, collapsed diagnostic sections by default,
  and disabled unavailable controls contextually.
- Added bounded resource caches and GPU-fence-based retirement for safer repeated
  scaling, pass, preset, and capture changes.
- Validated the Cost Scaler on NVIDIA hardware and WARP, plus focused DX11,
  lifetime, UI, hotkey, capture, and recovery fixtures.

[Unreleased]: https://github.com/An0sTheGreat/DLSS-5-Super-Anus-Manual/compare/v1.0.8.18...HEAD
[1.0.8.18]: https://github.com/An0sTheGreat/DLSS-5-Super-Anus-Manual/compare/v1.0.6.17...v1.0.8.18
[1.0.6.17]: https://github.com/An0sTheGreat/DLSS-5-Super-Anus-Manual/compare/v1.0.5.16...v1.0.6.17
[1.0.5.16]: https://github.com/An0sTheGreat/DLSS-5-Super-Anus-Manual/compare/v1.0.3.12...v1.0.5.16
[1.0.3.12]: https://github.com/An0sTheGreat/DLSS-5-Super-Anus-Manual/compare/v1.0.3...v1.0.3.12
[1.0.3]: https://github.com/An0sTheGreat/DLSS-5-Super-Anus-Manual/compare/v1.0.2...v1.0.3
[1.0.2]: https://github.com/An0sTheGreat/DLSS-5-Super-Anus-Manual/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/An0sTheGreat/DLSS-5-Super-Anus-Manual/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/An0sTheGreat/DLSS-5-Super-Anus-Manual/releases/tag/v1.0.0
