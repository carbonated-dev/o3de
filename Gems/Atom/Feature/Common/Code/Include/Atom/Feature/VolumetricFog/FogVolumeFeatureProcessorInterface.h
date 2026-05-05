/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/Feature/VolumetricFog/VolumetricFogConstants.h>
#include <Atom/RPI.Public/FeatureProcessor.h>
#include <Atom/RPI.Reflect/Image/StreamingImageAsset.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>

namespace AZ::Render
{
    // opaque handle returned by AcquireVolume, invalidated after ReleaseVolume
    using FogVolumeHandle = RHI::Handle<uint16_t, class FogVolumeTag>; 

    //! Manages the array of local fog volumes (box/sphere shapes that override fog
    //! parameters inside them) and their GPU buffer.
    class FogVolumeFeatureProcessorInterface
        : public RPI::FeatureProcessor
    {
    public:
        AZ_RTTI(AZ::Render::FogVolumeFeatureProcessorInterface, "{5F8B3C7A-2D4E-4F9B-A6C1-8E0D5B3F7A2C}");

        using VolumeHandle = FogVolumeHandle;

        //! Allocates a new GPU-backed fog volume slot; returns an invalid handle if the budget is full.
        virtual VolumeHandle AcquireVolume() = 0;
        //! Frees the slot and zeroes the handle.
        virtual bool ReleaseVolume(VolumeHandle& handle) = 0;
        //! Sets the world transform used to position the volume proxy mesh in the froxel injection pass.
        virtual void SetVolumeTransform(VolumeHandle handle, const AZ::Transform& transform) = 0;
        //! Sets the half-extents of the box volume in world units; ignored for sphere shapes.
        virtual void SetVolumeExtents(VolumeHandle handle, const AZ::Vector3& halfExtents) = 0;

        // Auto-gen per-property volume setters: SetVolume<Name>(handle, val)
#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)  \
        virtual void SetVolume##Name(VolumeHandle handle, ValueType val) = 0;

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>
#include <Atom/Feature/VolumetricFog/FogVolumeParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
    };
} // namespace AZ::Render
