/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Macros below are of the form:
// PARAM(NAME, MEMBER_NAME, DEFAULT_VALUE, ...)
// Requires FogVolumeBlendMode from VolumetricFogConstants.h to be in scope.

AZ_GFX_COMMON_PARAM(FogVolumeShape,     Shape,     m_shape,     FogVolumeShape::Unknown)         // Box or Sphere (determines which proxy mesh is rasterized)
AZ_GFX_COMMON_PARAM(FogVolumeBlendMode, BlendMode, m_blendMode, FogVolumeBlendMode::Additive)    // how this volume's density composites over the global fog
AZ_GFX_VEC3_PARAM(EdgeFade, m_edgeFade, AZ::Vector3(0.1f))                                       // per-axis distance (normalized 0–1) for a smooth fade at each face boundary
AZ_GFX_UINT32_PARAM(Priority, m_priority, 0)                                                      // sort key; higher priority volumes are injected last (draw-order within the same BlendMode batch)
#include <Atom/Feature/VolumetricFog/VolumetricFogVolumeParams.inl>
#include <Atom/Feature/VolumetricFog/VolumetricFogVolumeSRGConstants.inl>
