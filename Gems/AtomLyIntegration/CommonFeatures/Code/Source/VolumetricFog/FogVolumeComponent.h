/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzFramework/Components/ComponentAdapter.h>
#include <VolumetricFog/FogVolumeComponentController.h>
#include <AtomLyIntegration/CommonFeatures/VolumetricFog/FogVolumeComponentConfig.h>

namespace AZ::Render
{
    class FogVolumeComponent final
        : public AzFramework::Components::ComponentAdapter<FogVolumeComponentController, FogVolumeComponentConfig>
    {
    public:
        using BaseClass = AzFramework::Components::ComponentAdapter<FogVolumeComponentController, FogVolumeComponentConfig>;
        AZ_COMPONENT(FogVolumeComponent, "{1D4A9F7B-E3C2-4B8D-A5F1-6C0E9D2B7A3F}", BaseClass);

        FogVolumeComponent() = default;
        explicit FogVolumeComponent(const FogVolumeComponentConfig& config);

        static void Reflect(AZ::ReflectContext* context);
    };

} // namespace AZ::Render
