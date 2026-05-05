/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Macros below are of the form:
// PARAM(NAME, MEMBER_NAME, DEFAULT_VALUE, ...)

AZ_GFX_FLOAT_PARAM(FogStartDistance, m_fogNear, 1.0f)               // near clip of the froxel volume; fog is invisible before this depth
AZ_GFX_FLOAT_PARAM(FogEndDistance, m_fogFar, 200.0f)                // far clip of the froxel volume; slices are distributed logarithmically between Near and Far
AZ_GFX_UINT32_PARAM(FogSequenceLength, m_sequenceLength, 8)         // number of Halton jitter frames used for temporal anti-aliasing (1–16); more frames = smoother TAA but slower convergence after camera cuts
AZ_GFX_FLOAT_PARAM(TemporalBlendPercentage, m_blendPercentage, 4)   // fraction of the current frame blended into history each frame (low = more history weight = smoother but ghostier)
