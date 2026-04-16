/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Macros below are of the form:
// PARAM(NAME, MEMBER_NAME, DEFAULT_VALUE, ...)

AZ_GFX_VEC3_PARAM(FogAlbedo, m_fogAlbedo, Vector3(0.85, 0.90, 1.0))
AZ_GFX_FLOAT_PARAM(PhaseG, m_phaseG, 0.6f)
AZ_GFX_FLOAT_PARAM(AmbientIntensity, m_ambientIntensity, 0.8f)
AZ_GFX_FLOAT_PARAM(FogDensity, m_fogDensity, 0.17f)
AZ_GFX_FLOAT_PARAM(FogBaseHeight, m_fogBaseHeight, 0.0f)
AZ_GFX_FLOAT_PARAM(FogHeightFalloff, m_fogHeightFalloff, 0.22f)
AZ_GFX_VEC3_PARAM(FogNoiseWindVelocity, m_noiseWindVelocity, Vector3(0.7f, 0.0f, 0.0f))
AZ_GFX_FLOAT_PARAM(FogNoiseScale, m_noiseScale, 0.1f)
AZ_GFX_VEC3_PARAM(FogEmissiveColor, m_emissiveColor, Vector3(0.0f, 0.0f, 0.0f))
AZ_GFX_FLOAT_PARAM(FogEmissiveIntensity, m_emissiveIntensity, 0.0f)
