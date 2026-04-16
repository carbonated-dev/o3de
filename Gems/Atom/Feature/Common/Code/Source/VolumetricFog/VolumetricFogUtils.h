/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <Atom/RHI.Reflect/Size.h>
#include <AzCore/Name/Name.h>
#include <Atom/Feature/VolumetricFog/VolumetricFogConstants.h>

namespace AZ::Render::VolumetricFog
{
    RHI::Size ToFroxelSize(VolumetricFogQuality quality);
}   // namespace AZ::Render::VolumetricFog
