/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Macros below are of the form:
// PARAM(NAME, MEMBER_NAME, DEFAULT_VALUE, ...)

AZ_GFX_BOOL_PARAM(Enable, m_enabled, true)
AZ_GFX_COMMON_PARAM(Render::VolumetricFogQuality, FogQuality, m_quality, VolumetricFogQuality::Mid)
AZ_GFX_COMMON_PARAM(Render::LightingChannelConfiguration, LightingChannels, m_lightingChannelConfig, Render::LightingChannelConfiguration())
#include <Atom/Feature/VolumetricFog/VolumetricFogSRGConstants.inl>
#include <Atom/Feature/VolumetricFog/VolumetricFogVolumeParams.inl>
#include <Atom/Feature/VolumetricFog/VolumetricFogVolumeSRGConstants.inl>
