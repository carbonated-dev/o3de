/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <VolumetricFog/VolumetricFogUtils.h>

#include <AzCore/Console/IConsole.h>

namespace AZ::Render::VolumetricFog
{
    AZ_CVAR(
        bool,
        r_volumetricFogAsyncCompute,
        true,
        nullptr,
        AZ::ConsoleFunctorFlags::Null,
        "Run async-compute-eligible volumetric fog passes on the compute queue. "
        "0 uses the graphics queue; 1 uses async compute.");

    bool IsAsyncComputeEnabled()
    {
        return r_volumetricFogAsyncCompute;
    }

    RHI::Size ToFroxelSize(VolumetricFogQuality quality)
    {
        // The width and height are the pixel size of a froxel
        // The depth is the number of froxels in the z direction
        switch (quality)
        {
        case VolumetricFogQuality::Low:
            return RHI::Size(16, 16, 64);
        case VolumetricFogQuality::Mid:
            return RHI::Size(8, 8, 128);
        case VolumetricFogQuality::High:
            return RHI::Size(4, 4, 128);
        default:
            AZ_Assert(false, "Invalid VolumetricFogQuality value %d", static_cast<int>(quality));
            return RHI::Size();
        }
    }    
} // namespace AZ::Render::VolumetricFog
