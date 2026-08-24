/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <VolumetricFog/VolumetricFogFeatureProcessor.h>
#include <VolumetricFog/FroxelPass.h>
#include <VolumetricFog/FroxelIntegratePass.h>
#include <VolumetricFog/VolumetricFogUtils.h>

#include <AzCore/Console/IConsole.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/Math/Random.h>

#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <Atom/RPI.Public/Pass/PassSystem.h>
#include <Atom/RPI.Public/Pass/PassFilter.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/Shader/ShaderSystemInterface.h>
#include <Atom/RPI.Public/View.h>
#include <Atom/RPI.Public/RPIUtils.h>
#include <Atom/RHI/DrawPacketBuilder.h>
#include <Atom/RHI/RHISystemInterface.h>

namespace AZ::Render
{
    AZ_CVAR(
        bool,
        r_enableVolumetricFog,
        true,
        nullptr,
        AZ::ConsoleFunctorFlags::Null,
        "Enable volumetric fog rendering. The Volumetric Fog component must also be enabled.");

    AZ_CVAR(
        uint32_t,
        r_volumetricFogDebugMode,
        0,
        nullptr,
        AZ::ConsoleFunctorFlags::Null,
        "FroxelScatter diagnostics: 0=off, 1=listed lights, 2=contributing lights, 3=rejected lights, "
        "4=shadow evaluations, 5=NVLC overflow, 6=NVLC depth bin, 7=depth-bound slice.");

    AZ_CVAR(
        uint32_t,
        r_volumetricFogLightTypeMask,
        0x3f,
        nullptr,
        AZ::ConsoleFunctorFlags::Null,
        "Volumetric fog light mask: bit 0=directional, 1=ambient, 2=simple point, 3=simple spot, "
        "4=sphere/point, 5=disk. Useful for isolating FroxelScatter costs.");

    AZ_CVAR(
        float,
        r_volumetricFogDebugLightCountScale,
        1.0f / 16.0f,
        nullptr,
        AZ::ConsoleFunctorFlags::Null,
        "Scale applied to FroxelScatter light-count diagnostic heat maps.");

    AZ_CVAR(bool, r_volumetricFogLightLod, true, nullptr, AZ::ConsoleFunctorFlags::Null,
        "Enable projected-size LOD for local volumetric-fog lights.");
    AZ_CVAR(float, r_volumetricFogLightLodFullPixels, 32.0f, nullptr, AZ::ConsoleFunctorFlags::Null,
        "Projected light radius in pixels above which fog uses full local-light detail.");
    AZ_CVAR(float, r_volumetricFogLightLodShadowPixels, 8.0f, nullptr, AZ::ConsoleFunctorFlags::Null,
        "Projected radius below which fog skips local-light shadows and gobos.");
    AZ_CVAR(float, r_volumetricFogLightLodCullPixels, 1.0f, nullptr, AZ::ConsoleFunctorFlags::Null,
        "Intensity-weighted projected radius where local fog lights begin fading out.");
    AZ_CVAR(float, r_volumetricFogLightLodReferenceIntensity, 1000.0f, nullptr, AZ::ConsoleFunctorFlags::Null,
        "Reference intensity in candelas used by volumetric-fog light importance.");
    AZ_CVAR(uint32_t, r_volumetricFogLocalLightQuality, 0, nullptr, AZ::ConsoleFunctorFlags::Null,
        "Local fog-light quality: 0=fast unshadowed point/spot approximations, "
        "1=shadowed approximations, 2=full area geometry and spot gobos.");
    AZ_CVAR(bool, r_volumetricFogLightBudget, true, nullptr, AZ::ConsoleFunctorFlags::Null,
        "Enable a view-distance-scaled cap on contributing local fog lights per froxel.");
    AZ_CVAR(uint32_t, r_volumetricFogMaxLocalLightsNear, 32, nullptr, AZ::ConsoleFunctorFlags::Null,
        "Maximum contributing local fog lights per froxel at the near budget distance.");
    AZ_CVAR(uint32_t, r_volumetricFogMaxLocalLightsFar, 8, nullptr, AZ::ConsoleFunctorFlags::Null,
        "Maximum contributing local fog lights per froxel at and beyond the far budget distance.");
    AZ_CVAR(float, r_volumetricFogLightBudgetNearDistance, 20.0f, nullptr, AZ::ConsoleFunctorFlags::Null,
        "View distance in meters where the local fog-light budget starts decreasing.");
    AZ_CVAR(float, r_volumetricFogLightBudgetFarDistance, 60.0f, nullptr, AZ::ConsoleFunctorFlags::Null,
        "View distance in meters where the far local fog-light budget is reached.");
    AZ_CVAR(bool, r_volumetricFogDepthBounds, true, nullptr, AZ::ConsoleFunctorFlags::Null,
        "Limit volumetric-fog work to the farthest visible depth in each light-culling tile.");
    AZ_CVAR(uint32_t, r_volumetricFogDepthBoundsSliceMargin, 2, nullptr, AZ::ConsoleFunctorFlags::Null,
        "Additional fog slices retained behind the light-culling tile depth for filtering and jitter.");

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

