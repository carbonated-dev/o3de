/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Macros below are of the form:
// PARAM(NAME, MEMBER_NAME, DEFAULT_VALUE, ...)

AZ_GFX_BOOL_PARAM(Enabled, m_enabled, false)
AZ_GFX_ANY_PARAM_BOOL_OVERRIDE(bool, Enabled, m_enabled)

// Center of the radial blur in normalized screen coordinates.
AZ_GFX_VEC2_PARAM(Center, m_center, RadialBlur::DefaultCenter)
AZ_GFX_FLOAT_PARAM_FLOAT_OVERRIDE(AZ::Vector2, Center, m_center)

// Amount of blur applied outward from the center.
AZ_GFX_FLOAT_PARAM(Amount, m_amount, RadialBlur::DefaultAmount)
AZ_GFX_FLOAT_PARAM_FLOAT_OVERRIDE(float, Amount, m_amount)

// Unaffected distance from the center as a percentage of the farthest viewport corner.
AZ_GFX_FLOAT_PARAM(InnerRadius, m_innerRadius, RadialBlur::DefaultInnerRadius)
AZ_GFX_FLOAT_PARAM_FLOAT_OVERRIDE(float, InnerRadius, m_innerRadius)

// Number of samples used by the blur.
AZ_GFX_UINT32_PARAM(SampleCount, m_sampleCount, RadialBlur::DefaultSampleCount)
AZ_GFX_ANY_PARAM_BOOL_OVERRIDE(uint32_t, SampleCount, m_sampleCount)
