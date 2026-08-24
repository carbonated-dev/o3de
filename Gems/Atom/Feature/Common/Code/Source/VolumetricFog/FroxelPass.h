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
#include <Atom/RPI.Reflect/Shader/ShaderOptionGroup.h>

namespace AZ::Render
{
    //! Thin ComputePass subclass that exposes shader option control for quality/noise
    //! variants and resizes the dispatch count to match the froxel grid.
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

        //! Writes a named option value into the persistent shader option group.
        void SetShaderOption(const Name& optionName, const Name& valueName);

        //! Applies a fully configured shader option group to this pass.
        void SetShaderOptions(RPI::ShaderOptionGroup shaderOptions);

    private:
        FroxelPass(const RPI::PassDescriptor& descriptor);

        // Pass behavior overrides...
        void BuildInternal() override;
        void FrameBeginInternal(FramePrepareParams params) override;

        //! Resolves froxel grid dimensions
        void UpdateFroxelVolumeSize();

        //! Persistent option state used to select compute variants on non-Carbonated builds.
        RPI::ShaderOptionGroup m_froxelShaderOptions;
    };
}   // namespace AZ::Render
