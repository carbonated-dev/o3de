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

namespace AZ::Render
{
    //! Builds one conservative maximum visible Z slice for every XY froxel column.
    class FroxelMaxVisibleSlicePass final
        : public RPI::ComputePass
    {
        AZ_RPI_PASS(FroxelMaxVisibleSlicePass);

        using Base = RPI::ComputePass;

    public:
        AZ_RTTI(AZ::Render::FroxelMaxVisibleSlicePass, "{6D35A092-B2FD-47E7-9722-D256E62DF34B}", Base);
        AZ_CLASS_ALLOCATOR(FroxelMaxVisibleSlicePass, SystemAllocator);
        ~FroxelMaxVisibleSlicePass() override = default;

        static RPI::Ptr<FroxelMaxVisibleSlicePass> Create(const RPI::PassDescriptor& descriptor);

    private:
        explicit FroxelMaxVisibleSlicePass(const RPI::PassDescriptor& descriptor);

        void BuildInternal() override;
        void FrameBeginInternal(FramePrepareParams params) override;
        void UpdateOutputSize();
    };
} // namespace AZ::Render
