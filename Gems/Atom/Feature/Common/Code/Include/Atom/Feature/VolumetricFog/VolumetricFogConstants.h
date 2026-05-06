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
    //! Controls froxel grid resolution. Low is coarser/faster, High is finer/slower.
    //! Match VolumetricFogUtils::ToFroxelSize() for the actual tile sizes.
    AZ_ENUM_CLASS_WITH_UNDERLYING_TYPE(VolumetricFogQuality, uint32_t,
        Low,
        Mid,
        High);

    constexpr uint32_t VolumetricFogMaxSequenceLength = 16; // max length of the Halton jitter sequence; must match HLSL constant in SceneSrg.azsli

    //! How a local fog volume combines its density with underlying fog.
    //! Must match the shader defines in FroxelLocalVolumeCommon.azsli.
    AZ_ENUM_CLASS_WITH_UNDERLYING_TYPE(FogVolumeBlendMode, uint32_t,
        Additive,  // density is added on top of existing fog
        Multiply,  // density is multiplied with existing fog
        Overwrite, // density replaces existing fog entirely
        Min,       // takes the lesser of the two densities
        Max);      // takes the greater of the two densities

    //! Shape of the proxy mesh used to rasterize the volume into the froxel grid.
    AZ_ENUM_CLASS_WITH_UNDERLYING_TYPE(FogVolumeShape, uint32_t,
        Box,
        Sphere,
        Unknown);

    //! GPU-layout struct uploaded to the scene SRG for per-volume shader constants;
    //! layout must stay in sync with FroxelLocalVolumeCommon.azsli.
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
