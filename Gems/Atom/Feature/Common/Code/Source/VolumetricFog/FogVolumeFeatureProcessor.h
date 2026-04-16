/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/Feature/Utils/IndexedDataVector.h>
#include <Atom/Feature/VolumetricFog/FogVolumeFeatureProcessorInterface.h>
#include <Atom/Feature/VolumetricFog/VolumetricFogConstants.h>
#include <RayTracing/RayTracingResourceList.h>

#include <Atom/RHI/DrawPacket.h>
#include <Atom/RHI/DrawPacketBuilder.h>
#include <Atom/RHI.Reflect/ShaderResourceGroupLayoutDescriptor.h>

#include <Atom/RPI.Public/Buffer/Buffer.h>
#include <Atom/RPI.Public/Pass/RasterPass.h>
#include <Atom/RPI.Public/PipelineState.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RPI.Public/Image/StreamingImage.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>

namespace AZ::Render
{
    class FogVolumeFeatureProcessor final
        : public FogVolumeFeatureProcessorInterface
        , protected AZ::Data::AssetBus::MultiHandler
    {
    public:
        AZ_CLASS_ALLOCATOR(FogVolumeFeatureProcessor, AZ::SystemAllocator)
        AZ_RTTI(AZ::Render::FogVolumeFeatureProcessor, "{3E5D9A7B-1C4F-4B8E-A9D2-7F0C3B5E8A1D}", FogVolumeFeatureProcessorInterface)

        static void Reflect(AZ::ReflectContext* context);

        FogVolumeFeatureProcessor() = default;
        ~FogVolumeFeatureProcessor() = default;

        // FeatureProcessor
        void Activate() override;
        void Deactivate() override;
        void Simulate(const SimulatePacket& packet) override;
        void Render(const RenderPacket& packet) override;

        // FogVolumeFeatureProcessorInterface
        VolumeHandle AcquireVolume() override;
        bool ReleaseVolume(VolumeHandle& handle) override;
        void SetVolumeTransform(VolumeHandle handle, const AZ::Transform& transform) override;
        void SetVolumeExtents(VolumeHandle handle, const AZ::Vector3& halfExtents) override;

#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)  \
        void SetVolume##Name(VolumeHandle handle, ValueType val) override;

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>
#include <Atom/Feature/VolumetricFog/FogVolumeParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

    private:
        // CPU-side state per registered volume
        struct VolumeState
        {
            AZ::Transform  m_transform   = AZ::Transform::CreateIdentity();
            AZ::Vector3    m_halfExtents = AZ::Vector3(1.0f);

#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)  \
            ValueType MemberName = DefaultValue;

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>

#undef AZ_GFX_TEXTURE_ASSET_PARAM
#define AZ_GFX_TEXTURE_ASSET_PARAM(Name, MemberName, DefaultValue)  \
            int MemberName##Index = -1;                             \

#include <Atom/Feature/VolumetricFog/FogVolumeParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
        };

        // A contiguous run of volumes sharing (blendMode, shape) after sorting
        struct Batch
        {
            FogVolumeBlendMode m_mode;
            FogVolumeShape     m_shape;
            uint32_t           m_volumeStart;
            uint32_t           m_volumeCount;
            uint32_t           m_instanceStart;
            uint32_t           m_instanceCount;
        };

        // Must match LocalVolumeData in FroxelLocalVolumeCommon.azsli — verified by static_assert in .cpp.
        struct LocalVolumeData
        {
            AZStd::array<float, 3> m_center;
            uint32_t m_shape;
            AZStd::array<float, 3> m_right;
            float m_extentX;
            AZStd::array<float, 3> m_up;
            float m_extentY;
            AZStd::array<float, 3> m_forward;
            float m_extentZ;
            AZStd::array<float, 3> m_rcpFadePos;
            int32_t m_noiseTextureIndex;
            AZStd::array<float, 3> m_rcpFadeNeg;
            uint32_t m_sliceMin = 1;
            uint32_t m_sliceMax = 0;
            uint32_t m_priority;
            uint32_t m_blendMode;
            uint32_t m_pad0;
            VolumetricFogVolumeConstants m_volumeData;
        };

        // RPI::SceneNotification
        void OnRenderPipelineChanged(
            RPI::RenderPipeline* pipeline,
            RPI::SceneNotification::RenderPipelineChangeType changeType) override;

        // Data::AssetBus::MultiHandler
        void OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

        bool LoadShader();
        void BuildProxyMeshBuffers();
        void BuildCubeProxy();
        void BuildSphereProxy();
        void BuildPipelines();

        void ComputeSliceRanges(const RPI::ViewPtr& view, float fogNear, float fogFar, uint32_t sliceCount);
        void SortAndPartitionVolumes();
        void BuildInstanceMappingBuffer();
        void UploadVolumeBuffer();
        void BuildNoiseTextureArray();

        LocalVolumeData BuildVolumeGpuData(const VolumeState& state);
        static float EdgeFadeToRcp(float edgeFraction);

        IndexedDataVector<VolumeState>   m_volumeStates;
        AZStd::vector<LocalVolumeData>   m_currentFrameVolumes;
        AZStd::vector<Batch>             m_batches;
        AZStd::vector<AZStd::pair<uint32_t, uint32_t>> m_instanceMapping;

        Data::Instance<RPI::Buffer> m_volumeBuffer;
        uint32_t                    m_volumeBufferCapacity = 0;

        Data::Instance<RPI::Buffer> m_instanceMappingBuffer;
        uint32_t                    m_instanceMappingCapacity = 0;

        AZStd::array<Data::Instance<RPI::Buffer>, FogVolumeShapeCount - 1> m_vertexBuffers;
        AZStd::array<Data::Instance<RPI::Buffer>, FogVolumeShapeCount - 1> m_indexBuffers;

        RPI::RasterPass*             m_rasterPass = nullptr;
        Data::Instance<RPI::Shader>  m_shader;

        RHI::ShaderInputNameIndex m_verticesBufferIndex = RHI::ShaderInputNameIndex("m_proxyVertices");
        RHI::ShaderInputNameIndex m_indicesBufferIndex = RHI::ShaderInputNameIndex("m_proxyIndices");
        RHI::ShaderInputNameIndex m_volumesBufferIndex = RHI::ShaderInputNameIndex("m_volumes");
        RHI::ShaderInputNameIndex m_instanceMappingBufferIndex = RHI::ShaderInputNameIndex("m_instanceToVolumeSlice");
        RHI::ShaderInputImageUnboundedArrayIndex m_noiseTextureArrayIndex;

        AZStd::array<RPI::Ptr<RPI::PipelineStateForDraw>, FogVolumeBlendModeCount> m_pipelineStates;

        AZStd::vector<AZ::RHI::ConstPtr<RHI::DrawPacket>> m_drawPackets;

        RHI::DrawListTag m_drawListTag;

        RayTracingResourceList<AZ::Data::AssetId> m_noiseImageAssetsId;
        AZStd::vector<Data::Instance<RPI::StreamingImage>> m_noiseImageImages;
    };
} // namespace AZ::Render

namespace AZ::Data
{
    inline bool operator!(const AssetId& assetId)
    {
        return !assetId.IsValid();
    }
}
