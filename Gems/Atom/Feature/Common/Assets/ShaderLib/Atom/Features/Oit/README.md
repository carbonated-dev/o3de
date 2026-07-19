# Order Independent Transparency Shader Hook

Transparent shaders that use Atom's forward pass SRG can opt into the shared OIT path by including:

```azsl
#include <Atom/Features/Oit/OitTransparent.azsli>
```

After the shader computes its normal sorted-transparency output, call:

```azsl
OitStoreTransparentFragment(uint2(position.xy), depth, alpha, premultipliedColor, transmission);
```

When an OIT method is active, `OitStoreTransparentFragment` stores the fragment through the current method. The caller should then return outputs that leave the bound color target unchanged.

The OIT method is controlled by the global shader option `o_oitMethod`; method settings are stored in `SceneSrg` so every transparent shader can use the same scene-wide state. The forward pass SRG uses compact generic OIT bindings because only one OIT method is active at a time:

- `m_oitFragmentCount`: a screen-sized `RasterizerOrderedTexture2D<uint>` used by MLAB to serialize per-pixel node updates.
- `m_oitFragmentData0` and `m_oitFragmentData1`: `RasterizerOrderedTexture2D<float4>` payloads reused by MLAB, WBOIT, and MBOIT.

The MLAB implementation maps its color and depth/transmission atlases to `m_oitFragmentData0` and `m_oitFragmentData1`.

The WBOIT implementation maps weighted blended results to the same slots:

- `m_oitFragmentData0`: accumulates weighted premultiplied color and weighted opacity.
- `m_oitFragmentData1`: accumulates RGB transmission for tinted and non-tinted transparent fragments.

The MBOIT implementation also maps to `m_oitFragmentData0` and `m_oitFragmentData1`:

- `m_oitFragmentData0`: accumulates optical-depth moments. Four-moment mode uses one screen-height atlas slice; six-moment mode uses a second slice.
- `m_oitFragmentData1`: accumulates premultiplied color and opacity for MBOIT resolve.

The regular sorted transparency path remains the fallback when `o_oitMethod` is `OitMethod::Off`, when `r_oitMethod` is `0`, or when the backend does not expose ordered fragment writes.
