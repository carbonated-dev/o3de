/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/Feature/Utils/EditorRenderComponentAdapter.h>
#include <VolumetricFog/FogVolumeComponent.h>

namespace AZ::Render
{
    //! Editor component for a local fog volume override; requires a Box or Sphere shape
    //! component on the same entity.
    class EditorFogVolumeComponent final
        : public EditorRenderComponentAdapter<FogVolumeComponentController, FogVolumeComponent, FogVolumeComponentConfig>
    {
    public:
        using BaseClass = EditorRenderComponentAdapter<FogVolumeComponentController, FogVolumeComponent, FogVolumeComponentConfig>;
        AZ_EDITOR_COMPONENT(EditorFogVolumeComponent, "{A2D7F1E5-C4B3-4F9A-87D2-3B0E6C5A9F1D}", BaseClass);

        EditorFogVolumeComponent() = default;
        explicit EditorFogVolumeComponent(const FogVolumeComponentConfig& config);

        static void Reflect(AZ::ReflectContext* context);

        void Activate() override;
        void Deactivate() override;

    private:
        AZ::u32 OnConfigurationChanged() override;
        //! Detects when the Shape property has changed in the config and swaps the attached shape component (Box <-> Sphere) accordingly.
        bool HandleShapeTypeChange();

        FogVolumeShape m_shape = FogVolumeShape::Unknown; // cached shape from last frame to detect changes in HandleShapeTypeChange
    };

} // namespace AZ::Render
