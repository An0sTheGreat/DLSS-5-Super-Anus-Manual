# Multipass motion/history A/B diagnostic

This package contains three diagnostic addons. It does not claim a fix and is
not intended for redistribution as a normal release.

## Variants

- `A-current-zero`: current behavior. Pass 1 receives game motion; later passes
  receive zero motion.
- `B-reuse-motion`: every pass receives the appropriately resampled game motion.
- `C-reset-later`: current zero-motion behavior, but passes 2+ reset temporal
  history on every evaluation.

## Test procedure

1. Close the game and preserve the currently installed addon.
2. Install exactly one variant as `renodx-dlss5-super-anus.addon64`.
3. Confirm its `NR BUILD ID` in `ReShade.log`.
4. Use the same save, camera path, resolution, preset, two-pass configuration,
   Matched Residual mode, 100% transfer, 50% colour and 0% sharpness.
5. Record moving silhouettes, disocclusions and camera pans with Frame Generation
   off, then on. Repeat for each variant without changing other settings.

If B improves trails without restoring Frame Generation flicker, game motion is
appropriate for the independent later-pass histories. If C alone improves trails,
later-pass temporal history is implicated but motion reuse is not yet safe. If
neither helps, repeated neural reconstruction/residual transfer is the stronger
haloing candidate.

Variant C intentionally discards later-pass temporal history and may shimmer or
flicker. Do not treat it as a production fix.

## Result

The user reported Variant B works extremely well in BOTDW, TLOU2 and Cyberpunk,
without resolving Cyberpunk's separate two-pass shadow shimmer. Version 1.0.5
therefore makes motion reuse the default and exposes all three behaviors as one
persistent in-game selector. The standalone diagnostic builds are superseded.
