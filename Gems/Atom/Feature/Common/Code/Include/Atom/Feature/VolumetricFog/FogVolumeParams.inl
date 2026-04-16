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

AZ_GFX_COMMON_PARAM(FogVolumeShape,     Shape,               m_shape,               FogVolumeShape::Unknown)
AZ_GFX_COMMON_PARAM(FogVolumeBlendMode, BlendMode, m_blendMode, FogVolumeBlendMode::Additive)
AZ_GFX_VEC3_PARAM(EdgeFade, m_edgeFade, AZ::Vector3(0.1f))
AZ_GFX_UINT32_PARAM(Priority, m_priority, 0)
#include <Atom/Feature/VolumetricFog/VolumetricFogVolumeParams.inl>
#include <Atom/Feature/VolumetricFog/VolumetricFogVolumeSRGConstants.inl>
