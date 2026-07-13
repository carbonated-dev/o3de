/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/Asset/AssetCommon.h>
#include <Atom/Feature/VolumetricFog/VolumetricFogConstants.h>
#include <Atom/Feature/LightingChannel/LightingChannelConfiguration.h>
#include <Atom/RPI.Reflect/Image/StreamingImageAsset.h>

namespace AZ::Render
{
    //! POD settings bag that mirrors the VolumetricFogParams
    //! The feature processor stores one of these and exposes it via GetSettings().
    class VolumetricFogSettings final
    {
    public:
        AZ_RTTI(AZ::Render::VolumetricFogSettings, "{501C7191-7EE8-4101-9A93-EB918C99D9EE}");
        AZ_CLASS_ALLOCATOR(VolumetricFogSettings, SystemAllocator);

        VolumetricFogSettings() = default;
        ~VolumetricFogSettings() = default;

        // Generate members...
#include <Atom/Feature/ParamMacros/StartParamMembers.inl>
#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
    };

}
