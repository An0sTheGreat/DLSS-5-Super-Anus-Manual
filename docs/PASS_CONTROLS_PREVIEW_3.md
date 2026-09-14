# Pass-controls preview 3 — layout and build details

Build ID: `1.0.3-pass-controls.3`. Addon-only preview, not a new manager release.

## Changes

The relevant controls now appear in this order:

1. Neural Detail and Colour
2. Neural Rendering Performance
3. Advanced / pass count
4. Per-Pass Controls

Existing input/capture controls follow. The final four sections are **Debug → Runtime API → Links → About**, collapsed initially and still expandable by the user.

About displays `Build: V1.0.3 | D: 2026-09-12 | T: 13:57:16` for this package. Future builds use their actual local build timestamp. Native Windows Properties → Details exposes file version `1.0.3.3`, product version `1.0.3-preview.3`, and build/date/time in Comments.

Rendering, shaders, defaults, configuration keys and API support are unchanged from the working preview. Additional-pass controls still inherit the base settings unless customized. They remain DX12-only. No settings reset is needed.

## Install

Close the game. Back up its current addon and replace that one file with this package's `renodx-dlss5-super-anus.addon64` in the same location. Do not leave two active addon copies. Keep the existing ReShade installation, configuration and user-supplied DLSS DLLs. No NVIDIA DLLs are included. Restore the backed-up addon to roll back.

## Verification

- ImGui regression: 144 cases / 432 frames, section order, alignment, collapsed defaults and disabled-state isolation; 1,260 sharpness cases and pass-control persistence checks passed.
- DX11 lifecycle, native-SR duplicate guard and Vulkan export regression checks passed.
- Windows version APIs read the expected metadata; binary validation checks the About operands and preserves manifest content. The initial resource relocation failed the isolated load test; all resource data references were corrected before packaging.
- Corrected addon loaded in the isolated ReShade host and completed 400 frames, 12 preset transactions and 1,199 synthetic MFG callbacks. This tests loading/callback forwarding, not generated frames or NR image quality.
- Production shader bytes match previews 1 and 2. No new in-game acceptance run was performed.

Build: `scripts/build/build-dx11-experimental.cmd pass-controls`.
Output: `build/pass-controls-3/renodx-dlss5-super-anus.addon64`.
Addon SHA-256: `1AC2C590B8C2D1D26500964E04CB91397154E0FDB22233283BFB9087A9F45887`.

Previous preview packages and manager v1.0.2 are preserved. Nothing is installed into games or uploaded automatically.
