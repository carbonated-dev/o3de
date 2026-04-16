/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/Memory/SystemAllocator.h>

#include <Atom/RPI.Public/Pass/ComputePass.h>
#include <Atom/Feature/VolumetricFog/VolumetricFogSettings.h>

namespace AZ::Render
{
    class FroxelPass final
        : public RPI::ComputePass
    {
        AZ_RPI_PASS(FroxelPass);

        using Base = RPI::ComputePass;
    public:
        AZ_RTTI(AZ::Render::FroxelPass, "{8245E04B-AC97-4FC9-A5E4-90B6F41AC394}", Base);
        AZ_CLASS_ALLOCATOR(FroxelPass, SystemAllocator);
        virtual ~FroxelPass() = default;

        static RPI::Ptr<FroxelPass> Create(const RPI::PassDescriptor& descriptor);

        void SetShaderOption(const Name& optionName, const Name& valueName);

    private:
        FroxelPass(const RPI::PassDescriptor& descriptor);

        // Pass behavior overrides...
        void BuildInternal() override;

        void UpdateFroxelVolumeSize();
    };
}   // namespace AZ::Render