    void VolumetricFogFeatureProcessor::OnAssetReloaded([[maybe_unused]] AZ::Data::Asset<AZ::Data::AssetData> asset)
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
        if (!IsVolumetricFogEnabled() || !m_renderPipeline)
        {
            return;
        }

        // Update the froxel size every frame in case the pipeline output size changed.
        UpdateFroxelSize();
        if (m_scatterPass)
        {
            UpdateScatterPassShaderOptions();
        }
        const Name depthBoundsOptionValue = static_cast<bool>(r_volumetricFogDepthBounds)
            ? Name("true")
            : Name("false");
        if (m_injectPass)
        {
            m_injectPass->SetShaderOption(Name("o_volumetricFog_depth_bounds"), depthBoundsOptionValue);
        }
        if (m_integratePass)
        {
            m_integratePass->SetShaderOption(Name("o_volumetricFog_depth_bounds"), depthBoundsOptionValue);
        }
        m_sceneSrgGlobalConstants.m_debugMode = r_volumetricFogDebugMode;
        m_sceneSrgGlobalConstants.m_lightTypeMask = r_volumetricFogLightTypeMask;
        m_sceneSrgGlobalConstants.m_debugLightCountScale = r_volumetricFogDebugLightCountScale > 0.0f
            ? r_volumetricFogDebugLightCountScale
            : 0.0f;
        const float lightLodCullPixels = static_cast<float>(r_volumetricFogLightLodCullPixels);
        const float lightLodFullPixels = static_cast<float>(r_volumetricFogLightLodFullPixels);
        const float lightLodShadowPixels = static_cast<float>(r_volumetricFogLightLodShadowPixels);
        const float lightLodReferenceIntensity = static_cast<float>(r_volumetricFogLightLodReferenceIntensity);
        m_sceneSrgGlobalConstants.m_lightLodEnabled = static_cast<bool>(r_volumetricFogLightLod) ? 1u : 0u;
        m_sceneSrgGlobalConstants.m_lightLodCullPixels = AZStd::max(lightLodCullPixels, 0.0f);
        m_sceneSrgGlobalConstants.m_lightLodShadowPixels = AZStd::max(lightLodShadowPixels, 0.0f);
        m_sceneSrgGlobalConstants.m_lightLodFullPixels = AZStd::max(
            lightLodFullPixels,
            m_sceneSrgGlobalConstants.m_lightLodShadowPixels + 1.0e-3f);
        m_sceneSrgGlobalConstants.m_lightLodReferenceIntensity = AZStd::max(
            lightLodReferenceIntensity,
            1.0e-3f);
        m_sceneSrgGlobalConstants.m_depthBoundsSliceMargin =
            static_cast<uint32_t>(r_volumetricFogDepthBoundsSliceMargin);
        constexpr uint32_t MaxPackedLocalLightBudget = 0xffffu;
        const bool lightBudgetEnabled = static_cast<bool>(r_volumetricFogLightBudget);
        m_sceneSrgGlobalConstants.m_maxLocalLightsNear = lightBudgetEnabled
            ? AZStd::min(static_cast<uint32_t>(r_volumetricFogMaxLocalLightsNear), MaxPackedLocalLightBudget)
            : MaxPackedLocalLightBudget;
        m_sceneSrgGlobalConstants.m_maxLocalLightsFar = lightBudgetEnabled
            ? AZStd::min(static_cast<uint32_t>(r_volumetricFogMaxLocalLightsFar), MaxPackedLocalLightBudget)
            : MaxPackedLocalLightBudget;
        m_sceneSrgGlobalConstants.m_lightBudgetNearDistance =
            AZStd::max(static_cast<float>(r_volumetricFogLightBudgetNearDistance), 0.0f);
        m_sceneSrgGlobalConstants.m_lightBudgetFarDistance = AZStd::max(
            static_cast<float>(r_volumetricFogLightBudgetFarDistance),
            m_sceneSrgGlobalConstants.m_lightBudgetNearDistance + 1.0e-3f);
        m_sceneSrg->SetConstant(m_shaderConstantsIndex, m_sceneSrgGlobalConstants);
        m_sceneSrg->SetConstant(m_shaderConstantsVolumeIndex, m_sceneSrgVolumeConstants);

