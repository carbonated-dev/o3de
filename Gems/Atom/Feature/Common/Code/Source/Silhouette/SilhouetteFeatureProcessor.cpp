/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RPI.Public/Pass/Pass.h>
#include <Atom/RPI.Public/Pass/PassFilter.h>
#include <Atom/RPI.Public/Pass/RasterPass.h>
#include <Atom/RPI.Public/View.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/DynamicDraw/DynamicDrawInterface.h>
#include <AzCore/Console/IConsole.h>
#include <AzFramework/Entity/GameEntityContextBus.h>
#include <AzFramework/Scene/Scene.h>
#include <AzFramework/Scene/SceneSystemInterface.h>
#include <Silhouette/SilhouetteFeatureProcessor.h>

void OnSilhouetteActiveChanged(const bool& activate)
{
    AzFramework::EntityContextId entityContextId;
    AzFramework::GameEntityContextRequestBus::BroadcastResult(
        entityContextId, &AzFramework::GameEntityContextRequestBus::Events::GetGameEntityContextId);

    if (auto scene = AZ::RPI::Scene::GetSceneForEntityContextId(entityContextId); scene != nullptr)
    {
        // avoid unnecessary enable/disable to avoid warning log spam
        auto featureProcessor = scene->GetFeatureProcessor<AZ::Render::SilhouetteFeatureProcessor>();
        if (featureProcessor)
        {
            featureProcessor->SetPassesEnabled(activate);
        }
    }
}

#if defined(CARBONATED)
void OnSilhouetteMaxOutlineSizeChanged([[maybe_unused]] const uint32_t& size)
{
    AzFramework::EntityContextId entityContextId;
    AzFramework::GameEntityContextRequestBus::BroadcastResult(
        entityContextId, &AzFramework::GameEntityContextRequestBus::Events::GetGameEntityContextId);

    if (auto scene = AZ::RPI::Scene::GetSceneForEntityContextId(entityContextId); scene != nullptr)
    {
        // avoid unnecessary enable/disable to avoid warning log spam
        auto featureProcessor = scene->GetFeatureProcessor<AZ::Render::SilhouetteFeatureProcessor>();
        if (featureProcessor)
        {
            featureProcessor->SetMaxOutlineSize(size);
        }
    }
}

AZ_CVAR(
    uint32_t,
    r_silhouetteMaxOutlineSize,
    14,
    &OnSilhouetteMaxOutlineSizeChanged,
    AZ::ConsoleFunctorFlags::Null,
    "Controls the max size of the outline in pixels");
#endif

AZ_CVAR(
    bool,
    r_silhouette,
    true,
    &OnSilhouetteActiveChanged,
    AZ::ConsoleFunctorFlags::Null,
    "Controls if the silhouette rendering feature is active.  0 : Inactive,  1 : Active (default)");

namespace AZ::Render
{
    SilhouetteFeatureProcessor::SilhouetteFeatureProcessor() = default;
    SilhouetteFeatureProcessor::~SilhouetteFeatureProcessor() = default;

    void SilhouetteFeatureProcessor::Reflect(ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext->Class<SilhouetteFeatureProcessor, FeatureProcessor>()->Version(0);
        }
    }

    void SilhouetteFeatureProcessor::Activate()
    {
        EnableSceneNotification();
    }

    void SilhouetteFeatureProcessor::Deactivate()
    {
        DisableSceneNotification();

        m_rasterPass = nullptr;
        m_compositePass = nullptr;
    }

    void SilhouetteFeatureProcessor::SetPassesEnabled(bool enabled)
    {
#if defined(CARBONATED)
        enabled = enabled && r_silhouette;
#endif
        if (m_compositePass && m_rasterPass)
        {
            m_compositePass->SetEnabled(enabled);
            m_rasterPass->SetEnabled(enabled);
        }
#if defined(CARBONATED)
        m_enabled = enabled;
#endif
    }

#if defined(CARBONATED)
    void SilhouetteFeatureProcessor::SetMaxOutlineSize([[maybe_unused]] const uint32_t size)
    {
        if (m_renderPipeline)
        {
            Name jfaStepParentPassName = Name("SilhouetteJFAStepParentPass");
            auto jfaParentPass = m_renderPipeline->FindFirstPass(jfaStepParentPassName); 
            if (!jfaParentPass)
            {
                AZ_Warning("SilhouetteFeatureProcessor", false, "Can't find %s in the render pipeline.", jfaStepParentPassName.GetCStr());
                return;
            }

            jfaParentPass->QueueForBuildAndInitialization();
        }
    }
