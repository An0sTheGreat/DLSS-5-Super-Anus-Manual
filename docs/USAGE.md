# Usage and configuration

## Presets and operating mode

Use the existing **RenoDX DLSS_A** preset buttons to select Off or Preset 1–3.
The most recently enabled preset is saved and restored on future launches.

Hook Method controls where the add-on looks for a usable DLSS input. **Auto** is
the normal starting point. **Upscaled** can help games whose focus state or frame
generation changes which source is visible. Manual choices are diagnostic tools;
their behavior depends on the game pipeline.

Multipass Motion appears directly below Hook Method:

- **Reuse Game Motion** supplies the resampled game motion to every pass.
- **Zero Later-Pass Motion** preserves the earlier behavior where only Pass 1
  receives game motion.
- **Zero Motion + Reset History** also resets passes 2+ every evaluation. This is
  a diagnostic option that may shimmer or flicker.
- **Chained Temporal History (Recommended)** supplies resampled game motion while
  keeping an independent history for each pass. It is the default for new
  configurations; an existing saved selection is retained.

Changing this setting uses one native transition frame and resets pass history
before managed processing resumes. The selection persists across launches.

## Neural Rendering Performance

| Setting | Range | Behavior |
| --- | --- | --- |
| Neural Rendering Resolution | 25–150% | Stages the internal NR evaluation scale. Below 100% reduces NR cost; above 100% supersamples NR. Press Apply to activate it. |
| Reconstruction Mode | Direct / Matched Residual | Chooses how scaled NR output is combined with the native reference. |
| Neural Transfer Strength | 0–200% | Controls the strength of the neural edit. At 0%, output returns to the native reference, though NR still runs. |
| Neural Color Strength | 0–200% | Controls chromatic contribution relative to luminance/detail. Defaults to 100% for every pass. Values above 100% exaggerate chroma and may cause oversaturation, out-of-gamut color, or stronger haloing. |
| Reconstruction Sharpness | 0–100% | Applies after scaled reconstruction. Disabled only at applied 100%. |

Right-click any native RenoDX or custom Neural Rendering slider and choose
**Reset** to restore only that control. Native controls keep their RenoDX-defined
defaults and normal preset persistence. Resetting Neural Rendering Resolution
stages 100%; press **Apply** to activate it.

At applied 100%, the first pass uses original Neural Rendering. In a multipass
group, later evaluations use same-size working textures for the selected motion
policy. The controls below the resolution setting remain unavailable because
they do not affect the native-resolution path.

Recommended baseline:

- Resolution: 75%
- Reconstruction Mode: Matched Residual
- Neural Transfer Strength: 100%
- Neural Color Strength: 100%
- Reconstruction Sharpness: 0%

Change one control at a time and compare stable scenes. Lower scale values reduce
the Neural Rendering workload but cannot preserve every detail from a full-scale
network evaluation.

Scaled multipass starts only after a complete pass group can be reserved. If
the requested resolution/pass combination exceeds safe memory or in-flight
resource capacity, the current configuration remains on the native 100% path
until resolution, pass count, preset, or hook method changes. This deliberate
stable fallback avoids alternating scaled/native frames.

FrameGen hook selections run NR once per real source frame on the native Super
Resolution output. The add-on does not run NR, record GPU work, or change
resources inside FrameGen callbacks. This keeps vendor Frame Generation timing
and parameters untouched while allowing resolution scaling and multipass NR.
Pass 1 always receives the game's motion vectors. Later passes follow the
selected Multipass Motion policy.

Values above 100% increase internal NR detail and cost without changing the
game's output resolution or DLSS Super Resolution setting. They can consume
substantially more GPU time and working-texture memory.

## Controls

| Default | Action |
| --- | --- |
| F5 | Capture an NR ON/OFF pair |
| F6 | Toggle Neural Rendering |
| F7 | Cycle Preset 1 → 2 → 3 → 1 |
| = / + | Increase pass count (maximum 10) |
| - / _ | Decrease pass count (minimum 1) |

Click a binding in the Controls section, then press the replacement key. Preset
Preset, toggle, and pass-count actions display an upper-right notification for three seconds: one
second at full opacity followed by a two-second fade.

## Screenshots

F5 or **Capture Screenshot** records a pair with Neural Rendering enabled and
disabled. PNG files are written under `DLSS5 Screenshots` in ReShade's base
directory.

- **HDR mode off:** captures the normal PNG path.
- **HDR mode on:** on DX12, captures successive displayed frames with a target
  gap below 100 ms and a hard 500 ms expiry. UI and small scene movement can
  differ between images.
- DX11 retains the native same-frame pair path.

HDR output is an SDR rendition stored as PNG. It is intended for convenient
visual comparison and is not a lossless HDR master.

Capture requires active Neural Rendering and registered lifetime tracking. If a
capture fails, check `ReShade.log` for the specific rejection reason.

## Configuration keys

The add-on stores values in `ReShade.ini`, principally under
`RenoDXNeuralResolution`. Relevant keys include:

- `CostResolveMode`
- `CostTransferPercent`
- `CostColorPercent`
- `MultipassMotionMode`
- `LastEnabledPreset`
- `NRToggleKey`
- `PresetCycleKey`
- `NRScreenshotKey`
- `PassCountIncreaseKey`
- `PassCountDecreaseKey`
- `ScreenshotHDR`

Edit these through the ReShade interface where possible. Close the game before
manually changing the INI.

- `ConfigSchema`
## Runtime API and Debug

Runtime API and Debug are collapsed by default. Runtime API reports the observed
presentation API and active backend. Debug counters are intended for issue
reports and may make the panel considerably taller when expanded.
