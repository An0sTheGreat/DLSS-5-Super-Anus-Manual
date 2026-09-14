# Pass-controls preview 2 — layout update

Build ID: `1.0.3-pass-controls.2`. Addon-only preview; no manager release or game installation is changed.

## What changed

Only **Neural Detail and Colour** moved. The relevant section order is now:

1. Neural Detail and Colour
2. Advanced / pass count
3. Neural Rendering Performance
4. Per-Pass Controls
5. Debug / Runtime

Detail and colour remain visible when Advanced is collapsed. Per-Pass Controls stays in its previous position, with the same collapsible additional-pass nodes. Existing controls above this group are not rearranged.

Rendering code, shaders, defaults, config keys and settings behaviour are unchanged from preview 1, which the user confirmed was working. No settings reset or migration is needed.

## Controls and installation

The added detail/pass controls remain DX12-only. Detail 100% preserves the established result; 0% returns the pass input. Above 100%, zero colour coupling preserves hue through a common RGB multiplier. Coupling 100% uses RGB extrapolation; higher settings deliberately exaggerate colour changes. Coupling has no effect at or below 100% detail.

Additional passes inherit the existing settings unless **Customize this pass** is enabled. Settings remain in the game's ReShade configuration. DX11, experimental Vulkan and other API support are unchanged.

Close the game. Back up its current addon and replace that one file with `renodx-dlss5-super-anus.addon64` from this package, in the same location. Do not leave two active addon copies. Keep the existing ReShade installation and user-supplied DLSS DLLs; none are bundled here. Restore the backed-up addon to roll back.

## Verification

The ImGui regression checks assert the new order, a single detail section, alignment, collapsed/expanded Advanced states, and disabled-state isolation. The existing build also runs preset, lifetime, native-SR guard, DX11 and Vulkan export checks. The production shader is compared against the preserved preview-1 binary to confirm it is unchanged. These are automated checks, not a new in-game acceptance run.

Build command: `scripts/build/build-dx11-experimental.cmd pass-controls`.
Output: `build/pass-controls-2/renodx-dlss5-super-anus.addon64`.

Preview 1 and manager v1.0.2 packages are preserved. Nothing is installed or uploaded automatically.
