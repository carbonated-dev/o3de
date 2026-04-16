/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once
#include <AzCore/Component/ComponentBus.h>
#include <AtomLyIntegration/CommonFeatures/VolumetricFog/VolumetricFogComponentConfig.h>

namespace AZ::Render
{
    //! EBus request interface for the VolumetricFog component
    //! Use this to read/write global fog settings from Lua or other components.
    class VolumetricFogRequests
        : public ComponentBus
    {
    public:
        AZ_RTTI(AZ::Render::VolumetricFogRequests, "{3F4474E5-D618-4BBA-B4DC-E0E46B699D2C}");

        /// Overrides the default AZ::EBusTraits handler policy to allow one listener only.
        static const EBusHandlerPolicy HandlerPolicy = EBusHandlerPolicy::Single;
        virtual ~VolumetricFogRequests() {}

        // Auto-gen virtual getter and setter functions - matching the interface methods    
#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)                                  \
        virtual ValueType Get##Name() const = 0;                                                        \
        virtual void Set##Name(ValueType val) = 0;                                                      \

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>
#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
    };

    typedef AZ::EBus<VolumetricFogRequests> VolumetricFogRequestsBus;
}
