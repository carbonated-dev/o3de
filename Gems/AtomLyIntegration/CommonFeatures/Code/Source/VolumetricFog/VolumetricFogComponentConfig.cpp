/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AtomLyIntegration/CommonFeatures/VolumetricFog/VolumetricFogComponentConfig.h>
#include <AzCore/Asset/AssetSerializer.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <Atom/Feature/VolumetricFog/VolumetricFogFeatureProcessorInterface.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>

namespace AZ::Render
{
    void VolumetricFogComponentConfig::Reflect(ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<VolumetricFogComponentConfig, ComponentConfig>()->Version(0)
            // Auto-gen serialize context code...
#define SERIALIZE_CLASS VolumetricFogComponentConfig
#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)      \
            ->Field(#Name, &SERIALIZE_CLASS::MemberName)                    \

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>
#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
#undef SERIALIZE_CLASS
                ;
        }
    }   

    void VolumetricFogComponentConfig::CopySettingsFrom(VolumetricFogFeatureProcessorInterface * settings)
    {
        if (!settings)
        {
            return;
        }

#define COPY_SOURCE settings
#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)      \
        MemberName = COPY_SOURCE->Get##Name();                              \

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>
#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
#undef COPY_SOURCE
    }

    void VolumetricFogComponentConfig::CopySettingsTo(VolumetricFogFeatureProcessorInterface * settings)
    {
        if (!settings)
        {
            return;
        }

#define COPY_TARGET settings
#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)      \
        COPY_TARGET->Set##Name(MemberName);                                 \

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>
#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
#undef COPY_TARGET
    }
} // AZ::Render
