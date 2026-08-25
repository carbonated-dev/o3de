/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <VolumetricFog/FroxelIntegratePass.h>
#include <VolumetricFog/FroxelLocalVolumePass.h>
#include <VolumetricFog/VolumetricFogUtils.h>
#include <VolumetricFog/VolumetricFogFeatureProcessor.h>

namespace AZ::Render
{
    RPI::Ptr<FroxelIntegratePass> FroxelIntegratePass::Create(const RPI::PassDescriptor& descriptor)
    {
        RPI::Ptr<FroxelIntegratePass> pass = aznew FroxelIntegratePass(descriptor);
        return pass;
    }

    FroxelIntegratePass::FroxelIntegratePass(const RPI::PassDescriptor& descriptor)
        : RPI::ComputePass(descriptor)
    {
    }

    void FroxelIntegratePass::SetShaderOption(const Name& optionName, const Name& valueName)
    {
        const auto shader = GetShader();
        if (!shader)
        {
            return;
        }

        const auto& defaultOptions = shader->GetDefaultShaderOptions();
        const auto defaultLayout = defaultOptions.GetShaderOptionLayout();
        if (!defaultLayout)
        {
            return;
        }
        if (!m_froxelShaderOptions.GetShaderOptionLayout() ||
            m_froxelShaderOptions.GetShaderOptionLayout()->GetHash() != defaultLayout->GetHash())
        {
            m_froxelShaderOptions = defaultOptions;
        }

        const auto layout = m_froxelShaderOptions.GetShaderOptionLayout();
        if (!layout)
        {
            return;
        }

        const auto value = layout->FindValue(optionName, valueName);
        if (value != m_froxelShaderOptions.GetValue(optionName))
        {
            m_froxelShaderOptions.SetValue(optionName, valueName);
            UpdateShaderOptions(m_froxelShaderOptions.GetShaderVariantId());
        }
    }

    void FroxelIntegratePass::BuildInternal()
    {
        m_scatteringAttachments[0] = FindAttachment(Name("Scattering1"));
        m_scatteringAttachments[1] = FindAttachment(Name("Scattering2"));

        bool attachmentsValid = true;
        for (auto i : { 0, 1 })
        {
            if (m_scatteringAttachments[i])
            {
                attachmentsValid = UpdateAttachmentImage(i);
            }
            else
            {
                attachmentsValid = false;
            }
            if (!attachmentsValid)
            {
                break;
            }

        }

        if (!attachmentsValid)
        {
            SetEnabled(false);
            AZ_Error(
                "FroxelIntegratePass", false,
                "FroxelIntegratePass disabled because the ImageAttachments Scattering1 and Scattering2 are invalid.");
            return;
        }

        m_historyScatteredBinding = FindAttachmentBinding(Name("FroxelHistory"));
        AZ_Error("FroxelIntegratePass", m_historyScatteredBinding, "FroxelIntegratePass requires a slot for FroxelHistory.");
        m_scatteredBinding = FindAttachmentBinding(Name("FroxelOutput"));
        AZ_Error("FroxelIntegratePass", m_scatteredBinding, "FroxelIntegratePass requires a slot for FroxelOutput.");
        m_localMediumHistoryBinding = FindAttachmentBinding(Name("FroxelLocalMediumHistory"));
        m_localEmissiveHistoryBinding = FindAttachmentBinding(Name("FroxelLocalEmissiveHistory"));
        attachmentsValid = m_historyScatteredBinding && m_scatteredBinding &&
            m_localMediumHistoryBinding && m_localEmissiveHistoryBinding && UpdateLocalVolumeHistoryBindings();
        if (!attachmentsValid)
        {
            SetEnabled(false);
            AZ_Error(
                "FroxelIntegratePass",
                false,
                "FroxelIntegratePass requires valid previous-frame local-volume attachments.");
            return;
        }
        if (m_historyScatteredBinding->GetAttachment() == nullptr)
        {
            m_scatteredBinding->SetAttachment(m_scatteringAttachments[0]);
            m_historyScatteredBinding->SetAttachment(m_scatteringAttachments[1]);
        }

        Base::BuildInternal();
    }

    void FroxelIntegratePass::ResetInternal()
    {
        m_scatteringAttachments[0].reset();
        m_scatteringAttachments[1].reset();
        m_historyScatteredBinding = nullptr;
        m_scatteredBinding = nullptr;
        m_localMediumHistoryBinding = nullptr;
        m_localEmissiveHistoryBinding = nullptr;
        Base::ResetInternal();
    }

    void FroxelIntegratePass::FrameBeginInternal(FramePrepareParams params)
    {
        m_hardwareQueueClass = VolumetricFog::IsAsyncComputeEnabled()
            ? RHI::HardwareQueueClass::Compute
            : RHI::HardwareQueueClass::Graphics;
        SetHardwareQueueClass(m_hardwareQueueClass);
        UpdateLocalVolumeHistoryBindings();
        Base::FrameBeginInternal(params);
        UpdateThreadsCount();
    }

    bool FroxelIntegratePass::UpdateLocalVolumeHistoryBindings()
    {
        RPI::ParentPass* parent = GetParent();
        FroxelLocalVolumePass* localVolumePass = parent
            ? azrtti_cast<FroxelLocalVolumePass*>(parent->FindChildPass(Name("FroxelLocalVolumePass")).get())
            : nullptr;
        if (!localVolumePass || !m_localMediumHistoryBinding || !m_localEmissiveHistoryBinding ||
            !localVolumePass->GetMediumHistoryAttachment() || !localVolumePass->GetEmissiveHistoryAttachment())
        {
            return false;
        }

        m_localMediumHistoryBinding->SetAttachment(localVolumePass->GetMediumHistoryAttachment());
        m_localEmissiveHistoryBinding->SetAttachment(localVolumePass->GetEmissiveHistoryAttachment());
        return true;
    }

    void FroxelIntegratePass::FrameEndInternal()
    {
        m_historyScatteredBinding->SetAttachment(m_scatteringAttachments[m_scatteringOuptutIndex]);
        m_scatteringOuptutIndex ^= 1;
        UpdateAttachmentImage(m_scatteringOuptutIndex);
        m_scatteredBinding->SetAttachment(m_scatteringAttachments[m_scatteringOuptutIndex]);
        Base::FrameEndInternal();
    }

    bool FroxelIntegratePass::UpdateAttachmentImage(uint32_t attachmentIndex)
    {
        RPI::Ptr<RPI::PassAttachment>& attachment = m_scatteringAttachments[attachmentIndex];
        return UpdateImportedAttachmentImage(attachment);
    }

    void FroxelIntegratePass::UpdateThreadsCount()
    {
        RPI::PassAttachment* outputAttachment = nullptr;

        if (GetOutputCount() > 0)
        {
            outputAttachment = GetOutputBinding(0).GetAttachment().get();
        }
        else if (GetInputOutputCount() > 0)
        {
            outputAttachment = GetInputOutputBinding(0).GetAttachment().get();
        }

        AZ_Assert(
            outputAttachment != nullptr,
            "[FroxelIntegratePass '%s']: A fullscreen compute pass must have a valid output or input/output.",
            GetPathName().GetCStr());

        AZ_Assert(
            outputAttachment->GetAttachmentType() == RHI::AttachmentType::Image,
            "[FroxelIntegratePass '%s']: The output of a fullscreen compute pass must be an image.",
            GetPathName().GetCStr());

        RHI::Size targetImageSize = outputAttachment->m_descriptor.m_image.m_size;
        SetTargetThreadCounts(targetImageSize.m_width, targetImageSize.m_height, 1);
    }
}   // namespace AZ::Render
