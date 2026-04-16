/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/Feature/Utils/EditorRenderComponentAdapter.h>
#include <VolumetricFog/VolumetricFogComponent.h>

namespace AZ::Render
{
    class EditorVolumetricFogComponent final
        : public EditorRenderComponentAdapter<VolumetricFogComponentController, VolumetricFogComponent, VolumetricFogComponentConfig>
    {
    public:

        using BaseClass = EditorRenderComponentAdapter<VolumetricFogComponentController, VolumetricFogComponent, VolumetricFogComponentConfig>;
        AZ_EDITOR_COMPONENT(AZ::Render::EditorVolumetricFogComponent, "{25D8BC2C-A77E-46F5-B35D-FC6C61E97839}", BaseClass);

        static void Reflect(AZ::ReflectContext* context);

        EditorVolumetricFogComponent() = default;
        EditorVolumetricFogComponent(const VolumetricFogComponentConfig& config);

    private:
        //! EditorRenderComponentAdapter
        //! we override OnEntityVisibilityChanged to avoid deactivating and activating unnecessarily
        AZ::u32 OnConfigurationChanged() override;
        void OnEntityVisibilityChanged(bool visibility) override;
    };
} // namespace AZ::Render
