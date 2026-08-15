/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Macros below are of the form:
// PARAM(NAME, MEMBER_NAME, DEFAULT_VALUE, ...)

AZ_GFX_BOOL_PARAM(Enable, m_enabled, true)                                                                                                  // master toggle; when false the pass hierarchy is disabled
AZ_GFX_COMMON_PARAM(Render::VolumetricFogQuality, FogQuality, m_quality, VolumetricFogQuality::Mid)                                        // froxel resolution preset (see VolumetricFogQuality)
AZ_GFX_COMMON_PARAM(Render::LightingChannelConfiguration, LightingChannels, m_lightingChannelConfig, Render::LightingChannelConfiguration()) // which lighting layers contribute to in-scattering
AZ_GFX_COMMON_PARAM(Render::ShadowFilterMethod, ShadowFilterMethod, m_shadowFilterMethod, ShadowFilterMethod::None)                        // filtering method used when sampling directional-light shadows in the fog pass
AZ_GFX_UINT32_PARAM(FilteringSampleCount, m_filteringSampleCount, 4)                                                                        // requested PCF sample count; remapped to the supported 4, 9, or 16-tap kernel
#include <Atom/Feature/VolumetricFog/VolumetricFogSRGConstants.inl>
#include <Atom/Feature/VolumetricFog/VolumetricFogVolumeParams.inl>
#include <Atom/Feature/VolumetricFog/VolumetricFogVolumeSRGConstants.inl>
