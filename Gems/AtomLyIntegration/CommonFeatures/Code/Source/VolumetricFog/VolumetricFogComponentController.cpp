/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <VolumetricFog/VolumetricFogComponentController.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <Atom/RPI.Public/Scene.h>

namespace AZ::Render
{
    void VolumetricFogComponentController::Reflect(ReflectContext* context)
    {
        VolumetricFogComponentConfig::Reflect(context);

        if (auto* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext->Class<VolumetricFogComponentController>()
                ->Version(1)
                ->Field("Configuration", &VolumetricFogComponentController::m_configuration);
        }

        if (auto* behaviorContext = azrtti_cast<BehaviorContext*>(context))
        {
            behaviorContext->EBus<VolumetricFogRequestsBus>("VolumetricFogRequestsBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Category, "Render")
                ->Attribute(AZ::Script::Attributes::Module, "Render")

// Auto-gen behavior context...
#define PARAM_EVENT_BUS VolumetricFogRequestsBus::Events
#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)                                  \
        ->Event("Set" #Name, &PARAM_EVENT_BUS::Set##Name)                                               \
        ->Event("Get" #Name, &PARAM_EVENT_BUS::Get##Name)                                               \
        ->VirtualProperty(#Name, "Get" #Name, "Set" #Name)                                              \

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>
// Asset params are not scriptable via behavior context
#undef AZ_GFX_TEXTURE_ASSET_PARAM
#define AZ_GFX_TEXTURE_ASSET_PARAM(Name, MemberName, DefaultValue)
#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
#undef PARAM_EVENT_BUS
                ;

        }
    }

    void VolumetricFogComponentController::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("VolumetricFogService"));
    }

    void VolumetricFogComponentController::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("NonUniformScaleService"));
    }

    void VolumetricFogComponentController::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC("TransformService"));
    }

    VolumetricFogComponentController::VolumetricFogComponentController(const VolumetricFogComponentConfig& config)
        : m_configuration(config)
    {
    }

    void VolumetricFogComponentController::OnConfigChanged()
    {
        if (m_featureProcessorInterface)
        {
            // Set SRG constants
            m_configuration.CopySettingsTo(m_featureProcessorInterface);
        }
    }

    void VolumetricFogComponentController::Activate(EntityId entityId)
    {
        VolumetricFogRequestsBus::Handler::BusConnect(entityId);
        m_featureProcessorInterface = RPI::Scene::GetFeatureProcessorForEntity<VolumetricFogFeatureProcessorInterface>(entityId);

        if (m_featureProcessorInterface)
        {
            m_entityId = entityId;
            OnConfigChanged();
        }
    }

    void VolumetricFogComponentController::Deactivate()
    {
        VolumetricFogRequestsBus::Handler::BusDisconnect();

        if (m_featureProcessorInterface)
        {
            m_featureProcessorInterface->SetEnable(false);
            m_featureProcessorInterface = nullptr;
        }
    }

    void VolumetricFogComponentController::SetConfiguration(const VolumetricFogComponentConfig& config)
    {
        m_configuration = config;
    }

    const VolumetricFogComponentConfig& VolumetricFogComponentController::GetConfiguration() const
    {
        return m_configuration;
    }

    // Auto generated getter/setter functions
    // The setter functions will set the values on the Atom settings class, then get the value back
    // from the settings class to set the local configuration. This is in case the settings class
    // applies some custom logic that results in the set value being different from the input
#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)                              \
    ValueType VolumetricFogComponentController::Get##Name() const                                   \
    {                                                                                               \
        return m_configuration.Get##Name();                                                         \
    }                                                                                               \
    void VolumetricFogComponentController::Set##Name(ValueType val)                                 \
    {                                                                                               \
        if (m_featureProcessorInterface)                                                            \
        {                                                                                           \
            m_configuration.Set##Name( val );                                                       \
            m_featureProcessorInterface->Set##Name(val);                                            \
        }                                                                                           \
        else                                                                                        \
        {                                                                                           \
            m_configuration.Set##Name( val );                                                       \
        }                                                                                           \
    }                                                                                               \

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>
#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
} // namespace AZ::Render
