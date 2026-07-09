/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Macros below are of the form:
// PARAM(NAME, MEMBER_NAME, DEFAULT_VALUE, ...)

AZ_GFX_VEC3_PARAM(FogAlbedo, m_fogAlbedo, Vector3(0.85, 0.90, 1.0))                            // single-scattering albedo (RGB); 1.0 = pure white scatter, 0.0 = pure absorption
AZ_GFX_FLOAT_PARAM(PhaseG, m_phaseG, 0.6f)                                                     // Henyey-Greenstein asymmetry parameter (-1 = full back-scatter, 0 = isotropic, 1 = full forward-scatter)
AZ_GFX_FLOAT_PARAM(AmbientIntensity, m_ambientIntensity, 0.8f)                                 // scale applied to IBL irradiance sampled from the diffuse cubemap
AZ_GFX_FLOAT_PARAM(FogDensity, m_fogDensity, 0.17f)                                            // extinction coefficient sigma_t; linearly scales scattering and absorption
AZ_GFX_FLOAT_PARAM(FogBaseHeight, m_fogBaseHeight, 0.0f)                                       // world-space Z below which height fog reaches full density
AZ_GFX_FLOAT_PARAM(FogHeightFalloff, m_fogHeightFalloff, 0.22f)                                // exponential falloff rate above FogBaseHeight (higher = thinner fog at altitude)
AZ_GFX_VEC3_PARAM(FogNoiseWindVelocity, m_noiseWindVelocity, Vector3(0.7f, 0.0f, 0.0f))       // world-space scroll velocity applied to the 3D noise texture each frame
AZ_GFX_FLOAT_PARAM(FogNoiseScale, m_noiseScale, 0.1f)                                          // spatial frequency of the 3D noise in world units
AZ_GFX_VEC3_PARAM(FogEmissiveColor, m_emissiveColor, Vector3(0.0f, 0.0f, 0.0f))               // optional self-emission color added before Beer-Lambert integration
AZ_GFX_FLOAT_PARAM(FogEmissiveIntensity, m_emissiveIntensity, 0.0f)                            // intensity multiplier for self-emission; zero disables the emissive term
