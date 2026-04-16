/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzFramework/Components/ComponentAdapter.h>
#include <VolumetricFog/VolumetricFogComponentController.h>

namespace AZ::Render
{
    class VolumetricFogComponent final
        : public AzFramework::Components::ComponentAdapter<VolumetricFogComponentController, VolumetricFogComponentConfig>
    {
    public:
        using BaseClass = AzFramework::Components::ComponentAdapter<VolumetricFogComponentController, VolumetricFogComponentConfig>;
        AZ_COMPONENT(AZ::Render::VolumetricFogComponent, "{BAC79FA8-C018-4841-ACED-ABCAE395C51F}" , BaseClass);

        VolumetricFogComponent() = default;
        VolumetricFogComponent(const VolumetricFogComponentConfig& config);

        static void Reflect(AZ::ReflectContext* context);
    };
} // namespace AZ::Render
