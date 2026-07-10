/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <PostProcessing/RadialBlurPass.h>

#include <PostProcess/PostProcessFeatureProcessor.h>
#include <Atom/Feature/PostProcess/RadialBlur/RadialBlurConstants.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/Scene.h>

namespace AZ
{
    namespace Render
    {
        RPI::Ptr<RadialBlurPass> RadialBlurPass::Create(const RPI::PassDescriptor& descriptor)
        {
            RPI::Ptr<RadialBlurPass> pass = aznew RadialBlurPass(descriptor);
            return AZStd::move(pass);
        }

        RadialBlurPass::RadialBlurPass(const RPI::PassDescriptor& descriptor)
            : RPI::ComputePass(descriptor)
        {
        }

        bool RadialBlurPass::IsEnabled() const
        {
            if (!ComputePass::IsEnabled())
            {
                return false;
            }
            const RPI::Scene* scene = GetScene();
            if (!scene)
            {
                return false;
            }
            PostProcessFeatureProcessor* fp = scene->GetFeatureProcessor<PostProcessFeatureProcessor>();
            if (!fp)
            {
                return false;
            }
            const RPI::ViewPtr view = GetRenderPipeline()->GetFirstView(GetPipelineViewTag());
            PostProcessSettings* postProcessSettings = fp->GetLevelSettingsFromView(view);
            if (!postProcessSettings)
            {
                return false;
            }
            const RadialBlurSettings* radialBlurSettings = postProcessSettings->GetRadialBlurSettings();
            if (!radialBlurSettings)
            {
                return false;
            }
            return radialBlurSettings->GetEnabled();
        }

        void RadialBlurPass::FrameBeginInternal(FramePrepareParams params)
        {
            // Must match the struct in RadialBlur.azsl
            struct Constants
            {
                AZStd::array<u32, 2> m_outputSize;
                AZStd::array<float, 2> m_center;
                float m_amount = RadialBlur::DefaultAmount;
                float m_innerRadius = RadialBlur::DefaultInnerRadius;
                u32 m_sampleCount = RadialBlur::DefaultSampleCount;
                float m_pad = 0.0f;
            } constants{};

            RPI::Scene* scene = GetScene();
            PostProcessFeatureProcessor* fp = scene->GetFeatureProcessor<PostProcessFeatureProcessor>();
            if (fp)
            {
                RPI::ViewPtr view = m_pipeline->GetFirstView(GetPipelineViewTag());
                PostProcessSettings* postProcessSettings = fp->GetLevelSettingsFromView(view);
                if (postProcessSettings)
                {
                    RadialBlurSettings* radialBlurSettings = postProcessSettings->GetRadialBlurSettings();
                    if (radialBlurSettings)
                    {
                        const AZ::Vector2 center = radialBlurSettings->GetCenter();
                        constants.m_center[0] = center.GetX();
                        constants.m_center[1] = center.GetY();
                        constants.m_amount = radialBlurSettings->GetAmount();
                        constants.m_innerRadius = radialBlurSettings->GetInnerRadius();
                        constants.m_sampleCount = radialBlurSettings->GetSampleCount();
                    }
                }
            }

            AZ_Assert(GetOutputCount() > 0, "RadialBlurPass: No output bindings!");
            RPI::PassAttachment* outputAttachment = GetOutputBinding(0).GetAttachment().get();

            AZ_Assert(outputAttachment != nullptr, "RadialBlurPass: Output binding has no attachment!");
            RHI::Size size = outputAttachment->m_descriptor.m_image.m_size;

            constants.m_outputSize[0] = size.m_width;
            constants.m_outputSize[1] = size.m_height;

            m_shaderResourceGroup->SetConstant(m_constantsIndex, constants);

            RPI::ComputePass::FrameBeginInternal(params);
        }
    } // namespace Render
} // namespace AZ
