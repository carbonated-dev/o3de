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
#include <Atom/RPI.Reflect/Pass/ComputePassData.h>
#include <Atom/Feature/VolumetricFog/VolumetricFogSettings.h>

namespace AZ::Render
{
    //! Compute pass that performs temporal reprojection then ray-marching integration
    //! of the volumetric fog froxel volume in a single z-slice loop.
    class FroxelIntegratePass final
        : public RPI::ComputePass
    {
        AZ_RPI_PASS(FroxelIntegratePass);

        using Base = RPI::ComputePass;
    public:
        AZ_RTTI(AZ::Render::FroxelIntegratePass, "{E3E55EC0-BA58-4B33-BDA6-7746151E3408}", Base);
        AZ_CLASS_ALLOCATOR(FroxelIntegratePass, SystemAllocator);
        virtual ~FroxelIntegratePass() = default;

        static RPI::Ptr<FroxelIntegratePass> Create(const RPI::PassDescriptor& descriptor);

    private:
        FroxelIntegratePass(const RPI::PassDescriptor& descriptor);

        // Pass behavior overrides
        //! Creates the two ping-pong scatter history attachments and wires them to the pass bindings.
        void BuildInternal() override;
        //! Tears down ping-pong attachments so they are recreated on next resize.
        void ResetInternal() override;
        //! Swaps the ping-pong index and rebinds history/output attachments for this frame.
        void FrameBeginInternal(FramePrepareParams params) override;
        //! Advances the froxelFrameIndex in the scene SRG.
        void FrameEndInternal() override;

        //! Allocates or resizes the Texture3D at the given ping-pong slot to match the current froxel grid.
        bool UpdateAttachmentImage(uint32_t attachmentIndex);
        //! Sets dispatch thread count to match froxel XY.
        void UpdateThreadsCount();

        AZStd::array<Data::Instance<RPI::PassAttachment>, 2> m_scatteringAttachments; // ping-pong pair of Texture3D attachments holding previous-frame scatter for temporal reprojection.
        RPI::PassAttachmentBinding* m_historyScatteredBinding = nullptr; // cached binding pointers for fast per-frame rebind.
        RPI::PassAttachmentBinding* m_scatteredBinding = nullptr; // cached binding pointers for fast per-frame rebind.
        uint8_t m_scatteringOuptutIndex = 0; // 0 or 1, toggles each frame to alternate write target.
    };
}   // namespace AZ::Render
