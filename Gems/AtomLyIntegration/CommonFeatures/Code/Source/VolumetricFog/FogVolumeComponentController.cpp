/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <VolumetricFog/FogVolumeComponentController.h>

#include <Atom/RPI.Public/Scene.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <LmbrCentral/Shape/BoxShapeComponentBus.h>
#include <LmbrCentral/Shape/SphereShapeComponentBus.h>

namespace AZ::Render
{
    void FogVolumeComponentController::Reflect(AZ::ReflectContext* context)
    {
        FogVolumeComponentConfig::Reflect(context);

        if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
        {
            sc->Class<FogVolumeComponentController>()
                ->Version(1)
                ->Field("Configuration", &FogVolumeComponentController::m_config);
        }

        if (auto* bc = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            bc->EBus<FogVolumeRequestsBus>("FogVolumeRequestsBus")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Category, "Render")
                ->Attribute(AZ::Script::Attributes::Module, "Render")
// Auto-gen behavior context events
#define PARAM_EVENT_BUS FogVolumeRequestsBus::Events
#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)  \
                ->Event("Set" #Name, &PARAM_EVENT_BUS::Set##Name)       \
                ->Event("Get" #Name, &PARAM_EVENT_BUS::Get##Name)       \
                ->VirtualProperty(#Name, "Get" #Name, "Set" #Name)      \

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>
#include <Atom/Feature/VolumetricFog/FogVolumeParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
#undef PARAM_EVENT_BUS
                ;
        }
    }

    void FogVolumeComponentController::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("FogVolumeService"));
    }

    void FogVolumeComponentController::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("FogVolumeService"));
        incompatible.push_back(AZ_CRC_CE("NonUniformScaleService"));
    }

    void FogVolumeComponentController::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    void FogVolumeComponentController::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        // The shape component provides the volume bounds and editor visualization.
        // It's listed as dependent (not required) so the component can activate even
        // before a shape is added; PushShape() will just early-out if none is present.
        dependent.push_back(AZ_CRC_CE("ShapeService"));
    }

    FogVolumeComponentController::FogVolumeComponentController(const FogVolumeComponentConfig& config)
        : m_config(config)
    {
    }

    void FogVolumeComponentController::Activate(AZ::EntityId entityId)
    {
        m_entityId = entityId;

        m_featureProcessor =
            RPI::Scene::GetFeatureProcessorForEntity<FogVolumeFeatureProcessorInterface>(entityId);

        if (!m_featureProcessor)
        {
            AZ_Warning("FogVolumeComponentController", false,
                "FogVolumeFeatureProcessor not found for entity %s.",
                entityId.ToString().c_str());
            return;
        }

        m_handle = m_featureProcessor->AcquireVolume();

        AZ::TransformNotificationBus::Handler::BusConnect(entityId);
        LmbrCentral::ShapeComponentNotificationsBus::Handler::BusConnect(entityId);
        FogVolumeRequestsBus::Handler::BusConnect(entityId);

        PushAllToFeatureProcessor();
    }

    void FogVolumeComponentController::Deactivate()
    {
        FogVolumeRequestsBus::Handler::BusDisconnect();
        LmbrCentral::ShapeComponentNotificationsBus::Handler::BusDisconnect();
        AZ::TransformNotificationBus::Handler::BusDisconnect();

        if (m_featureProcessor)
        {
            m_featureProcessor->ReleaseVolume(m_handle);
            m_featureProcessor = nullptr;
        }
    }

    void FogVolumeComponentController::SetConfiguration(const FogVolumeComponentConfig& config)
    {
        m_config = config;
        OnConfigChanged();
    }

    const FogVolumeComponentConfig& FogVolumeComponentController::GetConfiguration() const
    {
        return m_config;
    }

    // -------------------------------------------------------------------------
    // Bus handlers
    // -------------------------------------------------------------------------

    void FogVolumeComponentController::OnTransformChanged(
        [[maybe_unused]] const AZ::Transform& local, const AZ::Transform& world)
    {
        if (m_featureProcessor)
        {
            m_featureProcessor->SetVolumeTransform(m_handle, world);
            PushShape(); // extents may depend on entity scale
        }
    }

    void FogVolumeComponentController::OnShapeChanged(
        [[maybe_unused]] LmbrCentral::ShapeComponentNotifications::ShapeChangeReasons reason)
    {
        PushShape();
    }

    // -------------------------------------------------------------------------
    // Config propagation
    // -------------------------------------------------------------------------

    void FogVolumeComponentController::OnConfigChanged()
    {
        if (!m_featureProcessor || !m_handle.IsValid())
        {
            return;
        }
#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)      \
        m_featureProcessor->SetVolume##Name(m_handle, m_config.Get##Name()); \

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>
#include <Atom/Feature/VolumetricFog/FogVolumeParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
    }

    void FogVolumeComponentController::PushTransform()
    {
        if (!m_featureProcessor || !m_handle.IsValid())
        {
            return;
        }
        AZ::Transform worldTM = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(worldTM, m_entityId, &AZ::TransformBus::Events::GetWorldTM);
        m_featureProcessor->SetVolumeTransform(m_handle, worldTM);
    }

    void FogVolumeComponentController::PushShape()
    {
        if (!m_featureProcessor || !m_handle.IsValid())
        {
            return;
        }       

        switch (m_config.GetShape())
        {
        case FogVolumeShape::Box:
            {
                AZ::Vector3 boxDimensions = AZ::Vector3(1.0f);
                LmbrCentral::BoxShapeComponentRequestsBus::EventResult(
                    boxDimensions, m_entityId, &LmbrCentral::BoxShapeComponentRequestsBus::Events::GetBoxDimensions);
                m_featureProcessor->SetVolumeExtents(m_handle, boxDimensions * 0.5f);
            }
            break;
        case FogVolumeShape::Sphere:
            {
                float radius = 1.0f;
                LmbrCentral::SphereShapeComponentRequestsBus::EventResult(
                    radius, m_entityId, &LmbrCentral::SphereShapeComponentRequestsBus::Events::GetRadius);

                // For the shader, sphere half-extents are (radius, radius, radius); only X is used for edge fade.
                m_featureProcessor->SetVolumeExtents(m_handle, AZ::Vector3(radius));
            }
        break;
        default:
            break;
        }
    }

    void FogVolumeComponentController::PushAllToFeatureProcessor()
    {
        PushTransform();
        PushShape();
        OnConfigChanged();
    }

    // -------------------------------------------------------------------------
    // Auto-gen FogVolumeRequestsBus getter/setter implementations.
    // Each setter updates the config and calls SetVolume<Name>(m_handle, val)
    // directly on the FP one property, one call, no full flush needed.
    // -------------------------------------------------------------------------
#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)      \
    ValueType FogVolumeComponentController::Get##Name() const               \
    {                                                                       \
        return m_config.Get##Name();                                        \
    }                                                                       \
    void FogVolumeComponentController::Set##Name(ValueType val)             \
    {                                                                       \
        m_config.Set##Name(val);                                            \
        if (m_featureProcessor && m_handle.IsValid())                       \
        {                                                                   \
            m_featureProcessor->SetVolume##Name(m_handle, val);             \
        }                                                                   \
    }                                                                       \

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>
#include <Atom/Feature/VolumetricFog/FogVolumeParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

} // namespace AZ::Render
