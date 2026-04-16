/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/Feature/VolumetricFog/VolumetricFogConstants.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Vector3.h>
#include <Atom/RPI.Reflect/Image/StreamingImageAsset.h>

namespace AZ::Render
{
    class FogVolumeComponentConfig final
        : public AZ::ComponentConfig
    {
        friend class EditorFogVolumeComponent;

    public:
        AZ_CLASS_ALLOCATOR(FogVolumeComponentConfig, AZ::SystemAllocator)
        AZ_RTTI(FogVolumeComponentConfig, "{3A7D9F2E-B1C4-4E8A-95D2-6F0B3C7A1E4D}", AZ::ComponentConfig);

        static void Reflect(AZ::ReflectContext* context);

        // Auto-generate getters/setters
#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)  \
        ValueType Get##Name() const { return MemberName; }              \
        void Set##Name(ValueType val) { MemberName = val; }             \

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>
#include <Atom/Feature/VolumetricFog/FogVolumeParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

    private:
        // Auto-generate members
#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)  \
        ValueType MemberName = DefaultValue;                            \

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>
#include <Atom/Feature/VolumetricFog/FogVolumeParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
    };

} // namespace AZ::Render
