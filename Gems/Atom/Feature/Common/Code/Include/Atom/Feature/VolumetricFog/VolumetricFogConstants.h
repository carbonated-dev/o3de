/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/Preprocessor/Enum.h>

namespace AZ::Render
{
    AZ_ENUM_CLASS_WITH_UNDERLYING_TYPE(VolumetricFogQuality, uint32_t,
        Low,
        Mid,
        High);

    // Must match value in Scenesrg.azsli
    constexpr uint32_t VolumetricFogMaxSequenceLength = 16;

    // Must match the shader defines in FroxelLocalVolumeCommon.azsli
    AZ_ENUM_CLASS_WITH_UNDERLYING_TYPE(FogVolumeBlendMode, uint32_t,
        Additive,
        Multiply,
        Overwrite,
        Min,
        Max);

    AZ_ENUM_CLASS_WITH_UNDERLYING_TYPE(FogVolumeShape, uint32_t,
        Box,
        Sphere,
        Unknown);

    // Volumetric Fog scene volume constants
    struct VolumetricFogVolumeConstants
    {
#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue) ValueType MemberName;

#include <Atom/Feature/ParamMacros/MapAllCommon.inl>

#undef AZ_GFX_VEC3_PARAM
#define AZ_GFX_VEC3_PARAM(Name, MemberName, DefaultValue) AZStd::array<float, 3> MemberName;

#include <Atom/Feature/VolumetricFog/VolumetricFogVolumeSRGConstants.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

    };
}
