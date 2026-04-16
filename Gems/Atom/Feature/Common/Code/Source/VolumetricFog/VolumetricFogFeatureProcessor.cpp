/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <VolumetricFog/VolumetricFogFeatureProcessor.h>
#include <VolumetricFog/FroxelPass.h>
#include <VolumetricFog/VolumetricFogUtils.h>

#include <AzCore/Name/NameDictionary.h>
#include <AzCore/Math/Random.h>

#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/Pass/PassSystem.h>
#include <Atom/RPI.Public/Pass/PassFilter.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/View.h>
#include <Atom/RPI.Public/RPIUtils.h>
#include <Atom/RHI/DrawPacketBuilder.h>
#include <Atom/RHI/RHISystemInterface.h>

namespace AZ::Render
{
    void VolumetricFogFeatureProcessor::Reflect(ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext
                ->Class<VolumetricFogFeatureProcessor, FeatureProcessor>()
                ->Version(0);
        }
    }

    //-------------------------------------------
    // Getters / setters macro
#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)                                                                     \
    ValueType VolumetricFogFeatureProcessor::Get##Name() const                                                                                       \
    {                                                                                                                                      \
        return m_settings.MemberName;                                                                                                                 \
    }                                                                                                                                      \
    void VolumetricFogFeatureProcessor::Set##Name(ValueType val)                                                                                     \
    {                                                                                                                                      \
        m_settings.MemberName = val;                                                                                                                  \
        OnSettingsChanged();                                                                                                               \
    }

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>
#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
    //-------------------------------------------
    
    void VolumetricFogFeatureProcessor::Activate()
    {
        m_settings.m_enabled = false;
        m_sceneSrg = GetParentScene()->GetShaderResourceGroup();
        m_shaderConstantsIndex = m_sceneSrg->FindShaderInputConstantIndex(Name("m_volumetricFogData"));
        m_shaderConstantsVolumeIndex = m_sceneSrg->FindShaderInputConstantIndex(Name("m_volumetricFogVolumeData"));

#include <Atom/Feature/ParamMacros/MapParamEmpty.inl>

#undef AZ_GFX_TEXTURE_ASSET_PARAM
#define AZ_GFX_TEXTURE_ASSET_PARAM(_Name, MemberName, DefaultValue)                                 \
            MemberName##SrgIndex = m_sceneSrg->FindShaderInputImageIndex(Name(#MemberName));    \
 
#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

        LoadShaders();

        auto* rhiSystem = RHI::RHISystemInterface::Get();
        for (uint32_t i = 0; i < m_drawListTagNames.size(); ++i)
        {
            m_drawListTags[i] = rhiSystem->GetDrawListTagRegistry()->AcquireTag(m_drawListTagNames[i]);
        }
        EnableSceneNotification();
    }
    
    void VolumetricFogFeatureProcessor::Deactivate()
    {
        DisableSceneNotification();
        m_shaderConstantsIndex.Reset();
        m_shaderConstantsVolumeIndex.Reset();
        #include <Atom/Feature/ParamMacros/MapParamEmpty.inl>

#undef AZ_GFX_TEXTURE_ASSET_PARAM
#define AZ_GFX_TEXTURE_ASSET_PARAM(_Name, MemberName, DefaultValue) \
            MemberName##SrgIndex.Reset();                           \
            MemberName##Image.reset();                              \
 
#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
        ResetShaderResources();
        m_sceneSrg = {};
        m_settings = {};
    }   

    void VolumetricFogFeatureProcessor::AddRenderPasses([[maybe_unused]] RPI::RenderPipeline* renderPipeline)
    {
    }

    void VolumetricFogFeatureProcessor::OnRenderPipelineChanged([[maybe_unused]] RPI::RenderPipeline* pipeline,
        RPI::SceneNotification::RenderPipelineChangeType changeType)
    {
        if (changeType == RPI::SceneNotification::RenderPipelineChangeType::Added
            || changeType == RPI::SceneNotification::RenderPipelineChangeType::PassChanged)
        {
            BuildPipelines();
            UpdatePasses(pipeline);
        }

        if (changeType == RPI::SceneNotification::RenderPipelineChangeType::Removed)
        {
            m_meshPipelineStates.fill(nullptr);
            UpdatePasses(nullptr);
        }
    }

    void VolumetricFogFeatureProcessor::OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        Data::AssetBus::MultiHandler::BusDisconnect();
        if (LoadShaders())
        {
            m_buildDrawPackets = true;
        }
    }
    
    void VolumetricFogFeatureProcessor::Render([[maybe_unused]] const FeatureProcessor::RenderPacket& packet)
    {
        AZ_PROFILE_SCOPE(RPI, "VolumetricFogFeatureProcessor: Render");
        if (!m_settings.m_enabled)
        {
            return;
        }

        auto toDepth = [this](uint32_t index)
        {
            return index == 0 ? m_settings.m_fogNear : m_settings.m_fogFar;
        };

        for (auto& view : packet.m_views)
        {
            for (uint32_t i = 0; i < m_drawListTags.size(); ++i)
            {
                if (!view->HasDrawListTag(m_drawListTags[i]))
                {
                    continue;
                }

                view->AddDrawPacket(m_drawPackets[i].get(), toDepth(i));
            }
        }
    }

    void VolumetricFogFeatureProcessor::Simulate([[maybe_unused]] const FeatureProcessor::SimulatePacket& packet)
    {
        SetPassesEnabled();
        if (m_settings.m_enabled)
        {
            AZ_PROFILE_SCOPE(RPI, "VolumetricFogFeatureProcessor: Simulate");

            if (m_buildDrawPackets)
            {
                BuildPipelines();
            }

            m_sceneSrgGlobalConstants.m_frameIndex++;
            if (m_needUpdate)
            {
                UpdateSceneSrgConstants();
                BuildDrawItems();
                m_needUpdate = false;
            }

            m_sceneSrg->SetConstant(m_shaderConstantsIndex, m_sceneSrgGlobalConstants);
            m_sceneSrg->SetConstant(m_shaderConstantsVolumeIndex, m_sceneSrgVolumeConstants);

#include <Atom/Feature/ParamMacros/MapParamEmpty.inl>

            // The following macro overrides the regular macro defined above, loads an image and bind it
#undef AZ_GFX_TEXTURE_ASSET_PARAM
#define AZ_GFX_TEXTURE_ASSET_PARAM(Name, MemberName, DefaultValue)                                  \
            if (!MemberName##Image)                                                                 \
            {                                                                                       \
                if (m_settings.MemberName.IsReady())                                               \
                {                                                                                   \
                    MemberName##Image = RPI::StreamingImage::FindOrCreate(m_settings.MemberName);   \
                }                                                                                   \
            }                                                                                       \
            if (MemberName##SrgIndex.IsValid())                                                     \
            {                                                                                       \
                if (!m_sceneSrg->SetImage(MemberName##SrgIndex, MemberName##Image))                 \
                {                                                                                   \
                    AZ_Error(                                                       \
                        "VolumetricFogFeatureProcessor::Simulate",                  \
                        false,                                                      \
                        "Failed to bind SRG image for %s = %s",                     \
                        #MemberName,                                                \
                        m_settings.MemberName.GetHint().c_str());                   \
                }                                                                   \
            }                                                                       \

#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

            if (m_injectPass)
            {
                m_injectPass->SetShaderOption(Name("o_enableNoiseTexture"), m_noiseTextureImage ? Name("true") : Name("false"));
            }
        }
        else
        {
            // Fog disabled — clear the enabled flag so transparent shaders skip fog sampling
            m_sceneSrgGlobalConstants.m_enabled = 0u;
            m_sceneSrg->SetConstant(m_shaderConstantsIndex, m_sceneSrgGlobalConstants);
        }
    }

    const VolumetricFogSettings& VolumetricFogFeatureProcessor::GetSettings() const
    {
        return m_settings;
    }

    void VolumetricFogFeatureProcessor::OnSettingsChanged()
    {
        m_needUpdate = true; // even if disabled, mark it for when it'll become enabled
        if (m_froxelParentPass)
        {
            if (m_settings.m_enabled != m_froxelParentPass->IsEnabled() ||
                VolumetricFog::ToFroxelSize(m_settings.m_quality).m_width != m_sceneSrgGlobalConstants.m_tileSize)
            {
                m_froxelParentPass->QueueForBuildAndInitialization();
            }
        }
    }

    void VolumetricFogFeatureProcessor::UpdatePasses(AZ::RPI::RenderPipeline* renderPipeline)
    {
        m_injectPass = nullptr;
        m_froxelParentPass = nullptr;
        m_froxelCompositePass = nullptr;

        if (renderPipeline == nullptr)
        {
            m_renderPipeline = nullptr;
            return;
        }

        {
            const auto templateName = Name("FroxelInjectTemplate");
            auto passFilter = AZ::RPI::PassFilter::CreateWithTemplateName(templateName, renderPipeline);
            if (auto foundPass = AZ::RPI::PassSystemInterface::Get()->FindFirstPass(passFilter); foundPass)
            {
                m_injectPass = static_cast<FroxelPass*>(foundPass);
            }
        }

        {
            const auto templateName = Name("FroxelParentTemplate");
            auto passFilter = AZ::RPI::PassFilter::CreateWithTemplateName(templateName, renderPipeline);
            if (auto foundPass = AZ::RPI::PassSystemInterface::Get()->FindFirstPass(passFilter); foundPass)
            {
                m_froxelParentPass = static_cast<RPI::ParentPass*>(foundPass);
            }
        }

        {
            const auto templateName = Name("FroxelCompositeTemplate");
            auto passFilter = AZ::RPI::PassFilter::CreateWithTemplateName(templateName, renderPipeline);
            if (auto foundPass = AZ::RPI::PassSystemInterface::Get()->FindFirstPass(passFilter); foundPass)
            {
                m_froxelCompositePass = foundPass;
            }
        }
        

        // remember which render pipeline we found our passes on
        m_renderPipeline = (m_injectPass && m_froxelParentPass && m_froxelCompositePass) ? renderPipeline : nullptr;
    }

    void VolumetricFogFeatureProcessor::UpdateSceneSrgConstants()
    {
        RHI::Size tileSize = VolumetricFog::ToFroxelSize(m_settings.m_quality);
        m_sceneSrgGlobalConstants.m_enabled = static_cast<uint32_t>(m_settings.m_enabled);
        m_sceneSrgGlobalConstants.m_tileSize = tileSize.m_width;
        if (m_froxelParentPass)
        {
            if (auto attachmentBinding = m_froxelParentPass->FindAttachmentBinding(Name("PipelineOutput")))
            {
                if (auto attachment = attachmentBinding->GetAttachment())
                {
                    RHI::Size outputSize = attachment->m_descriptor.m_image.m_size;
                    m_sceneSrgGlobalConstants.m_froxelCount =
                        RHI::Size(
                            uint32_t(ceil(float(outputSize.m_width) * 1.0f / tileSize.m_width)),
                            uint32_t(ceil(float(outputSize.m_height) * 1.0f / tileSize.m_height)),
                            tileSize.m_depth);
                }
            }
        }
        m_sceneSrgGlobalConstants.m_lightingChannelMask = m_settings.m_lightingChannelConfig.GetLightingChannelMask();
        // The coprimes 2, 3 and 5 are commonly used for halton sequences because they have an even distribution even for
        // few samples. With larger primes you need to offset by some amount between each prime to have the same
        // effect. We could allow this to be configurable in the future.
        SetupSubPixelOffsets(2, 3, 5, m_settings.m_sequenceLength);

#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)          \
        m_sceneSrgGlobalConstants.MemberName = m_settings.MemberName;                 \

#include <Atom/Feature/ParamMacros/MapAllCommon.inl>
#undef AZ_GFX_VEC3_PARAM
#define AZ_GFX_VEC3_PARAM(Name, MemberName, DefaultValue)                                                                           \
        m_sceneSrgGlobalConstants.MemberName = { m_settings.MemberName.m_x, m_settings.MemberName.m_y, m_settings.MemberName.m_z };       \

#include <Atom/Feature/VolumetricFog/VolumetricFogSRGConstants.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)          \
        m_sceneSrgVolumeConstants.MemberName = m_settings.MemberName;                 \

#include <Atom/Feature/ParamMacros/MapAllCommon.inl>
#undef AZ_GFX_VEC3_PARAM
#define AZ_GFX_VEC3_PARAM(Name, MemberName, DefaultValue)                                                                           \
        m_sceneSrgVolumeConstants.MemberName = { m_settings.MemberName.m_x, m_settings.MemberName.m_y, m_settings.MemberName.m_z };       \

#include <Atom/Feature/VolumetricFog/VolumetricFogVolumeSRGConstants.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

        // Load all texture resources:
        // first set all macros to be empty, but override the texture for setting images.
#include <Atom/Feature/ParamMacros/MapParamEmpty.inl>

#undef AZ_GFX_TEXTURE_ASSET_PARAM
#define AZ_GFX_TEXTURE_ASSET_PARAM(_Name, MemberName, DefaultValue)                         \
        MemberName##Image = {};                                                             \
        if (!m_settings.MemberName.IsReady() && m_settings.MemberName.GetId().IsValid())    \
        {                                                                                   \
            /*This is a workaround because on certain cases the subId of the StreamingImageAsset doesn't load and it's 0*/  \
            Data::AssetId id(m_settings.MemberName.GetId().m_guid, RPI::StreamingImageAsset::GetImageAssetSubId());         \
            m_settings.MemberName = AZ::Data::AssetManager::Instance().GetAsset<RPI::StreamingImageAsset>(                  \
                id, AZ::Data::AssetLoadBehavior::PreLoad);                                                                  \
        }                                                                                                                   \

#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
        
        m_sceneSrgGlobalConstants.m_blendPercentage /= 100.0f;
    }

    void VolumetricFogFeatureProcessor::BuildDrawItems()
    {
        m_drawPackets[0] = BuildDrawItem(m_settings.m_fogNear, m_drawListTags[0], m_meshPipelineStates[0].get());
        m_drawPackets[1] = BuildDrawItem(m_settings.m_fogFar, m_drawListTags[1], m_meshPipelineStates[1].get());
    }

    AZ::RHI::DrawPacket* VolumetricFogFeatureProcessor::BuildDrawItem(
        float depth, RHI::DrawListTag tag, const AZ::RPI::PipelineStateForDraw* pipelineState)
    {
        if (!pipelineState)
        {
            return nullptr;
        }

        AZ::RHI::DrawPacketBuilder drawPacketBuilder;

        drawPacketBuilder.Begin(nullptr);

        // This draw item purposefully does not reference any geometry buffers.
        // Instead it's expected that the extended class uses a vertex shader
        // that generates a full-screen triangle completely from vertex ids.
        RHI::DrawLinear draw = RHI::DrawLinear();
        draw.m_vertexCount = 3;
        drawPacketBuilder.SetDrawArguments(RHI::DrawArguments(draw));

        AZ::RHI::DrawPacketBuilder::DrawRequest drawRequest;
        drawRequest.m_listTag = tag;
        drawRequest.m_pipelineState = pipelineState->GetRHIPipelineState();
        drawPacketBuilder.AddDrawItem(drawRequest);
        drawPacketBuilder.SetRootConstants(AZStd::span<const uint8_t>(reinterpret_cast<uint8_t*>(&depth), sizeof(depth)));
        return drawPacketBuilder.End();
    }

    AZ::RPI::PipelineStateForDraw* VolumetricFogFeatureProcessor::BuildPipeline(
        RHI::DrawListTag tag, const Data::Instance<RPI::Shader>& shader)
    {
        if (!shader)
        {
            return nullptr;
        }

        auto pipeline = aznew AZ::RPI::PipelineStateForDraw;
        pipeline->Init(shader);

        // No streams required
        RHI::InputStreamLayout inputStreamLayout;
        inputStreamLayout.SetTopology(RHI::PrimitiveTopology::TriangleList);
        inputStreamLayout.Finalize();

        pipeline->InputStreamLayout() = inputStreamLayout;
        pipeline->SetOutputFromScene(GetParentScene(), tag);
        pipeline->Finalize();
        return pipeline;
    }

    void VolumetricFogFeatureProcessor::BuildPipelines()
    {
        for (uint32_t i = 0; i < m_meshPipelineStates.size(); ++i)
        {          
            m_meshPipelineStates[i] = BuildPipeline(m_drawListTags[i], m_shaders[i]);
        }
        BuildDrawItems();
        m_buildDrawPackets = false;
    }

    bool VolumetricFogFeatureProcessor::LoadShaders()
    {
        ResetShaderResources();

        constexpr const char* ShaderFilePaths[] = {
            "shaders/volumetricfog/volumetricfogmintransparent.azshader",
            "shaders/volumetricfog/volumetricfogmaxtransparent.azshader",
        };
        for (uint32_t i = 0; i < m_shaders.size(); ++i)
        {
            m_shaders[i] = AZ::RPI::LoadCriticalShader(ShaderFilePaths[i]);

            if (!m_shaders[i])
            {
                AZ_Error("VolumetricFogFeatureProcessor", false, "LoadShader(): Failed to load required Transparent Min/Max shader.");
                return false;
            }

            AZ::Data::AssetBus::MultiHandler::BusConnect(m_shaders[i]->GetAssetId());
        }       

        return true;
    }

    void VolumetricFogFeatureProcessor::ResetShaderResources()
    {
        m_drawPackets.fill(nullptr);
        m_meshPipelineStates.fill(nullptr);
        m_shaders.fill(nullptr);
        Data::AssetBus::MultiHandler::BusDisconnect();
    }

    void VolumetricFogFeatureProcessor::SetupSubPixelOffsets(uint32_t haltonX, uint32_t haltonY, uint32_t haltonZ, uint32_t length)
    {
        HaltonSequence<3> sequence = HaltonSequence<3>({ haltonX, haltonY, haltonZ });
        AZ_Assert(
            length <= VolumetricFogMaxSequenceLength,
            "[VolumetricFogFeatureProcessor] Sequence %d is larger that max allowed %d",
            static_cast<int>(length),
            static_cast<int>(VolumetricFogMaxSequenceLength));
        auto beginIt = AZStd::begin(m_sceneSrgGlobalConstants.m_haltonSequence);
        auto endIt = beginIt + length;
        sequence.FillHaltonSequence(beginIt, endIt);

        AZStd::for_each(
            beginIt,
            endIt,
            [](Offset& offset)
            {
                offset.m_xOffset -= 0.5f;
                offset.m_yOffset -= 0.5f;
                offset.m_zOffset -= 0.5f;
            });
    }

    void VolumetricFogFeatureProcessor::SetPassesEnabled()
    {
        if (m_froxelParentPass)
        {
            m_froxelParentPass->SetEnabled(m_settings.m_enabled);
        }

        if (m_froxelCompositePass)
        {
            m_froxelCompositePass->SetEnabled(m_settings.m_enabled);
        }
    }
}
