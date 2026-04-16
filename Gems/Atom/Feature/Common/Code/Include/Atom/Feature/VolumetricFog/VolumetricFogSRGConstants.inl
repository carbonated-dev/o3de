/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Macros below are of the form:
// PARAM(NAME, MEMBER_NAME, DEFAULT_VALUE, ...)

AZ_GFX_FLOAT_PARAM(FogStartDistance, m_fogNear, 1.0f)
AZ_GFX_FLOAT_PARAM(FogEndDistance, m_fogFar, 200.0f)
AZ_GFX_UINT32_PARAM(FogSequenceLength, m_sequenceLength, 8)
AZ_GFX_FLOAT_PARAM(TemporalBlendPercentage, m_blendPercentage, 4)
