# Per-pass controls preview 1

Build ID: `1.0.3-pass-controls.1`. This is an **addon-only preview**, not a replacement manager release. The v1.0.2 manager package and its addon remain unchanged. No games are installed or modified by building this preview.

## Controls

New sections appear after **Neural Rendering Performance**, before Debug/Runtime:

- **Neural Detail and Colour**: Hue-stable Detail Strength (0–200%) and Colour Coupling (0–300%).
- **Per-Pass Controls**: collapsed initially, with collapsed Pass 2 through Pass N nodes. One pass shows no additional nodes; two passes shows Pass 2, etc., up to the existing ten-pass limit.
- Enable **Customize this pass** to independently adjust reconstruction mode, neural transfer strength, neural color strength, reconstruction sharpness, detail strength and colour coupling. Enabling customization copies the currently inherited settings. Disabling it restores inheritance. Reducing the pass count hides unused nodes without deleting their saved settings.

These are per-pass **reconstruction/output controls**, not independent NGX model preset, local-tone, skin or masking parameters. The original model controls and pass count remain where they were. No extra neural evaluations are added.

## Defaults and behaviour

All additional passes inherit existing reconstruction controls by default. Detail starts at 100%, coupling at 0%. Missing configuration keys do not change existing presets.

Detail 100% leaves the established resolve untouched; 0% returns that pass's input, including alpha. Below 100% blends toward the input. Above 100%, zero coupling scales the completed RGB result with a common, bounded luminance-ratio multiplier. This retains its hue instead of amplifying individual colour-channel differences. Near-black or non-positive luminance avoids unstable ratio amplification. Signed HDR values are not clamped to SDR.

Coupling only affects detail above 100%. At 100% coupling the adjustment uses RGB residual extrapolation; above 100% it deliberately exaggerates colour changes. This is an independent implementation, not a claim of pixel equivalence with Deep Fried Chicken.

At native NR resolution, **neutral first-pass detail keeps the existing native bypass**. Non-neutral first-pass detail opts into the existing fence-owned working textures and resolve. That adds memory/compute cost, not an NR pass. Whole-group prewarming, resource budgets, queue-fence retirement, and native fallback remain active. Unsupported resources or insufficient memory can therefore preserve native output instead of applying customization.

New controls are enabled for the DX12 NR path only. DX11 and experimental Vulkan keep their existing implementations; this preview does not claim new controls for them. DX9/OpenGL support is unchanged.

Settings save to the game's ReShade configuration in `RenoDXNeuralDetail` and `RenoDXPass2` through `RenoDXPass10`. They are global within that game/configuration, independent of the existing three preset banks; upstream presets are not rewritten. Return detail to 100% and disable pass customization to restore inherited behaviour.

## Reference and packaging

The supplied Deep Fried Chicken 1.7.4 config and public-facing documentation were inspected for control behaviour. Its binary was not executed, decompiled or modified. No Deep Fried Chicken files or NVIDIA DLLs are included in this preview.

Build with `scripts/build/build-dx11-experimental.cmd pass-controls`. Output: `build/pass-controls-1/renodx-dlss5-super-anus.addon64`. The scoped native-SR duplicate-return fix from v1.0.2 remains enabled; diagnostic boundary/input probes are not enabled.

Close the game before manually testing. Back up its existing addon, then replace that **one** addon in the same location with the preview using the exact filename above. Do not leave two active copies. Keep the game's existing ReShade and user-supplied DLSS DLLs. Restore the backed-up addon to roll back.

Automated tests are not game acceptance. Confirm BOTDW in-game FrameGen, KCD2 Smooth Motion and TLOU2 in-game FrameGen at neutral defaults first, then adjust additional passes and detail at 75%, 100% and 125% NR resolution. This preview has not been verified in those games.

## Validation record (2026-09-12)

- Final addon SHA-256: `0701F15CCE492D9763314842CFC8043761C53608A4F59578A233BDBFF8D94692`.
- Static validation preserves original binary sections outside the existing 27 verified hook sites and three display-name sites. Native-SR duplicate-return guard retained; no investigation-only boundary/input hooks enabled.
- Production shader: 433 resolve cases plus 45 detail/coupling cases on both WARP and the hardware GPU. Neutral resolve and sharpen results compare byte-for-byte with the previous shader. The baseline shader is confirmed present in the preserved v1.0.2 addon; its SHA-256 is `46BCDB63CB6BC5D36DC16426A7A4B08B813AB4CDFCAF5CE353BC09B4222524E6`.
- UI/config tests cover default inheritance, independent Pass 2, bounds, absent keys, save/load round trips, collapsed/expanded headers, 1–10 pass visibility, section order and disabled-state isolation. Existing preset/gate, lifetime, DX11 registry/rollback, and Vulkan export-lookup checks also pass.
- Live SR/NR fixture, neutral defaults: 390 successful NR evaluations; 10,240 synthetic FG callbacks without callback-side NR; presets/off, nested returns and 50–150% scale changes passed. `build/framegen-fixture-pass-controls-default-2`. Its binary precedes only the final explanatory UI text change.
- Live SR/NR fixture, customized: global detail 150%, Pass 2 customized independently; 390 NR evaluations, 363 working-path evaluations and 10,240 transparent FG callbacks. `build/framegen-fixture-pass-controls-custom-1` uses the final binary.
- MFG cadence: 1,199 synthetic callbacks (indices 1–4: 400/400/266/133), no NR evaluations, passed with the final binary in `build/framegen-fixture-pass-controls-mfg-2`. An earlier invocation incorrectly combined recovery and cadence, which use separate host loops, so its zero cadence counters correctly failed validation. The harness now rejects that combination; that invocation is not counted as a pass.
- The first default fixture reached its 55-second time limit while still progressing; its logs remain in `build/framegen-fixture-pass-controls-default-1`. A fresh run completed under the updated bounded 90-second deadline. No timeout result is counted as a pass.

The original manager v1.0.2 ZIP remains SHA-256 `58C6C5A22BBADB66D9CE4C5FCE0BB71CAC428D81F0ED8A98940247BEBC865ADF`; its addon remains `C0AC9E92DCD344C46A5B9D6ECAED5583DA4B63FAFF4B1C3A9F8A499F46DB84DB`.
