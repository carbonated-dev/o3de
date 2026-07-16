/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <PostProcess/RadialBlur/RadialBlurComponentController.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <Atom/RPI.Public/Scene.h>

namespace AZ
{
    namespace Render
    {
        void RadialBlurComponentController::Reflect(ReflectContext* context)
        {
            RadialBlurComponentConfig::Reflect(context);

            if (auto* serializeContext = azrtti_cast<SerializeContext*>(context))
            {
                serializeContext->Class<RadialBlurComponentController>()
                    ->Version(0)
                    ->Field("Configuration", &RadialBlurComponentController::m_configuration);
            }

            if (AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
            {
                behaviorContext->EBus<RadialBlurRequestBus>("RadialBlurRequestBus")
                    ->Attribute(AZ::Script::Attributes::Module, "render")
                    ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)

#define PARAM_EVENT_BUS RadialBlurRequestBus::Events
#include <Atom/Feature/ParamMacros/StartParamBehaviorContext.inl>
#include <Atom/Feature/PostProcess/RadialBlur/RadialBlurParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
#undef PARAM_EVENT_BUS
                    ;
            }
        }

        void RadialBlurComponentController::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC_CE("RadialBlurService"));
        }

        void RadialBlurComponentController::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC_CE("RadialBlurService"));
        }

        void RadialBlurComponentController::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC_CE("PostFXLayerService"));
        }

        RadialBlurComponentController::RadialBlurComponentController(const RadialBlurComponentConfig& config)
            : m_configuration(config)
        {
        }

        void RadialBlurComponentController::Activate(EntityId entityId)
        {
            m_entityId = entityId;

            PostProcessFeatureProcessorInterface* fp =
                RPI::Scene::GetFeatureProcessorForEntity<PostProcessFeatureProcessorInterface>(m_entityId);
            if (fp)
            {
                m_postProcessInterface = fp->GetOrCreateSettingsInterface(m_entityId);
                if (m_postProcessInterface)
                {
                    m_settingsInterface = m_postProcessInterface->GetOrCreateRadialBlurSettingsInterface();
                    OnConfigChanged();
                }
            }
            RadialBlurRequestBus::Handler::BusConnect(m_entityId);
        }

        void RadialBlurComponentController::Deactivate()
        {
            RadialBlurRequestBus::Handler::BusDisconnect(m_entityId);

            if (m_postProcessInterface)
            {
                m_postProcessInterface->RemoveRadialBlurSettingsInterface();
            }

            m_postProcessInterface = nullptr;
            m_settingsInterface = nullptr;
            m_entityId.SetInvalid();
        }

        void RadialBlurComponentController::SetConfiguration(const RadialBlurComponentConfig& config)
        {
            m_configuration = config;
            OnConfigChanged();
        }

        const RadialBlurComponentConfig& RadialBlurComponentController::GetConfiguration() const
        {
            return m_configuration;
        }

        void RadialBlurComponentController::OnConfigChanged()
        {
            if (m_settingsInterface)
            {
                m_configuration.CopySettingsTo(m_settingsInterface);
                m_settingsInterface->OnConfigChanged();
                m_configuration.CopySettingsFrom(m_settingsInterface);
            }
        }

#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)                                  \
        ValueType RadialBlurComponentController::Get##Name() const                                      \
        {                                                                                               \
            return m_configuration.MemberName;                                                          \
        }                                                                                               \
        void RadialBlurComponentController::Set##Name(ValueType val)                                    \
        {                                                                                               \
            if (m_settingsInterface)                                                                    \
            {                                                                                           \
                m_settingsInterface->Set##Name(val);                                                    \
                m_settingsInterface->OnConfigChanged();                                                 \
                m_configuration.MemberName = m_settingsInterface->Get##Name();                          \
            }                                                                                           \
            else                                                                                        \
            {                                                                                           \
                m_configuration.MemberName = val;                                                       \
            }                                                                                           \
        }

#define AZ_GFX_COMMON_OVERRIDE(ValueType, Name, MemberName, OverrideValueType)                          \
        OverrideValueType RadialBlurComponentController::Get##Name##Override() const                    \
        {                                                                                               \
            return m_configuration.MemberName##Override;                                                \
        }                                                                                               \
        void RadialBlurComponentController::Set##Name##Override(OverrideValueType val)                  \
        {                                                                                               \
            m_configuration.MemberName##Override = val;                                                 \
            if (m_settingsInterface)                                                                    \
            {                                                                                           \
                m_settingsInterface->Set##Name##Override(val);                                          \
                m_settingsInterface->OnConfigChanged();                                                 \
                m_configuration.MemberName##Override = m_settingsInterface->Get##Name##Override();      \
            }                                                                                           \
        }

#include <Atom/Feature/ParamMacros/MapAllCommon.inl>
#include <Atom/Feature/PostProcess/RadialBlur/RadialBlurParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
    } // namespace Render
} // namespace AZ
