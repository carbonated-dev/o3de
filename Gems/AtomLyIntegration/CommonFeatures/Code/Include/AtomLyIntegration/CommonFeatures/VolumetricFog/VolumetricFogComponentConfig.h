/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Vector3.h>
#include <Atom/Feature/VolumetricFog/VolumetricFogConstants.h>
#include <Atom/Feature/LightingChannel/LightingChannelConfiguration.h>
#include <Atom/RPI.Reflect/Image/StreamingImageAsset.h>

namespace AZ::Render
{
    class VolumetricFogFeatureProcessorInterface;

    //! Serializable config for the VolumetricFog component
    //! All fields are auto-generated from VolumetricFogParams.inl macros.
    class VolumetricFogComponentConfig final
        : public AZ::ComponentConfig
    {
        friend class EditorVolumetricFogComponent;

    public:
        AZ_CLASS_ALLOCATOR(VolumetricFogComponentConfig, SystemAllocator)
        AZ_RTTI(AZ::Render::VolumetricFogComponentConfig, "{FAEC22F1-91EB-4304-9779-5C6239B236E5}", AZ::ComponentConfig);

        static void Reflect(AZ::ReflectContext* context);

        //! Pulls current values from a live feature processor into this config.
        void CopySettingsFrom(VolumetricFogFeatureProcessorInterface* settings);
        //! Pushes config values into a live feature processor.
        void CopySettingsTo(VolumetricFogFeatureProcessorInterface* settings);

        // Generate Get / Set methods
#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)                                  \
        virtual ValueType Get##Name() const { return MemberName; }                                      \
        virtual void Set##Name(ValueType val) { MemberName = val; }                                     \

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>
#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

        private:

            // SRG constants
#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)                                  \
        ValueType MemberName = DefaultValue;                                                            \

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>
#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
    };
} // namespace AZ::Render
