/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <PostProcess/RadialBlur/RadialBlurComponent.h>

#include <AzToolsFramework/API/ComponentEntitySelectionBus.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentAdapter.h>

namespace AZ
{
    namespace Render
    {
        namespace RadialBlur
        {
            inline constexpr AZ::TypeId EditorRadialBlurComponentTypeId{ "{7D477F47-9C29-414E-8B7E-9980594CC5CC}" };
        }

        //! Editor component that exposes radial blur post-process settings in the editor.
        class EditorRadialBlurComponent final
            : public AzToolsFramework::Components::EditorComponentAdapter<RadialBlurComponentController, RadialBlurComponent, RadialBlurComponentConfig>
        {
        public:
            using BaseClass =
                AzToolsFramework::Components::EditorComponentAdapter<RadialBlurComponentController, RadialBlurComponent, RadialBlurComponentConfig>;
            AZ_EDITOR_COMPONENT(AZ::Render::EditorRadialBlurComponent, RadialBlur::EditorRadialBlurComponentTypeId, BaseClass);

            //! Create an editor radial blur component with default configuration.
            EditorRadialBlurComponent() = default;
            //! Create an editor radial blur component with the supplied configuration.
            EditorRadialBlurComponent(const RadialBlurComponentConfig& config);
            ~EditorRadialBlurComponent() = default;

            //! Reflect the editor radial blur component and its editor properties.
            static void Reflect(AZ::ReflectContext* context);

        protected:
            u32 OnConfigurationChanged() override;
        };
    } // namespace Render
} // namespace AZ
