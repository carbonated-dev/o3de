/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/Feature/PostProcess/PostProcessFeatureProcessorInterface.h>
#include <Atom/Feature/PostProcess/PostProcessSettingsInterface.h>
#include <Atom/Feature/PostProcess/RadialBlur/RadialBlurSettingsInterface.h>
#include <AtomLyIntegration/CommonFeatures/PostProcess/RadialBlur/RadialBlurBus.h>
#include <AtomLyIntegration/CommonFeatures/PostProcess/RadialBlur/RadialBlurComponentConfig.h>

namespace AZ
{
    namespace Render
    {
        //! Controller that connects a radial blur component configuration to Atom post-process settings.
        class RadialBlurComponentController final : public RadialBlurRequestBus::Handler
        {
            friend class EditorRadialBlurComponent;

        public:
            AZ_TYPE_INFO(AZ::Render::RadialBlurComponentController, "{A4EBC8A9-949E-400B-94BC-53E5AA84EC9A}");
            AZ_CLASS_ALLOCATOR(RadialBlurComponentController, AZ::SystemAllocator);

            static void Reflect(ReflectContext* context);
            static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
            static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
            static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

            RadialBlurComponentController() = default;
            RadialBlurComponentController(const RadialBlurComponentConfig& config);
            ~RadialBlurComponentController() = default;

            void Activate(EntityId entityId);
            void Deactivate();
            //! Replace the controller configuration and apply it to Atom settings.
            void SetConfiguration(const RadialBlurComponentConfig& config);
            //! Return the current controller configuration.
            const RadialBlurComponentConfig& GetConfiguration() const;
            //! Apply the current configuration to Atom settings.
            void OnConfigChanged();

            // Generate getters and setters.
#include <Atom/Feature/ParamMacros/StartParamFunctionsOverride.inl>
#include <Atom/Feature/PostProcess/RadialBlur/RadialBlurParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

        private:
            AZ_DISABLE_COPY(RadialBlurComponentController);

            EntityId m_entityId;
            PostProcessSettingsInterface* m_postProcessInterface = nullptr;
            RadialBlurSettingsInterface* m_settingsInterface = nullptr;
            RadialBlurComponentConfig m_configuration;
        };
    } // namespace Render
} // namespace AZ
