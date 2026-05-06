/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Math/Color.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/Math/Vector3.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/RTTI/ReflectContext.h>
#include <AzCore/std/containers/vector.h>

#include <Atom/RHI.Reflect/ShaderResourceGroupLayoutDescriptor.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RPI.Public/Image/StreamingImage.h>
#include <Atom/RPI.Public/PipelineState.h>

#include <Atom/Feature/VolumetricFog/VolumetricFogFeatureProcessorInterface.h>

namespace AZ::Render
{
    class FroxelPass;

    //! Feature processor for global volumetric fog; owns the froxel grid, halton jitter sequence,
    //! scene SRG constants, and the FroxelParent pass hierarchy. It handles enabling/disabling the passes
    //! involved in the Volumetric Fog process. Manages the volumetric fog constant data that the passes access.
    class VolumetricFogFeatureProcessor final
        : public VolumetricFogFeatureProcessorInterface
        , protected AZ::Data::AssetBus::MultiHandler
    {
    public:
        AZ_CLASS_ALLOCATOR(VolumetricFogFeatureProcessor, AZ::SystemAllocator)

        AZ_RTTI(AZ::Render::VolumetricFogFeatureProcessor, "{0A638612-B357-4C41-B1E5-0DD86B8310C6}", AZ::Render::VolumetricFogFeatureProcessorInterface);

        static void Reflect(AZ::ReflectContext* context);

        VolumetricFogFeatureProcessor() = default;
        virtual ~VolumetricFogFeatureProcessor() = default;

        //! FeatureProcessor
        void Activate() override;
        void Deactivate() override;
        void Render(const RenderPacket& packet) override;
        void Simulate(const FeatureProcessor::SimulatePacket& packet) override;

        //! VolumetricFogFeatureProcessorInterface. Global fog settings
        const VolumetricFogSettings& GetSettings() const override;

#include <Atom/Feature/ParamMacros/StartParamFunctionsOverride.inl>
#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

    private:
        //! 16-byte GPU-aligned Halton jitter sample.
        struct Offset
        {
            Offset() = default;

            // Constructor for implicit conversion from array output by HaltonSequence.
            Offset(AZStd::array<float, 3> offsets)
                : m_xOffset(offsets[0])
                , m_yOffset(offsets[1])
                , m_zOffset(offsets[2]){};

            float m_xOffset = 0.0f;
            float m_yOffset = 0.0f;
            float m_zOffset = 0.0f;
            float m_padding0; // explicit pad to 16 bytes for constant buffer alignment.
        };

        RHI::ShaderInputConstantIndex m_shaderConstantsIndex;
        RHI::ShaderInputConstantIndex m_shaderConstantsVolumeIndex;

        //---------------------------------------------------------
        // Generate all Image members instances
        // Set all macros to be empty, but override the texture for defining image members
#include <Atom/Feature/ParamMacros/MapParamEmpty.inl>

        // Macro to replace only the image macro for defining only image resources
#undef AZ_GFX_TEXTURE_ASSET_PARAM
#define AZ_GFX_TEXTURE_ASSET_PARAM(Name, MemberName, DefaultValue)              \
            RHI::ShaderInputImageIndex MemberName##SrgIndex;                    \

#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

#include <Atom/Feature/ParamMacros/MapParamEmpty.inl>

        // Macro to replace only the image macro for defining only image resources
#undef AZ_GFX_TEXTURE_ASSET_PARAM
#define AZ_GFX_TEXTURE_ASSET_PARAM(Name, MemberName, DefaultValue)            \
            Data::Instance<AZ::RPI::StreamingImage> MemberName##Image;        \

#include <Atom/Feature/VolumetricFog/VolumetricFogParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
        //---------------------------------------------------------

        //! CPU mirror of the global fog constants pushed to the scene SRG each frame.
        struct VolumetricFogConstants
        {
            Offset m_haltonSequence[VolumetricFogMaxSequenceLength]; // pre-computed Halton jitter offsets, one per temporal frame.
            RHI::Size m_froxelCount; // froxel grid dimensions (xy = screen tiles, z = depth slices).
            uint32_t m_tileSize = 0; // pixels per froxel tile (e.g. 8 for High quality).
            uint32_t m_frameIndex = 0; // monotonically increasing frame counter, wraps at sequenceLength.
            uint32_t m_lightingChannelMask = 0; // bitmask selecting which lighting layers contribute to in-scattering.
#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue) ValueType MemberName;

#include <Atom/Feature/ParamMacros/MapAllCommon.inl>

#undef AZ_GFX_VEC3_PARAM
#define AZ_GFX_VEC3_PARAM(Name, MemberName, DefaultValue) AZStd::array<float, 3> MemberName;

#include <Atom/Feature/VolumetricFog/VolumetricFogSRGConstants.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

            uint32_t m_enabled = 0; // 0/1 toggle sent to the shader to short-circuit the composite pass.
            float m_padding0 = 0.0f;
        };

        VolumetricFogConstants m_sceneSrgGlobalConstants;
        VolumetricFogVolumeConstants m_sceneSrgVolumeConstants;

        //! RPI::SceneNotificationBus::Handler
        void OnRenderPipelineChanged(AZ::RPI::RenderPipeline* pipeline, RPI::SceneNotification::RenderPipelineChangeType changeType) override;
        // AZ::Data::AssetBus override
        void OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

        void OnSettingsChanged();
        void UpdatePasses(AZ::RPI::RenderPipeline* renderPipeline);
        void UpdateSceneSrgConstants();
        void UpdateFroxelSize();
        //! Creates draw packets for the transparent depth min/max pre-pass that positions one screen-space quad per froxel slice.
        void BuildDrawItems();
        //! Creates draw items for the transparent depth min/max pre-pass that positions one screen-space quad per froxel slice.
        AZ::RHI::DrawPacket* BuildDrawItem(float depth, RHI::DrawListTag tag, const AZ::RPI::PipelineStateForDraw* pipelineState);
        //! Compiles pipeline states for the two draw list tags (depthTransparentMin / depthTransparentMax).
        AZ::RPI::PipelineStateForDraw* BuildPipeline(RHI::DrawListTag tag, const Data::Instance<RPI::Shader>& shader);
        //! Compiles pipeline states for the two draw list tags (depthTransparentMin / depthTransparentMax).
        void BuildPipelines();
        bool LoadShaders();
        void ResetShaderResources();
        //! Generates the Halton low-discrepancy jitter sequence for temporal anti-aliasing.
        void SetupSubPixelOffsets(uint32_t haltonX, uint32_t haltonY, uint32_t haltonZ, uint32_t length);
        //! Enables or disables child passes based on the current Enable setting.
        void SetPassesEnabled();

        VolumetricFogSettings m_settings;

        bool m_needUpdate = true; // set to true when SRG index cache must be rebuilt.
        bool m_buildDrawPackets = false; // set when froxel size changes and draw packets must be rebuilt.
        RPI::ParentPass* m_froxelParentPass = nullptr;
        FroxelPass* m_injectPass = nullptr;
        RPI::Pass* m_froxelCompositePass = nullptr;
        AZ::RPI::RenderPipeline* m_renderPipeline = nullptr;
        Data::Instance<RPI::ShaderResourceGroup> m_sceneSrg;
        AZStd::array<AZ::RHI::DrawListTag, 2> m_drawListTags; // one tag per transparent depth pass (min/max).
        AZStd::array<AZ::Name, 2> m_drawListTagNames = { AZ::Name("depthTransparentMin"), AZ::Name("depthTransparentMax") };
        AZStd::array<AZ::RHI::ConstPtr<AZ::RHI::DrawPacket>,2> m_drawPackets; // pre-built draw packets submitted each Render() call.
        AZStd::array<AZ::RPI::Ptr<AZ::RPI::PipelineStateForDraw>, 2> m_meshPipelineStates;
        AZStd::array<AZ::Data::Instance<AZ::RPI::Shader>, 2> m_shaders;
    };
}
