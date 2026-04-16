/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/Feature/VolumetricFog/VolumetricFogConstants.h>
#include <AzCore/Component/ComponentBus.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Vector3.h>

namespace AZ::Render
{
    class FogVolumeRequests : public AZ::ComponentBus
    {
    public:
        // Auto-gen virtual getter/setter declarations
#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)  \
        virtual ValueType Get##Name() const = 0;                        \
        virtual void Set##Name(ValueType val) = 0;                      \

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>
#include <Atom/Feature/VolumetricFog/FogVolumeParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
    };

    using FogVolumeRequestsBus = AZ::EBus<FogVolumeRequests>;

} // namespace AZ::Render
