/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AtomLyIntegration/CommonFeatures/VolumetricFog/FogVolumeComponentConfig.h>
#include <AtomLyIntegration/CommonFeatures/VolumetricFog/FogVolumeBus.h>
#include <Atom/Feature/VolumetricFog/FogVolumeFeatureProcessorInterface.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TransformBus.h>
#include <LmbrCentral/Shape/ShapeComponentBus.h>

namespace AZ::Render
{
    class FogVolumeComponentController final
        : public FogVolumeRequestsBus::Handler
        , private AZ::TransformNotificationBus::Handler
        , private LmbrCentral::ShapeComponentNotificationsBus::Handler
    {
    public:
        friend class EditorFogVolumeComponent;

        AZ_TYPE_INFO(FogVolumeComponentController, "{8C2F5A3D-E7B1-4D9F-A2E8-5C0B7F3D9A1C}");
        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        FogVolumeComponentController() = default;
        explicit FogVolumeComponentController(const FogVolumeComponentConfig& config);

        void Activate(AZ::EntityId entityId);
        void Deactivate();
        void SetConfiguration(const FogVolumeComponentConfig& config);
        const FogVolumeComponentConfig& GetConfiguration() const;

        // Auto-gen FogVolumeRequestsBus::Handler override declarations
#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)  \
        ValueType Get##Name() const override;                           \
        void Set##Name(ValueType val) override;                         \

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>
#include <Atom/Feature/VolumetricFog/FogVolumeParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

    private:
        AZ_DISABLE_COPY(FogVolumeComponentController);

        // TransformNotificationBus::Handler
        void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;

        // ShapeComponentNotificationsBus::Handler
        void OnShapeChanged(LmbrCentral::ShapeComponentNotifications::ShapeChangeReasons reason) override;

        void OnConfigChanged();
        void PushTransform();
        void PushShape();
        void PushAllToFeatureProcessor();

        FogVolumeFeatureProcessorInterface*              m_featureProcessor = nullptr;
        FogVolumeFeatureProcessorInterface::VolumeHandle m_handle;
        FogVolumeComponentConfig                         m_config;
        AZ::EntityId                                     m_entityId;
    };

} // namespace AZ::Render
