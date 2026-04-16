/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/RPI.Public/FeatureProcessor.h>
#include <Atom/Feature/VolumetricFog/VolumetricFogConstants.h>
#include <Atom/Feature/VolumetricFog/VolumetricFogSettings.h>
#include <Atom/Feature/LightingChannel/LightingChannelConfiguration.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>

namespace AZ::Render
{
    class VolumetricFogFeatureProcessorInterface
        : public RPI::FeatureProcessor
    {
    public:
        AZ_RTTI(AZ::Render::VolumetricFogFeatureProcessorInterface, "{B95F37FC-DF37-4F48-AE57-9A8AC6E3BE95}");

        virtual const VolumetricFogSettings& GetSettings() const = 0;

        // Generate global fog getters and setters.
#include <Atom/Feature/ParamMacros/StartParamFunctionsVirtual.inl>
#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
    };
} // namespace AZ::Render