#endif

    void SilhouetteFeatureProcessor::OnEndPrepareRender()
    {
        if (auto scene = GetParentScene(); scene != nullptr)
        {
            if (m_compositePass && m_rasterPass && m_rasterPass->GetRenderPipeline())
            {
                // Get DrawList from the dynamic draw interface and view
                AZStd::vector<RHI::DrawListView> drawLists = AZ::RPI::DynamicDrawInterface::Get()->GetDrawListsForPass(m_rasterPass);
                const AZStd::vector<AZ::RPI::ViewPtr>& views = m_rasterPass->GetRenderPipeline()->GetViews(m_rasterPass->GetPipelineViewTag());
                RHI::DrawListView viewDrawList;
                if (!views.empty())
                {
                    const AZ::RPI::ViewPtr& view = views.front();
                    viewDrawList = view->GetDrawList(m_rasterPass->GetDrawListTag());
                }

                if (drawLists.empty() && viewDrawList.empty())
                {
                    m_compositePass->SetEnabled(false);
                }
                else
                {
                    m_compositePass->SetEnabled(true);
                }
            }
        }
    }

    void SilhouetteFeatureProcessor::AddRenderPasses(RPI::RenderPipeline* renderPipeline)
    {
        // Early return if pass is already found in render pipeline or if the pipeline is not the default one.
        if (renderPipeline->GetViewType() != RPI::ViewType::Default)
        {
            return;
        }

        // the silhouette passes are already added in another render pipeline
        if (m_renderPipeline && m_renderPipeline != renderPipeline)
        {
            return;
        }

        UpdatePasses(renderPipeline);

        // if we already have valid render pass members then we don't need to dynamically
        // create them and add them to the pipeline
        if (m_compositePass && m_rasterPass)
        {
            return;
        }

        // unset the render pipeline until we add the passes successfully 
        m_renderPipeline = nullptr;

        const auto mergeTemplateName = Name("SilhouettePassTemplate");
        const auto gatherTemplateName = Name("SilhouetteGatherPassTemplate");

        Name postProcessPassName = Name("PostProcessPass");
        if (renderPipeline->FindFirstPass(postProcessPassName) == nullptr)
        {
            AZ_Warning("SilhouetteFeatureProcessor", false, "Can't find %s in the render pipeline.", postProcessPassName.GetCStr());
            return;
        }

#if !defined(CARBONATED)
        Name forwardProcessPassName = Name("Forward");
        if (renderPipeline->FindFirstPass(forwardProcessPassName) == nullptr)
        {
            AZ_Warning("SilhouetteFeatureProcessor", false, "Can't find %s in the render pipeline.", forwardProcessPassName.GetCStr());
            return;
        }

        // Add the gather pass which draws all the silhouette objects into a render target
        // using depth and stencil to determine where to draw
        RPI::PassRequest gatherPassRequest;
        gatherPassRequest.m_passName = Name("SilhouetteGatherPass");
        gatherPassRequest.m_templateName = gatherTemplateName;
        gatherPassRequest.AddInputConnection(RPI::PassConnection{
            Name("DepthStencilInputOutput"), RPI::PassAttachmentRef{ forwardProcessPassName, Name("DepthStencilInputOutput") } });

        if (auto pass = RPI::PassSystemInterface::Get()->CreatePassFromRequest(&gatherPassRequest); pass != nullptr)
        {
            m_rasterPass = static_cast<AZ::RPI::RasterPass*>(pass.get());
            renderPipeline->AddPassAfter(pass, forwardProcessPassName);
        }

        // Add the full screen silhouette pass which merges the silhouettes render target with
        // the framebuffer diffuse, and adds outlines to the silhouette shapes
        RPI::PassRequest compositePassRequest;
        compositePassRequest.m_passName = Name("SilhouettePass");
        compositePassRequest.m_templateName = mergeTemplateName;
        compositePassRequest.AddInputConnection(
            RPI::PassConnection{ Name("InputOutput"), RPI::PassAttachmentRef{ postProcessPassName, Name("Output") } });

        if (auto pass = RPI::PassSystemInterface::Get()->CreatePassFromRequest(&compositePassRequest); pass != nullptr)
        {
            m_compositePass = pass.get();
            renderPipeline->AddPassAfter(pass, postProcessPassName);
        }
#else
        // Add the full screen silhouette pass which merges the silhouettes render target with
        // the framebuffer diffuse, and adds outlines to the silhouette shapes
        Name depthPrePassName = Name("DepthPrePass");
        if (renderPipeline->FindFirstPass(depthPrePassName) == nullptr)
        {
            AZ_Warning("SilhouetteFeatureProcessor", false, "Can't find %s in the render pipeline.", depthPrePassName.GetCStr());
            return;
        }

        Name opaquePassName = Name("OpaquePass");
        if (renderPipeline->FindFirstPass(opaquePassName) == nullptr)
        {
            AZ_Warning("SilhouetteFeatureProcessor", false, "Can't find %s in the render pipeline.", opaquePassName.GetCStr());
            return;
        }

        RPI::PassRequest compositePassRequest;
        compositePassRequest.m_passName = Name("SilhouetteParentPass");
        compositePassRequest.m_templateName = Name("SilhouetteParentPassTemplate");
        compositePassRequest.AddInputConnection(
            RPI::PassConnection{ Name("InputOutput"), RPI::PassAttachmentRef{ postProcessPassName, Name("Output") } });
        compositePassRequest.AddInputConnection(
            RPI::PassConnection{ Name("DepthStencilResolvedInputOutput"), RPI::PassAttachmentRef{ depthPrePassName, Name("Depth") } });
        compositePassRequest.AddInputConnection(
            RPI::PassConnection{ Name("DepthStencilInputOutput"), RPI::PassAttachmentRef{ opaquePassName, Name("DepthStencil") } });

        if (auto pass = RPI::PassSystemInterface::Get()->CreatePassFromRequest(&compositePassRequest); pass != nullptr)
        {
            renderPipeline->AddPassAfter(pass, postProcessPassName);
        }
        UpdatePasses(renderPipeline);
#endif

        // remember which render pipeline we added our passes to
        m_renderPipeline = renderPipeline;
    }

    void SilhouetteFeatureProcessor::OnRenderPipelineChanged(AZ::RPI::RenderPipeline* pipeline, AZ::RPI::SceneNotification::RenderPipelineChangeType changeType)
    {
        // Only need to recache silhouette passes if the pipeline had them
        if (pipeline == m_renderPipeline)
        {
            if (changeType == AZ::RPI::SceneNotification::RenderPipelineChangeType::Removed)
            {
                UpdatePasses(nullptr);
            }
            else
            {
                UpdatePasses(pipeline);
            }
        }
    }
    void SilhouetteFeatureProcessor::UpdatePasses(AZ::RPI::RenderPipeline* renderPipeline)
    {
        m_compositePass = nullptr;
        m_rasterPass = nullptr;

        if (renderPipeline == nullptr)
        {
            m_renderPipeline = nullptr;
            return;
        }

#if defined(CARBONATED)
        const auto mergeTemplateName = Name("SilhouetteJFAParentTemplate");
#else
        const auto mergeTemplateName = Name("SilhouettePassTemplate");
#endif
        auto compositePassFilter = AZ::RPI::PassFilter::CreateWithTemplateName(mergeTemplateName, renderPipeline);
        if (auto foundPass = AZ::RPI::PassSystemInterface::Get()->FindFirstPass(compositePassFilter); foundPass)
        {
            m_compositePass = foundPass;
        }

        const auto gatherTemplateName = Name("SilhouetteGatherPassTemplate");
        auto gatherPassFilter = AZ::RPI::PassFilter::CreateWithTemplateName(gatherTemplateName, renderPipeline);
        if (auto foundPass = AZ::RPI::PassSystemInterface::Get()->FindFirstPass(gatherPassFilter); foundPass)
        {
            m_rasterPass = static_cast<AZ::RPI::RasterPass*>(foundPass);
        }

        // remember which render pipeline we found our passes on
        m_renderPipeline = (m_compositePass && m_rasterPass) ? renderPipeline : nullptr;
#if defined(CARBONATED)
        SetPassesEnabled(m_enabled);
#endif
    }
} // namespace AZ::Render
