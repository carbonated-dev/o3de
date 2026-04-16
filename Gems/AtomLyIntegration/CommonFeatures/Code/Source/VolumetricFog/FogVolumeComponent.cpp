/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <VolumetricFog/FogVolumeComponent.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace AZ::Render
{
    FogVolumeComponent::FogVolumeComponent(const FogVolumeComponentConfig& config)
        : BaseClass(config)
    {
    }

    void FogVolumeComponent::Reflect(AZ::ReflectContext* context)
    {
        BaseClass::Reflect(context);

        if (auto* sc = azrtti_cast<AZ::SerializeContext*>(context))
        {
            sc->Class<FogVolumeComponent, BaseClass>()
                ->Version(1);
        }
    }

} // namespace AZ::Render
