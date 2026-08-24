/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <VolumetricFog/FroxelPass.h>
#include <VolumetricFog/VolumetricFogUtils.h>
#include <VolumetricFog/VolumetricFogFeatureProcessor.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/View.h>
#include <Atom/RPI.Public/Pass/ParentPass.h>
#include <Atom/RPI.Reflect/Shader/ShaderOptionGroup.h>
#include <Atom/RHI.Reflect/ShaderResourceGroupLayout.h>
#include <AzCore/Math/Plane.h>


namespace AZ::Render
{
    RPI::Ptr<FroxelPass> FroxelPass::Create(const RPI::PassDescriptor& descriptor)
    {
        RPI::Ptr<FroxelPass> pass = aznew FroxelPass(descriptor);
        return pass;
    }


    FroxelPass::FroxelPass(const RPI::PassDescriptor& descriptor)
        : RPI::ComputePass(descriptor)
    {
    }

    void FroxelPass::SetShaderOption(const Name& optionName, const Name& valueName)
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

    void FroxelPass::SetShaderOptions(RPI::ShaderOptionGroup shaderOptions)
    {
        if (!m_froxelShaderOptions.GetShaderOptionLayout() ||
            shaderOptions.GetShaderVariantId() != m_froxelShaderOptions.GetShaderVariantId())
        {
            m_froxelShaderOptions = AZStd::move(shaderOptions);
            UpdateShaderOptions(m_froxelShaderOptions.GetShaderVariantId());
        }
    }

    void FroxelPass::BuildInternal()
    {
        // Scatter is deliberately serialized after the bandwidth-heavy opaque pass. On async
        // compute it competes for the same SM/cache resources and remains on the frame's critical
        // path. Inject remains an early async pass; Integrate's attachment dependency waits for it
        // only when the injected volume is actually consumed.
        if (GetName() == Name("FroxelScatterPass"))
        {
            RPI::ParentPass* froxelParent = GetParent();
            RPI::ParentPass* pipelineParent = froxelParent ? froxelParent->GetParent() : nullptr;
            RPI::Ptr<RPI::Pass> opaquePass = pipelineParent
                ? pipelineParent->FindChildPass(Name("OpaquePass"))
                : nullptr;
            AZ_Error(
                "FroxelPass",
                opaquePass,
                "FroxelScatterPass could not find OpaquePass for graphics-queue scheduling.");
            if (opaquePass)
            {
                m_executeAfterPasses.push_back(opaquePass.get());
            }
        }

        UpdateFroxelVolumeSize();
        Base::BuildInternal();
    }

    void FroxelPass::FrameBeginInternal(FramePrepareParams params)
    {
        // FroxelPass is shared by Inject and Scatter. Scatter deliberately stays on graphics
        // because it regressed under opaque-pass contention; only Inject is async eligible.
        const bool useAsyncCompute =
            GetName() == Name("FroxelInjectPass") && VolumetricFog::IsAsyncComputeEnabled();
        m_hardwareQueueClass = useAsyncCompute
            ? RHI::HardwareQueueClass::Compute
            : RHI::HardwareQueueClass::Graphics;
        SetHardwareQueueClass(m_hardwareQueueClass);
        Base::FrameBeginInternal(params);
    }

    void FroxelPass::UpdateFroxelVolumeSize()
    {
        if (auto scene = GetScene())
        {
            if (auto fp = scene->GetFeatureProcessor<VolumetricFogFeatureProcessor>())
            {
                RPI::Ptr<RPI::PassAttachment> attachment = m_ownedAttachments.front();
                AZ_Assert(attachment, "[FroxelPass %s] Cannot find froxel volume image attachment.", GetPathName().GetCStr());
                AZ_Assert(
                    attachment->m_descriptor.m_type == RHI::AttachmentType::Image,
                    "[FroxelPass %s] requires an image attachment",
                    GetPathName().GetCStr());

                RHI::Size tileSize = VolumetricFog::ToFroxelSize(fp->GetFogQuality());
                auto& multipliers = attachment->m_sizeMultipliers;
                multipliers.m_widthMultiplier = 1.0f / tileSize.m_width;
                multipliers.m_heightMultiplier = 1.0f / tileSize.m_height;
                multipliers.m_depthMultiplier = static_cast<float>(tileSize.m_depth);
            }
        }
    }
}   // namespace AZ::Render
