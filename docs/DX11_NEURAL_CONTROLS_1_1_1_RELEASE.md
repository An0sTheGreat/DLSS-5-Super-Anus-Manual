# DLSS 5 Super Anus Addon v1.1.1.30

## Changes since v1.1.0.23

- Restore live Neural Transfer, Neural Colour, and pass-count changes in
  supported DX11 games without restarting the game.
- Refresh reused private-DX12 command-list identities and retain compatible
  working sets across DX11 pass transitions.
- Allow the working cache to grow from 512 MiB to 1 GiB only when DXGI confirms
  sufficient safe VRAM headroom. The original reserve, allocation safeguards,
  set-count limits, and 512 MiB no-query fallback remain active.
- Remove the external DLSS 5 Bridge requirement. Existing bridge files are not
  modified by this addon or the matching manager application.

DX11 Present-hook motion-related flickering remains under investigation. This
release does not include DLSS 5 Feeder integration or claim a flicker fix.

## Build identity

- Public release: **v1.1.1**.
- Addon Windows file/product version: **1.1.1.30**.
- Runtime ID: `1.1.1-dx11-neural-controls.1`.
- SHA-256: `8F4858A8AF6992794E4C721AC13B5850714EEBDBA8E9E9B93884092603C699AA`.

## Validation

The V6.4-V6.6 regression suite, DX11 WARP lifecycle tests, static addon checks,
Windows version checks, Vulkan export lookup, command-list identity rebinding,
DX11 1 -> 2 -> 1 pass transitions, and Skyrim's reported 496 MiB plus 61 MiB
admission case are covered. Live game validation remains separate.