        #include <Atom/Feature/ParamMacros/MapParamEmpty.inl>

        // The following macro overrides the regular macro defined above, loads an image and bind it
#undef AZ_GFX_TEXTURE_ASSET_PARAM
#define AZ_GFX_TEXTURE_ASSET_PARAM(Name, MemberName, DefaultValue)                                                                             \
        if (MemberName##SrgIndex.IsValid())                                                                                                    \
        {                                                                                                                                      \
            if (!m_sceneSrg->SetImage(MemberName##SrgIndex, MemberName##Image))                                                                \
            {                                                                                                                                  \
                AZ_Error(                                                                                                                      \
                    "VolumetricFogFeatureProcessor::Simulate",                                                                                 \
                    false,                                                                                                                     \
                    "Failed to bind SRG image for %s = %s",                                                                                    \
                    #MemberName,                                                                                                               \
                    m_settings.MemberName.GetHint().c_str());                                                                                  \
            }                                                                                                                                  \
        }

#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

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
        AZ_PROFILE_SCOPE(RPI, "VolumetricFogFeatureProcessor: Simulate");
        const bool enabled = IsVolumetricFogEnabled();
        if (enabled != m_wasEnabled)
        {
            m_needUpdate = true;
            m_wasEnabled = enabled;
        }

        SetPassesEnabled(enabled);
        if (enabled)
        {
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

#include <Atom/Feature/ParamMacros/MapParamEmpty.inl>

            // The following macro overrides the regular macro defined above, loads an image and bind it
#undef AZ_GFX_TEXTURE_ASSET_PARAM
#define AZ_GFX_TEXTURE_ASSET_PARAM(Name, MemberName, DefaultValue)                                                                         \
        if (!MemberName##Image)                                                                                                                \
        {                                                                                                                                      \
            if (m_settings.MemberName.IsReady())                                                                                               \
            {                                                                                                                                  \
                MemberName##Image = RPI::StreamingImage::FindOrCreate(m_settings.MemberName);                                                  \
            }                                                                                                                                  \
        }                                                                                                                                      \

#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

            if (m_injectPass)
            {
                m_injectPass->SetShaderOption(
                    Name("o_enableNoiseTexture"), m_noiseTextureImage ? Name("true") : Name("false"));
            }

        }
        else
        {
            // Fog disabled. Clear the enabled flag so transparent shaders skip fog sampling
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
            if (IsVolumetricFogEnabled() != m_froxelParentPass->IsEnabled() ||
                VolumetricFog::ToFroxelSize(m_settings.m_quality).m_width != m_sceneSrgGlobalConstants.m_tileSize)
            {
                m_froxelParentPass->QueueForBuildAndInitialization();
                if (m_froxelMaxVisibleSlicePass)
                {
                    m_froxelMaxVisibleSlicePass->QueueForBuildAndInitialization();
                }
            }
        }
    }

    bool VolumetricFogFeatureProcessor::IsVolumetricFogEnabled() const
    {
        return r_enableVolumetricFog && m_settings.m_enabled;
    }

    void VolumetricFogFeatureProcessor::UpdatePasses(AZ::RPI::RenderPipeline* renderPipeline)
    {
        m_injectPass = nullptr;
        m_scatterPass = nullptr;
        m_integratePass = nullptr;
        m_froxelParentPass = nullptr;
        m_froxelMaxVisibleSlicePass = nullptr;
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
            const auto templateName = Name("FroxelScatterTemplate");
            auto passFilter = AZ::RPI::PassFilter::CreateWithTemplateName(templateName, renderPipeline);
            if (auto foundPass = AZ::RPI::PassSystemInterface::Get()->FindFirstPass(passFilter); foundPass)
            {
                m_scatterPass = static_cast<FroxelPass*>(foundPass);
            }
        }

        {
            const auto templateName = Name("FroxelIntegrateTemplate");
            auto passFilter = AZ::RPI::PassFilter::CreateWithTemplateName(templateName, renderPipeline);
            if (auto foundPass = AZ::RPI::PassSystemInterface::Get()->FindFirstPass(passFilter); foundPass)
            {
                m_integratePass = static_cast<FroxelIntegratePass*>(foundPass);
            }
        }

        {
            const auto templateName = Name("FroxelMaxVisibleSliceTemplate");
            auto passFilter = AZ::RPI::PassFilter::CreateWithTemplateName(templateName, renderPipeline);
            m_froxelMaxVisibleSlicePass = AZ::RPI::PassSystemInterface::Get()->FindFirstPass(passFilter);
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
        m_renderPipeline =
            (m_froxelMaxVisibleSlicePass && m_injectPass && m_scatterPass && m_integratePass && m_froxelParentPass &&
                m_froxelCompositePass)
            ? renderPipeline
            : nullptr;
    }

    void VolumetricFogFeatureProcessor::UpdateSceneSrgConstants()
    {
        RHI::Size tileSize = VolumetricFog::ToFroxelSize(m_settings.m_quality);
        m_sceneSrgGlobalConstants.m_enabled = static_cast<uint32_t>(IsVolumetricFogEnabled());
        m_sceneSrgGlobalConstants.m_tileSize = tileSize.m_width;
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
        m_sceneSrgGlobalConstants.MemberName = { m_settings.MemberName.GetX(), m_settings.MemberName.GetY(), m_settings.MemberName.GetZ() };       \

#include <Atom/Feature/VolumetricFog/VolumetricFogSRGConstants.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)          \
        m_sceneSrgVolumeConstants.MemberName = m_settings.MemberName;                 \

#include <Atom/Feature/ParamMacros/MapAllCommon.inl>
#undef AZ_GFX_VEC3_PARAM
#define AZ_GFX_VEC3_PARAM(Name, MemberName, DefaultValue)                                                                           \
        m_sceneSrgVolumeConstants.MemberName = { m_settings.MemberName.GetX(), m_settings.MemberName.GetY(), m_settings.MemberName.GetZ() };       \

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

    void VolumetricFogFeatureProcessor::UpdateFroxelSize()
    {
        if (!m_froxelParentPass)
        {
            return;
        }

        if (auto attachmentBinding = m_froxelParentPass->FindAttachmentBinding(Name("PipelineOutput")))
        {
            if (auto attachment = attachmentBinding->GetAttachment())
            {
                auto outputSize = attachment->m_descriptor.m_image.m_size;
                RHI::Size tileSize = VolumetricFog::ToFroxelSize(m_settings.m_quality);
                m_sceneSrgGlobalConstants.m_froxelCount = RHI::Size(
                    uint32_t(ceil(float(outputSize.m_width) * 1.0f / tileSize.m_width)),
                    uint32_t(ceil(float(outputSize.m_height) * 1.0f / tileSize.m_height)),
                    tileSize.m_depth);

            }
        }
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

    void VolumetricFogFeatureProcessor::UpdateScatterPassShaderOptions()
    {
        const Data::Instance<RPI::Shader> scatterShader = m_scatterPass->GetShader();
        if (!scatterShader)
        {
            return;
        }

        RPI::ShaderOptionGroup shaderOptions = scatterShader->GetDefaultShaderOptions();
        for (const auto& [optionName, optionValue] : RPI::ShaderSystemInterface::Get()->GetGlobalShaderOptions())
        {
            const RPI::ShaderOptionIndex optionIndex = shaderOptions.FindShaderOptionIndex(optionName);
            if (optionIndex.IsValid())
            {
                shaderOptions.SetValue(optionIndex, optionValue);
            }
        }

        ShadowFilterMethod shadowFilterMethod = m_settings.m_shadowFilterMethod;
        const RPI::ShaderOptionValue directionalShadowFilterMethod =
            RPI::ShaderSystemInterface::Get()->GetGlobalShaderOption(Name("o_directional_shadow_filtering_method"));
        const bool directionalLightUsesEsm =
            directionalShadowFilterMethod.IsValid() &&
            (directionalShadowFilterMethod.GetIndex() == aznumeric_cast<uint32_t>(ShadowFilterMethod::Esm) ||
             directionalShadowFilterMethod.GetIndex() == aznumeric_cast<uint32_t>(ShadowFilterMethod::EsmPcf));
        const bool volumetricFogUsesEsm =
            shadowFilterMethod == ShadowFilterMethod::Esm || shadowFilterMethod == ShadowFilterMethod::EsmPcf;
        if (volumetricFogUsesEsm && !directionalLightUsesEsm)
        {
            shadowFilterMethod = ShadowFilterMethod::Pcf;
        }

        ShadowFilterSampleCount filteringSampleCount = ShadowFilterSampleCount::PcfTap16;
        if (m_settings.m_filteringSampleCount <= 4)
        {
            filteringSampleCount = ShadowFilterSampleCount::PcfTap4;
        }
        else if (m_settings.m_filteringSampleCount <= 9)
        {
            filteringSampleCount = ShadowFilterSampleCount::PcfTap9;
        }

        shaderOptions.SetValue(
            Name("o_directional_shadow_filtering_method"),
            RPI::ShaderOptionValue{ aznumeric_cast<uint32_t>(shadowFilterMethod) });
        shaderOptions.SetValue(
            Name("o_directional_shadow_filtering_sample_count"),
            RPI::ShaderOptionValue{ aznumeric_cast<uint32_t>(filteringSampleCount) });
        shaderOptions.SetValue(
            Name("o_volumetricFog_collect_stats"),
            Name(static_cast<uint32_t>(r_volumetricFogDebugMode) != 0 ? "true" : "false"));
        shaderOptions.SetValue(
            Name("o_volumetricFog_depth_bounds"),
            Name(static_cast<bool>(r_volumetricFogDepthBounds) ? "true" : "false"));

        const uint32_t localLightQuality = AZStd::min(static_cast<uint32_t>(r_volumetricFogLocalLightQuality), 2u);
        shaderOptions.SetValue(
            Name("o_volumetricFog_simple_area_lights"),
            Name(localLightQuality < 2 ? "true" : "false"));
        shaderOptions.SetValue(
            Name("o_volumetricFog_spot_gobos"),
            Name(localLightQuality == 2 ? "true" : "false"));
        shaderOptions.SetValue(
            Name("o_volumetricFog_local_light_shadows"),
            Name(localLightQuality == 0 ? "false" : "true"));

        m_scatterPass->SetShaderOptions(AZStd::move(shaderOptions));
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

    void VolumetricFogFeatureProcessor::SetPassesEnabled(bool enabled)
    {
        if (m_froxelMaxVisibleSlicePass)
        {
            m_froxelMaxVisibleSlicePass->SetEnabled(enabled);
        }

        if (m_froxelParentPass)
        {
            m_froxelParentPass->SetEnabled(enabled);
        }

        if (m_froxelCompositePass)
        {
            m_froxelCompositePass->SetEnabled(enabled);
        }
    }
}
