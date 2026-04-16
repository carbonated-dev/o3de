/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AtomLyIntegration/CommonFeatures/VolumetricFog/FogVolumeComponentConfig.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace AZ::Render
{
    void FogVolumeComponentConfig::Reflect(AZ::ReflectContext* context)
    {
        if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
        {
            sc->Class<FogVolumeComponentConfig, AZ::ComponentConfig>()
                ->Version(1)
#define SERIALIZE_CLASS FogVolumeComponentConfig
#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)  \
                ->Field(#Name, &SERIALIZE_CLASS::MemberName)            \

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>
#include <Atom/Feature/VolumetricFog/FogVolumeParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
#undef SERIALIZE_CLASS
                ;
        }
    }

} // namespace AZ::Render
