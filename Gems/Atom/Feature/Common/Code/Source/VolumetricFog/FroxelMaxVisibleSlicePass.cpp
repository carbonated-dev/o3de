/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <VolumetricFog/FroxelMaxVisibleSlicePass.h>
#include <VolumetricFog/VolumetricFogFeatureProcessor.h>
#include <VolumetricFog/VolumetricFogUtils.h>

#include <Atom/RPI.Public/Scene.h>

namespace AZ::Render
{
    RPI::Ptr<FroxelMaxVisibleSlicePass> FroxelMaxVisibleSlicePass::Create(const RPI::PassDescriptor& descriptor)
    {
        return aznew FroxelMaxVisibleSlicePass(descriptor);
    }

    FroxelMaxVisibleSlicePass::FroxelMaxVisibleSlicePass(const RPI::PassDescriptor& descriptor)
        : Base(descriptor)
    {
    }

    void FroxelMaxVisibleSlicePass::BuildInternal()
    {
        UpdateOutputSize();
        Base::BuildInternal();
    }

    void FroxelMaxVisibleSlicePass::FrameBeginInternal(FramePrepareParams params)
    {
        m_hardwareQueueClass = VolumetricFog::IsAsyncComputeEnabled()
            ? RHI::HardwareQueueClass::Compute
            : RHI::HardwareQueueClass::Graphics;
        SetHardwareQueueClass(m_hardwareQueueClass);
        Base::FrameBeginInternal(params);
    }

    void FroxelMaxVisibleSlicePass::UpdateOutputSize()
    {
        if (auto scene = GetScene())
        {
            if (auto fogFeatureProcessor = scene->GetFeatureProcessor<VolumetricFogFeatureProcessor>())
            {
                // The output binding is not connected until Base::BuildInternal(). Look up the
                // attachment owned by this pass so its size can be configured before that build.
                RPI::Ptr<RPI::PassAttachment> attachment = FindOwnedAttachment(Name("FroxelMaxVisibleSlice"));
                AZ_Assert(attachment, "[FroxelMaxVisibleSlicePass %s] cannot find its output image.", GetPathName().GetCStr());
                if (!attachment)
                {
                    return;
                }
                AZ_Assert(
                    attachment->m_descriptor.m_type == RHI::AttachmentType::Image,
                    "[FroxelMaxVisibleSlicePass %s] requires an image attachment.",
                    GetPathName().GetCStr());

                const RHI::Size froxelSize = VolumetricFog::ToFroxelSize(fogFeatureProcessor->GetFogQuality());
                auto& multipliers = attachment->m_sizeMultipliers;
                multipliers.m_widthMultiplier = 1.0f / froxelSize.m_width;
                multipliers.m_heightMultiplier = 1.0f / froxelSize.m_height;
                multipliers.m_depthMultiplier = 1.0f;
            }
        }
    }
} // namespace AZ::Render
