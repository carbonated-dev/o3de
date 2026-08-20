# Volumetric fog performance validation

Use this workflow to measure the complete fog pipeline, validate the depth-aware light list, and isolate the cost of each supported fog light type. Capture GPU timings after camera motion has stopped and temporal history is stable.

## Test scenes

Keep fog settings and the camera path fixed across captures. Use at least these cases:

1. Empty fog: ambient and one directional light.
2. Punctual stress: overlapping simple point and simple spot lights.
3. Area-light stress: overlapping sphere/point and disk lights, with and without shadows.
4. Depth stress: lights distributed from `fogNear` to `fogFar` to exercise all NVLC Z bins.

Record GPU time for `FroxelInject`, `FroxelScatter`, `FroxelIntegrate`, and `VolumetricFogComposite`, plus total frame time. Report the froxel dimensions and active light counts with every result.

## Isolation controls

`r_volumetricFogLightTypeMask` selects work in `FroxelScatter`:

- bit 0 (`1`): directional
- bit 1 (`2`): ambient IBL
- bit 2 (`4`): simple point
- bit 3 (`8`): simple spot
- bit 4 (`16`): sphere/point
- bit 5 (`32`): disk
- `63`: all supported types

Measure the all-lights case, then enable one bit at a time. This separates ambient sampling, light evaluation, and shadow costs without changing the scene.

## Froxel diagnostics

Set `r_volumetricFogDebugMode` to visualize the light-list work reaching each froxel:

- `0`: normal scattering
- `1`: lights listed by NVLC
- `2`: lights that contribute after channel, range, cone, and visibility tests
- `3`: rejected lights
- `4`: shadow-map evaluations
- `5`: NVLC overflow (magenta)
- `6`: selected NVLC Z bin

`r_volumetricFogDebugLightCountScale` maps counts into the heat map; the default `0.0625` reaches the top of the range at 16 lights. Diagnostic modes bypass temporal history so counts are not smeared between froxels.

For a healthy depth-aware list, mode 6 changes with froxel depth and modes 1/3 fall when lights no longer overlap that Z range. Any magenta in mode 5 invalidates performance and image-quality comparisons until the overflow is resolved.

## Acceptance checks

- Compare normal output against a reference capture for directional, spot-gobo, punctual-shadow, sphere-shadow, and disk-shadow scenes.
- Check camera cuts and motion for temporal instability.
- Check very low density, high anisotropy, and near-zero light distance for NaNs or flashes.
- Confirm raw scatter and temporal-history attachments are `R11G11B10_FLOAT`; the integrated attachment must remain `R16G16B16A16_FLOAT` because alpha stores transmittance.
- Use identical resolution, froxel settings, light masks, shadow settings, and camera paths for before/after timing comparisons.
