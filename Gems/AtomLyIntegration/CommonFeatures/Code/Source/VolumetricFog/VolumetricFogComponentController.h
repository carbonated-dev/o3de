/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Component/Component.h>
#include <Atom/Feature/VolumetricFog/VolumetricFogFeatureProcessorInterface.h>
#include <AtomLyIntegration/CommonFeatures/VolumetricFog/VolumetricFogBus.h>

namespace AZ::Render
{
    //! Handles VolumetricFogRequestsBus and forwards property changes to VolumetricFogFeatureProcessorInterface
    //! One instance per active VolumetricFog component.
    class VolumetricFogComponentController final
        : public VolumetricFogRequestsBus::Handler
    {
    public:
        friend class EditorVolumetricFogComponent;

        AZ_TYPE_INFO(AZ::Render::VolumetricFogComponentController, "{79545B74-F14F-4AAA-8861-DBDF67802EEA}");
        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        VolumetricFogComponentController() = default;
        VolumetricFogComponentController(const VolumetricFogComponentConfig& config);

        //! Connects to the bus and acquires VolumetricFogFeatureProcessorInterface from the scene.
        void Activate(EntityId entityId);
        //! Disconnects from the bus and releases the feature processor pointer.
        void Deactivate();
        void SetConfiguration(const VolumetricFogComponentConfig& config);
        const VolumetricFogComponentConfig& GetConfiguration() const;

         // Getter / Setter methods override declarations
         // Generate Get / Set methods

#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)                                  \
        ValueType Get##Name() const override;                                                           \
        void Set##Name(ValueType val) override;                                                         \

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>
#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

    private:
        AZ_DISABLE_COPY(VolumetricFogComponentController);

        //! Pushes all config values to the feature processor; called after any setter.
        void OnConfigChanged();

        VolumetricFogFeatureProcessorInterface* m_featureProcessorInterface = nullptr; // non-owning pointer; valid only between Activate / Deactivate
        VolumetricFogComponentConfig m_configuration;
        EntityId m_entityId;
    };
} // namespace AZ::Render
