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
        void BuildInternal() override;
        void ResetInternal() override;
        void FrameBeginInternal(FramePrepareParams params) override;
        void FrameEndInternal() override;

        bool UpdateAttachmentImage(uint32_t attachmentIndex);
        void UpdateThreadsCount();

        // Ping-pong attachments for temporal history
        AZStd::array<Data::Instance<RPI::PassAttachment>, 2> m_scatteringAttachments;
        RPI::PassAttachmentBinding* m_historyScatteredBinding = nullptr;
        RPI::PassAttachmentBinding* m_scatteredBinding = nullptr;
        uint8_t m_scatteringOuptutIndex = 0;
    };
}   // namespace AZ::Render
