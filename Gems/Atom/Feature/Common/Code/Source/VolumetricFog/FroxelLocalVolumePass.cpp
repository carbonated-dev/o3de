/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <VolumetricFog/FroxelLocalVolumePass.h>
#include <VolumetricFog/VolumetricFogFeatureProcessor.h>
#include <VolumetricFog/VolumetricFogUtils.h>

#include <Atom/RPI.Public/Scene.h>

namespace AZ::Render
{
    RPI::Ptr<FroxelLocalVolumePass> FroxelLocalVolumePass::Create(const RPI::PassDescriptor& descriptor)
    {
        return aznew FroxelLocalVolumePass(descriptor);
    }

    FroxelLocalVolumePass::FroxelLocalVolumePass(const RPI::PassDescriptor& descriptor)
        : Base(descriptor)
    {
    }

    const RPI::Ptr<RPI::PassAttachment>& FroxelLocalVolumePass::GetMediumHistoryAttachment() const
    {
        return m_mediumAttachments[(m_outputIndex + HistoryImageCount - 1) % HistoryImageCount];
    }

    const RPI::Ptr<RPI::PassAttachment>& FroxelLocalVolumePass::GetEmissiveHistoryAttachment() const
    {
        return m_emissiveAttachments[(m_outputIndex + HistoryImageCount - 1) % HistoryImageCount];
    }

    void FroxelLocalVolumePass::BuildInternal()
    {
        UpdateFroxelVolumeSize();

        m_mediumAttachments[0] = FindAttachment(Name("LocalMedium1"));
        m_mediumAttachments[1] = FindAttachment(Name("LocalMedium2"));
        m_mediumAttachments[2] = FindAttachment(Name("LocalMedium3"));
        m_emissiveAttachments[0] = FindAttachment(Name("LocalEmissive1"));
        m_emissiveAttachments[1] = FindAttachment(Name("LocalEmissive2"));
        m_emissiveAttachments[2] = FindAttachment(Name("LocalEmissive3"));

        bool attachmentsValid = true;
        for (uint32_t index = 0; index < HistoryImageCount; ++index)
        {
            attachmentsValid = attachmentsValid && UpdateAttachmentImage(m_mediumAttachments[index]);
            attachmentsValid = attachmentsValid && UpdateAttachmentImage(m_emissiveAttachments[index]);
        }

        m_mediumOutputBinding = FindAttachmentBinding(Name("FroxelMedium"));
        m_emissiveOutputBinding = FindAttachmentBinding(Name("FroxelEmissive"));

        attachmentsValid = attachmentsValid && m_mediumOutputBinding && m_emissiveOutputBinding;
        if (!attachmentsValid)
        {
            SetEnabled(false);
            AZ_Error(
                "FroxelLocalVolumePass",
                false,
                "FroxelLocalVolumePass requires three valid medium and emissive history attachments.");
            return;
        }

        m_mediumOutputBinding->SetAttachment(m_mediumAttachments[m_outputIndex]);
        m_emissiveOutputBinding->SetAttachment(m_emissiveAttachments[m_outputIndex]);

        Base::BuildInternal();
    }

    void FroxelLocalVolumePass::ResetInternal()
    {
        m_mediumAttachments.fill(nullptr);
        m_emissiveAttachments.fill(nullptr);
        m_mediumOutputBinding = nullptr;
        m_emissiveOutputBinding = nullptr;
        m_outputIndex = 0;
        Base::ResetInternal();
    }

    void FroxelLocalVolumePass::FrameEndInternal()
    {
        // The target just rendered becomes history. Rotate across the engine's normal three
        // in-flight frames so graphics does not recycle a render target while async Integrate
        // can still be sampling it from an earlier frame.
        m_outputIndex = (m_outputIndex + 1) % HistoryImageCount;
        UpdateAttachmentImage(m_mediumAttachments[m_outputIndex]);
        UpdateAttachmentImage(m_emissiveAttachments[m_outputIndex]);
        m_mediumOutputBinding->SetAttachment(m_mediumAttachments[m_outputIndex]);
        m_emissiveOutputBinding->SetAttachment(m_emissiveAttachments[m_outputIndex]);

        Base::FrameEndInternal();
    }

    bool FroxelLocalVolumePass::UpdateAttachmentImage(RPI::Ptr<RPI::PassAttachment>& attachment)
    {
        return UpdateImportedAttachmentImage(
            attachment,
            RHI::ImageBindFlags::Color | RHI::ImageBindFlags::ShaderRead);
    }

    void FroxelLocalVolumePass::UpdateFroxelVolumeSize()
    {
        const RPI::Scene* scene = GetScene();
        const auto* fogFeatureProcessor = scene ? scene->GetFeatureProcessor<VolumetricFogFeatureProcessor>() : nullptr;
        if (!fogFeatureProcessor)
        {
            return;
        }

        const RHI::Size tileSize = VolumetricFog::ToFroxelSize(fogFeatureProcessor->GetFogQuality());
        for (RPI::Ptr<RPI::PassAttachment>& attachment : m_ownedAttachments)
        {
            AZ_Assert(
                attachment && attachment->m_descriptor.m_type == RHI::AttachmentType::Image,
                "[FroxelLocalVolumePass %s] requires image attachments.",
                GetPathName().GetCStr());
            if (!attachment || attachment->m_descriptor.m_type != RHI::AttachmentType::Image)
            {
                continue;
            }

            attachment->m_sizeMultipliers.m_widthMultiplier = 1.0f / tileSize.m_width;
            attachment->m_sizeMultipliers.m_heightMultiplier = 1.0f / tileSize.m_height;
            attachment->m_sizeMultipliers.m_depthMultiplier = static_cast<float>(tileSize.m_depth);
        }
    }
} // namespace AZ::Render
