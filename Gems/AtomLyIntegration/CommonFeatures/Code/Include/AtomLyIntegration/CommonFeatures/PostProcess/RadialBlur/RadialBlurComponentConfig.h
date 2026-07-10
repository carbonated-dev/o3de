/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/Feature/PostProcess/RadialBlur/RadialBlurSettingsInterface.h>
#include <AzCore/Component/Component.h>

namespace AZ
{
    namespace Render
    {
        //! Serializable configuration for the radial blur component.
        class RadialBlurComponentConfig final : public ComponentConfig
        {
        public:
            AZ_RTTI(AZ::Render::RadialBlurComponentConfig, "{D48FBD98-2412-4CF4-8872-397FDFFC6927}", AZ::ComponentConfig);

            //! Reflect radial blur component configuration for serialization and editing.
            static void Reflect(ReflectContext* context);

            //! Generated members for all radial blur parameters.
#include <Atom/Feature/ParamMacros/StartParamMembers.inl>
#include <Atom/Feature/PostProcess/RadialBlur/RadialBlurParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

            //! Generated getters and setters for all radial blur parameters.
#include <Atom/Feature/ParamMacros/StartParamFunctions.inl>
#include <Atom/Feature/PostProcess/RadialBlur/RadialBlurParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

            //! Copy radial blur values from Atom settings into this component configuration.
            void CopySettingsFrom(RadialBlurSettingsInterface* settings);
            //! Copy radial blur values from this component configuration into Atom settings.
            void CopySettingsTo(RadialBlurSettingsInterface* settings);

            //! Return true when radial blur properties should be read-only in the editor.
            bool ArePropertiesReadOnly() const
            {
                return !m_enabled;
            }
        };
    } // namespace Render
} // namespace AZ
