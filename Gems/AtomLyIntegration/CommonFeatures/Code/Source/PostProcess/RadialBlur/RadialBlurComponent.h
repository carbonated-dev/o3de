/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/Feature/PostProcess/RadialBlur/RadialBlurConstants.h>
#include <AtomLyIntegration/CommonFeatures/PostProcess/RadialBlur/RadialBlurComponentConfig.h>
#include <PostProcess/RadialBlur/RadialBlurComponentController.h>

#include <AzFramework/Components/ComponentAdapter.h>

namespace AZ
{
    namespace Render
    {
        namespace RadialBlur
        {
            inline constexpr AZ::TypeId RadialBlurComponentTypeId{ "{3C442CFC-8120-4BAE-9AA0-F9110B40D1B1}" };
        }

        //! Runtime component that exposes radial blur post-process settings on an entity.
        class RadialBlurComponent final
            : public AzFramework::Components::ComponentAdapter<RadialBlurComponentController, RadialBlurComponentConfig>
        {
        public:
            using BaseClass = AzFramework::Components::ComponentAdapter<RadialBlurComponentController, RadialBlurComponentConfig>;
            AZ_COMPONENT(AZ::Render::RadialBlurComponent, RadialBlur::RadialBlurComponentTypeId, BaseClass);

            //! Create a runtime radial blur component with default configuration.
            RadialBlurComponent() = default;
            //! Create a runtime radial blur component with the supplied configuration.
            RadialBlurComponent(const RadialBlurComponentConfig& config);
            ~RadialBlurComponent() = default;

            //! Reflect the runtime radial blur component.
            static void Reflect(AZ::ReflectContext* context);
        };
    } // namespace Render
} // namespace AZ
