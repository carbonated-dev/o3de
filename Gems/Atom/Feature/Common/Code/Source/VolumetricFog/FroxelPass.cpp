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
        auto layout = m_shaderOptions.GetShaderOptionLayout();
        if (!layout)
        {
            return;
        }

        auto value = layout->FindValue(optionName, valueName);
        if (value != m_shaderOptions.GetValue(optionName))
        {
            m_shaderOptions.SetValue(optionName, valueName);
            LoadShader();
        }
    }

    void FroxelPass::BuildInternal()
    {
        UpdateFroxelVolumeSize();
        Base::BuildInternal();
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
