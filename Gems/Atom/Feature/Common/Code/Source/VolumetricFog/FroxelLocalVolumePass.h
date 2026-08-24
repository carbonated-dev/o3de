/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */
#pragma once

#include <Atom/RPI.Public/Pass/RasterPass.h>
#include <AzCore/std/containers/array.h>

namespace AZ::Render
{
    //! Rasterizes local fog into a current-frame target while exposing the other target as history.
    //! Keeping the targets imported and ping-ponged allows graphics to produce frame N while async
    //! compute consumes and reprojects frame N-1 without a cross-queue dependency.
    class FroxelLocalVolumePass final
        : public RPI::RasterPass
    {
        AZ_RPI_PASS(FroxelLocalVolumePass);

        using Base = RPI::RasterPass;

    public:
        AZ_RTTI(AZ::Render::FroxelLocalVolumePass, "{7253369D-C357-46A4-8339-A2812243B84A}", Base);
        AZ_CLASS_ALLOCATOR(FroxelLocalVolumePass, SystemAllocator);

        static RPI::Ptr<FroxelLocalVolumePass> Create(const RPI::PassDescriptor& descriptor);

        //! Returns the completed target from the previous frame. These attachments are
        //! bound directly by FroxelIntegratePass so this graphics pass does not declare
        //! a current-frame access to them and introduce a graphics/compute dependency.
        const RPI::Ptr<RPI::PassAttachment>& GetMediumHistoryAttachment() const;
        const RPI::Ptr<RPI::PassAttachment>& GetEmissiveHistoryAttachment() const;

    private:
        static constexpr uint8_t HistoryImageCount = 3;

        explicit FroxelLocalVolumePass(const RPI::PassDescriptor& descriptor);

        void BuildInternal() override;
        void ResetInternal() override;
        void FrameEndInternal() override;

        void UpdateFroxelVolumeSize();
        bool UpdateAttachmentImage(RPI::Ptr<RPI::PassAttachment>& attachment);

        AZStd::array<RPI::Ptr<RPI::PassAttachment>, HistoryImageCount> m_mediumAttachments;
        AZStd::array<RPI::Ptr<RPI::PassAttachment>, HistoryImageCount> m_emissiveAttachments;
        RPI::PassAttachmentBinding* m_mediumOutputBinding = nullptr;
        RPI::PassAttachmentBinding* m_emissiveOutputBinding = nullptr;
        uint8_t m_outputIndex = 0;
    };
} // namespace AZ::Render
