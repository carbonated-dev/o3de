/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <VolumetricFog/FogVolumeFeatureProcessor.h>
#include <VolumetricFog/VolumetricFogUtils.h>
#include <Atom/Feature/VolumetricFog/VolumetricFogFeatureProcessorInterface.h>

#include <Atom/RHI/DrawItem.h>
#include <Atom/RHI/DrawPacketBuilder.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/RHISystemInterface.h>

#include <Atom/RPI.Public/Buffer/BufferSystemInterface.h>
#include <Atom/RPI.Public/Pass/PassFilter.h>
#include <Atom/RPI.Public/Pass/PassSystemInterface.h>
#include <Atom/RPI.Public/Pass/PassUtils.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Public/View.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>
#include <Atom/RPI.Reflect/Model/ModelLodAsset.h>
#include <Atom/RPI.Public/RPIUtils.h>

#include <AzCore/Math/Frustum.h>
#include <AzCore/Math/Aabb.h>
#include <AzCore/Math/Obb.h>
#include <AzCore/Math/ShapeIntersection.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Jobs/JobCompletion.h>
#include <AzCore/Jobs/JobFunction.h>

namespace AZ::Render
{
    namespace
    {
        constexpr float CUBE_VERTICES[8 * 3] = {
            -1, -1, -1,   +1, -1, -1,   +1, +1, -1,   -1, +1, -1,
            -1, -1, +1,   +1, -1, +1,   +1, +1, +1,   -1, +1, +1,
        };
        constexpr uint32_t CUBE_INDICES[36] = {
            0, 2, 1,  0, 3, 2,   4, 5, 6,  4, 6, 7,
            0, 1, 5,  0, 5, 4,   2, 3, 7,  2, 7, 6,
            0, 4, 7,  0, 7, 3,   1, 2, 6,  1, 6, 5,
        };

        struct SliceRange { uint32_t min; uint32_t max; };

        SliceRange ComputeSliceRange(const AZ::Aabb& worldAabb,
                                     const AZ::Transform& viewMatrix,
                                     float fogNear, float fogFar, uint32_t sliceCount)
        {
            const AZ::Vector3 mn = worldAabb.GetMin();
            const AZ::Vector3 mx = worldAabb.GetMax();
            const AZ::Vector3 corners[8] = {
                {mn.GetX(), mn.GetY(), mn.GetZ()}, {mx.GetX(), mn.GetY(), mn.GetZ()},
                {mn.GetX(), mx.GetY(), mn.GetZ()}, {mx.GetX(), mx.GetY(), mn.GetZ()},
                {mn.GetX(), mn.GetY(), mx.GetZ()}, {mx.GetX(), mn.GetY(), mx.GetZ()},
                {mn.GetX(), mx.GetY(), mx.GetZ()}, {mx.GetX(), mx.GetY(), mx.GetZ()},
            };

            const AZ::Vector3 cameraPos     = viewMatrix.GetTranslation();
            const AZ::Vector3 cameraForward = viewMatrix.GetBasisY(); // Y is forward in O3DE (Z-up)

            float minVZ = std::numeric_limits<float>::max();
            float maxVZ = std::numeric_limits<float>::lowest();
            for (const auto& c : corners)
            {
                const float vz = (c - cameraPos).Dot(cameraForward);
                minVZ = AZ::GetMin(minVZ, vz);
                maxVZ = AZ::GetMax(maxVZ, vz);
            }

            minVZ = AZ::GetMax(minVZ, fogNear);
            maxVZ = AZ::GetMin(maxVZ, fogFar);
            if (minVZ >= maxVZ)
            {
                return { 0, 0 };
            }

            const float logRange = std::log(fogFar / fogNear);
            auto viewZToSlice = [&](float vz) {
                return sliceCount * std::log(AZ::GetMax(vz, fogNear) / fogNear) / logRange;
            };

            const uint32_t sliceMin = AZ::GetClamp(
                static_cast<int32_t>(std::floor(viewZToSlice(minVZ))),
                0, static_cast<int32_t>(sliceCount) - 1);
            const uint32_t sliceMax = AZ::GetClamp(
                static_cast<int32_t>(std::ceil(viewZToSlice(maxVZ))),
                0, static_cast<int32_t>(sliceCount) - 1);
            return { sliceMin, sliceMax };
        }

        RHI::TargetBlendState GetBlendState(FogVolumeBlendMode mode)
        {
            RHI::TargetBlendState blend;
            blend.m_enable = true;

            switch (mode)
            {
            case FogVolumeBlendMode::Additive:
                blend.m_blendOp         = RHI::BlendOp::Add;
                blend.m_blendSource     = RHI::BlendFactor::One;
                blend.m_blendDest       = RHI::BlendFactor::One;
                blend.m_blendAlphaOp    = RHI::BlendOp::Add;
                blend.m_blendAlphaSource = RHI::BlendFactor::One;
                blend.m_blendAlphaDest  = RHI::BlendFactor::One;
                break;
            case FogVolumeBlendMode::Multiply:
                blend.m_blendOp         = RHI::BlendOp::Add;
                blend.m_blendSource     = RHI::BlendFactor::ColorDest;
                blend.m_blendDest       = RHI::BlendFactor::Zero;
                blend.m_blendAlphaOp    = RHI::BlendOp::Add;
                blend.m_blendAlphaSource = RHI::BlendFactor::AlphaDest;
                blend.m_blendAlphaDest  = RHI::BlendFactor::Zero;
                break;
            case FogVolumeBlendMode::Overwrite:
                blend.m_blendOp         = RHI::BlendOp::Add;
                blend.m_blendSource     = RHI::BlendFactor::One;
                blend.m_blendDest       = RHI::BlendFactor::Zero;
                blend.m_blendAlphaOp    = RHI::BlendOp::Add;
                blend.m_blendAlphaSource = RHI::BlendFactor::One;
                blend.m_blendAlphaDest  = RHI::BlendFactor::Zero;
                break;
            case FogVolumeBlendMode::Min:
                blend.m_blendOp         = RHI::BlendOp::Minimum;
                blend.m_blendSource     = RHI::BlendFactor::One;
                blend.m_blendDest       = RHI::BlendFactor::One;
                blend.m_blendAlphaOp    = RHI::BlendOp::Minimum;
                blend.m_blendAlphaSource = RHI::BlendFactor::One;
                blend.m_blendAlphaDest  = RHI::BlendFactor::One;
                break;
            case FogVolumeBlendMode::Max:
                blend.m_blendOp         = RHI::BlendOp::Maximum;
                blend.m_blendSource     = RHI::BlendFactor::One;
                blend.m_blendDest       = RHI::BlendFactor::One;
                blend.m_blendAlphaOp    = RHI::BlendOp::Maximum;
                blend.m_blendAlphaSource = RHI::BlendFactor::One;
                blend.m_blendAlphaDest  = RHI::BlendFactor::One;
                break;
            default:
                AZ_Assert(false, "Unknown FogVolumeBlendMode");
                break;
            }
            return blend;
        }
    } // anonymous namespace

    void FogVolumeFeatureProcessor::Reflect(ReflectContext* context)
    {
        if (auto* sc = azrtti_cast<SerializeContext*>(context))
        {
            sc->Class<FogVolumeFeatureProcessor, FeatureProcessor>()
                ->Version(0);
        }
    }

    void FogVolumeFeatureProcessor::Activate()
    {
        auto* rhiSystem = RHI::RHISystemInterface::Get();
        m_drawListTag = rhiSystem->GetDrawListTagRegistry()->AcquireTag(AZ::Name("froxelLocalVolume"));

        // Build the proxy mesh GPU buffers once at activation; SRG bindings happen
        // in OnRenderPipelineChanged() after we locate the RasterPass.
        BuildProxyMeshBuffers();

        EnableSceneNotification();
    }

    void FogVolumeFeatureProcessor::Deactivate()
    {
        DisableSceneNotification();
        Data::AssetBus::MultiHandler::BusDisconnect();

        m_noiseImageImages.clear();
        m_noiseImageAssetsId.Reset();
        m_rasterPass = nullptr;
        m_drawPackets.clear();
        m_pipelineStates.fill(nullptr);
        m_shader = nullptr;
        m_vertexBuffers.fill(nullptr);
        m_indexBuffers.fill(nullptr);
        m_volumeBuffer = nullptr;
        m_instanceMappingBuffer = nullptr;
        m_volumeBufferCapacity = 0;
        m_instanceMappingCapacity = 0;
    }

    void FogVolumeFeatureProcessor::Simulate([[maybe_unused]] const SimulatePacket& packet)
    {
    }

    void FogVolumeFeatureProcessor::Render(const RenderPacket& packet)
    {
        if (m_volumeStates.GetDataCount() == 0 || !m_rasterPass || !m_shader)
        {
            return;
        }

        RPI::Pass* pass = m_rasterPass;
        while (!pass)
        {
            if (!pass->IsEnabled())
            {
                return;
            }
            pass = pass->GetParent();
        }

        // Find the first view with the froxelLocalVolume draw list tag to use for slice computation.
        RPI::ViewPtr computeView;
        for (const auto& view : packet.m_views)
        {
            if (view->HasDrawListTag(m_drawListTag))
            {
                computeView = view;
                break;
            }
        }
        if (!computeView)
        {
            return;
        }

        // Get global fog settings needed for froxel slice computation.
        auto* vfFP = GetParentScene()->GetFeatureProcessor<VolumetricFogFeatureProcessorInterface>();
        if (!vfFP)
        {
            return;
        }
        const float fogNear    = vfFP->GetFogStartDistance();
        const float fogFar     = vfFP->GetFogEndDistance();
        const uint32_t sliceCount = VolumetricFog::ToFroxelSize(vfFP->GetFogQuality()).m_depth;

        // Build this frame's packed GPU data from the current CPU state.
        ComputeSliceRanges(computeView, fogNear, fogFar, sliceCount);

        // Sort volumes and partition into batches, then build instance-mapping.
        SortAndPartitionVolumes();
        BuildInstanceMappingBuffer();
        BuildNoiseTextureArray();
        UploadVolumeBuffer();

        // Push all buffers into the pass SRG. The pass compiles the SRG itself in
        // CompileResources(), so we must not call Compile() here.
        RPI::ShaderResourceGroup* passSrg = m_rasterPass->GetShaderResourceGroup();
        passSrg->SetBufferArray(m_verticesBufferIndex, m_vertexBuffers);
        passSrg->SetBufferArray(m_indicesBufferIndex, m_indexBuffers);
        passSrg->SetBuffer(m_volumesBufferIndex,        m_volumeBuffer);
        passSrg->SetBuffer(m_instanceMappingBufferIndex, m_instanceMappingBuffer);

        AZStd::vector<const RHI::ImageView*> rhiImages;
        rhiImages.reserve(m_noiseImageImages.size());
        AZStd::transform(
            m_noiseImageImages.begin(),
            m_noiseImageImages.end(),
            AZStd::back_inserter(rhiImages),
            [](const Data::Instance<RPI::StreamingImage>& rpiImage)
            {
                return rpiImage->GetImageView();
            });
        passSrg->SetImageViewUnboundedArray(m_noiseTextureArrayIndex, rhiImages);

        // Build one draw packet per non-empty batch. The pass SRG is bound at the
        // pass level by the pass infrastructure, so draw packets carry no SRG.
        m_drawPackets.clear();
        for (const auto& batch : m_batches)
        {
            if (batch.m_instanceCount == 0)
            {
                continue;
            }

            RHI::DrawLinear drawArgs;
            drawArgs.m_vertexCount = m_indexBuffers[static_cast<uint32_t>(batch.m_shape)]->GetBufferViewDescriptor().m_elementCount;
            drawArgs.m_instanceCount  = batch.m_instanceCount;
            drawArgs.m_vertexOffset   = 0;
            drawArgs.m_instanceOffset = batch.m_instanceStart;

            RHI::DrawPacketBuilder builder;
            builder.Begin(nullptr);
            builder.SetDrawArguments(RHI::DrawArguments(drawArgs));

            RHI::DrawPacketBuilder::DrawRequest request;
            request.m_listTag       = m_drawListTag;
            request.m_pipelineState = m_pipelineStates[static_cast<size_t>(batch.m_mode)]->GetRHIPipelineState();
            builder.AddDrawItem(request);

            m_drawPackets.push_back(builder.End());
        }

        if (m_drawPackets.empty())
        {
            return;
        }

        // Submit draw packets to every view that participates in the froxelLocalVolume draw list.
        for (const auto& view : packet.m_views)
        {
            if (!view->HasDrawListTag(m_drawListTag))
            {
                continue;
            }
            for (const auto& pkt : m_drawPackets)
            {
                view->AddDrawPacket(pkt.get(), 0.0f);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Volume registration
    // -------------------------------------------------------------------------

    FogVolumeFeatureProcessor::VolumeHandle FogVolumeFeatureProcessor::AcquireVolume()
    {
        const uint16_t rawIndex = m_volumeStates.GetFreeSlotIndex();
        m_volumeStates.GetData(rawIndex) = VolumeState{};
        return VolumeHandle(rawIndex);
    }

    bool FogVolumeFeatureProcessor::ReleaseVolume(VolumeHandle& handle)
    {
        if (!handle.IsValid())
        {
            return false;
        }
        SetVolumeNoiseTexture(handle, {});
        m_volumeStates.RemoveIndex(handle.GetIndex());
        handle.Reset();
        return true;
    }

    void FogVolumeFeatureProcessor::SetVolumeTransform(VolumeHandle handle, const AZ::Transform& transform)
    {
        m_volumeStates.GetData(handle.GetIndex()).m_transform = transform;
    }

    void FogVolumeFeatureProcessor::SetVolumeExtents(VolumeHandle handle, const AZ::Vector3& halfExtents)
    {
        m_volumeStates.GetData(handle.GetIndex()).m_halfExtents = halfExtents;
    }

#define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)           \
    void FogVolumeFeatureProcessor::SetVolume##Name(VolumeHandle handle, ValueType val) \
    {                                                                            \
        m_volumeStates.GetData(handle.GetIndex()).MemberName = val;              \
    }

#include <Atom/Feature/ParamMacros/MapParamCommon.inl>

#undef AZ_GFX_TEXTURE_ASSET_PARAM
#define AZ_GFX_TEXTURE_ASSET_PARAM(Name, MemberName, DefaultValue)

#include <Atom/Feature/VolumetricFog/FogVolumeParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

    void FogVolumeFeatureProcessor::SetVolumeNoiseTexture(
        VolumeHandle handle, Data::Asset<AZ::RPI::StreamingImageAsset> val)
    {
        auto& state = m_volumeStates.GetData(handle.GetIndex());
        const auto& indirectionList = m_noiseImageAssetsId.GetIndirectionList();
        if (state.m_noiseTextureIndex >= 0 && state.m_noiseTextureIndex < static_cast<int>(indirectionList.size()))
        {
            uint32_t resourceIndex = indirectionList[state.m_noiseTextureIndex];
            const auto& assetId = m_noiseImageAssetsId.GetResourceList()[resourceIndex];
            if (assetId == val.GetId())
            {
                return;
            }

            m_noiseImageAssetsId.RemoveResource(assetId);
        }

        int index = -1;
        if (val.GetId().IsValid())
        {
            index = static_cast<int>(m_noiseImageAssetsId.AddResource(val.GetId()));
            if (!val.IsReady() && !val.IsLoading())
            {
                val.QueueLoad();
            }

        }
        state.m_noiseTextureIndex = index;

    }

    // -------------------------------------------------------------------------
    // Pipeline change notification
    // -------------------------------------------------------------------------

    void FogVolumeFeatureProcessor::OnRenderPipelineChanged(
        RPI::RenderPipeline* pipeline,
        RPI::SceneNotification::RenderPipelineChangeType changeType)
    {
        if (changeType == RPI::SceneNotification::RenderPipelineChangeType::Added ||
            changeType == RPI::SceneNotification::RenderPipelineChangeType::PassChanged)
        {
            auto passFilter = RPI::PassFilter::CreateWithTemplateName(
                AZ::Name("FroxelLocalVolumeTemplate"), pipeline);
            if (auto* foundPass = RPI::PassSystemInterface::Get()->FindFirstPass(passFilter); foundPass)
            {
                m_rasterPass = static_cast<RPI::RasterPass*>(foundPass);
                m_noiseTextureArrayIndex =
                    m_rasterPass->GetShaderResourceGroup()->FindShaderInputImageUnboundedArrayIndex(Name("m_noiseTextures"));

            }
            else
            {
                m_rasterPass = nullptr;
            }

            if (m_rasterPass && LoadShader())
            {
                BuildPipelines();
            }
        }
        else if (changeType == RPI::SceneNotification::RenderPipelineChangeType::Removed)
        {
            m_rasterPass = nullptr;
            m_shader = nullptr;
            m_pipelineStates.fill(nullptr);
        }
    }

    // -------------------------------------------------------------------------
    // Shader reload
    // -------------------------------------------------------------------------

    void FogVolumeFeatureProcessor::OnAssetReloaded([[maybe_unused]] AZ::Data::Asset<AZ::Data::AssetData> asset)
    {
        Data::AssetBus::MultiHandler::BusDisconnect();
        if (LoadShader())
        {
            BuildPipelines();
        }
    }

    // -------------------------------------------------------------------------
    // Initialisation helpers
    // -------------------------------------------------------------------------

    bool FogVolumeFeatureProcessor::LoadShader()
    {
        m_shader = RPI::LoadCriticalShader("Shaders/VolumetricFog/FroxelLocalVolume.azshader");
        if (!m_shader)
        {
            AZ_Error("FogVolumeFeatureProcessor", false, "Failed to create Shader instance.");
            return false;
        }

        Data::AssetBus::MultiHandler::BusConnect(m_shader->GetAssetId());
        return true;
    }

    void FogVolumeFeatureProcessor::BuildProxyMeshBuffers()
    {
        BuildCubeProxy();
        BuildSphereProxy();
    }

    void FogVolumeFeatureProcessor::BuildCubeProxy()
    {
        auto* bufferSystem = RPI::BufferSystemInterface::Get();
        constexpr uint32_t boxIndex = static_cast<uint32_t>(FogVolumeShape::Box);
        RPI::CommonBufferDescriptor desc;
        desc.m_poolType   = RPI::CommonBufferPoolType::ReadOnly;
        desc.m_bufferName = "FogVolumeCubeVerts";
        desc.m_byteCount  = sizeof(CUBE_VERTICES);
        desc.m_elementSize = sizeof(float) * 3;
        desc.m_bufferData = CUBE_VERTICES;
        desc.m_elementFormat = RHI::Format::R32G32B32_FLOAT;
        m_vertexBuffers[boxIndex] = bufferSystem->CreateBufferFromCommonPool(desc);

        desc.m_bufferName = "FogVolumeCubeIndices";
        desc.m_byteCount  = sizeof(CUBE_INDICES);
        desc.m_elementSize = sizeof(uint32_t);
        desc.m_bufferData = CUBE_INDICES;
        desc.m_elementFormat = RHI::Format::R32_UINT;
        m_indexBuffers[boxIndex] = bufferSystem->CreateBufferFromCommonPool(desc);
    }

    void FogVolumeFeatureProcessor::BuildSphereProxy()
    {
        uint32_t numRings = 25;
        uint32_t numSections = 25;

        const float radius = 1.0f;

        // calculate "inner" vertices
        float sectionAngle(DegToRad(360.0f / static_cast<float>(numSections)));
        float ringSlice(DegToRad(180.0f / static_cast<float>(numRings)));

        uint32_t numberOfPoles = 2;
        // calc required number of vertices/indices/triangles to build a sphere for the given parameters
        uint32_t numVertices = (numRings - 1) * numSections + numberOfPoles;

        using PosType = AZStd::array<float, 3>;
        // setup buffers
        AZStd::vector<PosType> positions;
        positions.clear();
        positions.reserve(numVertices);

        // 1st pole vertex (top, along +Z. Up axis in O3DE)
        positions.push_back(PosType{ 0.0f, 0.0f, radius });

        for (uint32_t ring = 1; ring < numRings - numberOfPoles + 2; ++ring)
        {
            float w(sinf(ring * ringSlice));
            for (uint32_t section = 0; section < numSections; ++section)
            {
                float x = radius * cosf(section * sectionAngle) * w;
                float y = radius * sinf(section * sectionAngle) * w;
                float z = radius * cosf(ring * ringSlice);
                PosType radialVector{ x, y, z };
                positions.push_back(radialVector);
            }
        }

        // 2nd vertex of pole (bottom, along -Z)
        positions.push_back(PosType{ 0.0f, 0.0f, -radius });     

        // NumTriangles = NumTrianglesAtPoles + NumQuads * 2
        //              = (numSections * 2) + ((numRings - 2) * numSections * 2)
        //              = (numSections * 2) * (numRings - 2 + 1)
        const uint32_t numTriangles = (numRings - 1) * numSections * 2;
        const uint32_t numTriangleIndices = numTriangles * 3;

        // build "inner" faces
        AZStd::vector<uint32_t> indices;
        indices.clear();
        indices.reserve(numTriangleIndices);

        for (uint32_t ring = 0; ring < numRings - numberOfPoles; ++ring)
        {
            uint32_t firstVertOfThisRing = 1 + ring * numSections;
            uint32_t firstVertOfNextRing = firstVertOfThisRing + numSections;

            for (uint32_t section = 0; section < numSections; ++section)
            {
                uint32_t nextSection = (section + 1) % numSections;
                indices.push_back(static_cast<uint16_t>(firstVertOfThisRing + section));
                indices.push_back(static_cast<uint16_t>(firstVertOfThisRing + nextSection));
                indices.push_back(static_cast<uint16_t>(firstVertOfNextRing + nextSection));

                indices.push_back(static_cast<uint16_t>(firstVertOfNextRing + nextSection));
                indices.push_back(static_cast<uint16_t>(firstVertOfNextRing + section));
                indices.push_back(static_cast<uint16_t>(firstVertOfThisRing + section));
            }
        }

        // build faces for end caps (to connect "inner" vertices with poles)
        uint32_t firstPoleVert = 0;
        uint32_t firstVertOfFirstRing = 1 + (0) * numSections;
        for (uint32_t section = 0; section < numSections; ++section)
        {
            uint32_t nextSection = (section + 1) % numSections;
            indices.push_back(static_cast<uint16_t>(firstVertOfFirstRing + nextSection));
            indices.push_back(static_cast<uint16_t>(firstVertOfFirstRing + section));
            indices.push_back(static_cast<uint16_t>(firstPoleVert));
        }

        uint32_t lastPoleVert = (numRings - 1) * numSections + 1;
        uint32_t firstVertOfLastRing = 1 + (numRings - 2) * numSections;
        for (uint32_t section = 0; section < numSections; ++section)
        {
            uint32_t nextSection = (section + 1) % numSections;
            indices.push_back(static_cast<uint16_t>(firstVertOfLastRing + section));
            indices.push_back(static_cast<uint16_t>(firstVertOfLastRing + nextSection));
            indices.push_back(static_cast<uint16_t>(lastPoleVert));
        }

        auto* bufferSystem = RPI::BufferSystemInterface::Get();
        constexpr uint32_t sphereIndex = static_cast<uint32_t>(FogVolumeShape::Sphere);
        RPI::CommonBufferDescriptor desc;
        desc.m_poolType = RPI::CommonBufferPoolType::ReadOnly;
        desc.m_bufferName = "FogVolumeSphereVerts";
        desc.m_elementSize = sizeof(PosType);
        desc.m_byteCount = positions.size() * desc.m_elementSize;
        desc.m_bufferData = positions.data();
        desc.m_elementFormat = RHI::Format::R32G32B32_FLOAT;
        m_vertexBuffers[sphereIndex] = bufferSystem->CreateBufferFromCommonPool(desc);

        desc.m_bufferName = "FogVolumeSphereIndices";
        desc.m_elementSize = sizeof(uint32_t);
        desc.m_byteCount = indices.size() * desc.m_elementSize;
        desc.m_bufferData = indices.data();
        desc.m_elementFormat = RHI::Format::R32_UINT;
        m_indexBuffers[sphereIndex] = bufferSystem->CreateBufferFromCommonPool(desc);
    }

    void FogVolumeFeatureProcessor::BuildPipelines()
    {
        if (!m_shader || !m_rasterPass)
        {
            return;
        }

        RHI::InputStreamLayout inputStreamLayout;
        inputStreamLayout.SetTopology(RHI::PrimitiveTopology::TriangleList);
        inputStreamLayout.Finalize();

        for (uint32_t i = 0; i < FogVolumeBlendModeCount; ++i)
        {
            auto* pipeline = aznew RPI::PipelineStateForDraw;
            pipeline->Init(m_shader, m_shader->GetDefaultShaderOptions().GetShaderVariantId());
            pipeline->SetOutputFromPass(m_rasterPass);
            // RT0 (FroxelMedium) and RT1 (FroxelEmissive) share the same blend mode so that
            // an Overwrite volume replaces the global emissive, a Multiply volume scales it, etc.
            RHI::TargetBlendState blendState = GetBlendState(static_cast<FogVolumeBlendMode>(i));
            pipeline->RenderStatesOverlay().m_blendState.m_targets[0] = blendState;
            pipeline->RenderStatesOverlay().m_blendState.m_targets[1] = blendState;

            pipeline->InputStreamLayout() = inputStreamLayout;
            pipeline->Finalize();
            m_pipelineStates[i].reset(pipeline);
        }
    }

    // -------------------------------------------------------------------------
    // Per-frame data preparation
    // -------------------------------------------------------------------------

    void FogVolumeFeatureProcessor::ComputeSliceRanges(
        const RPI::ViewPtr& view, float fogNear, float fogFar, uint32_t sliceCount)
    {
        const auto& stateVec = m_volumeStates.GetDataVector();
        const uint32_t volumeCount = static_cast<uint32_t>(stateVec.size());
        if (volumeCount == 0)
        {
            m_currentFrameVolumes.clear();
            return;
        }

        // Split volumes into at most 8 batches so each job handles a contiguous index range.
        const uint32_t numBatches = AZ::GetMax(1u, AZ::GetMin(volumeCount, 8u));
        const uint32_t batchSize = (volumeCount + numBatches - 1) / numBatches;

        m_currentFrameVolumes.resize(stateVec.size());
        AZ::JobCompletion jobCompletion;

        for (uint32_t batchStart = 0; batchStart < volumeCount; batchStart += batchSize)
        {
            const uint32_t batchEnd = AZ::GetMin(batchStart + batchSize, volumeCount);

            const auto jobLambda = [this, batchStart, batchEnd, view, fogNear, fogFar, sliceCount, &stateVec]() -> void
            {
                for (uint32_t i = batchStart; i < batchEnd; ++i)
                {
                    auto& state = stateVec[i];
                    FogVolumeFeatureProcessor::LocalVolumeData& volumeData = m_currentFrameVolumes[i];
                    volumeData = FogVolumeFeatureProcessor::LocalVolumeData();
                    if (state.m_shape != FogVolumeShape::Unknown)
                    {
                        AZ::Aabb localAabb = AZ::Aabb::CreateFromMinMax(-state.m_halfExtents, state.m_halfExtents);
                        AZ::Obb worldObb = localAabb.GetTransformedObb(state.m_transform);

                        const AZ::Frustum viewFrustum = AZ::Frustum::CreateFromMatrixColumnMajor(view->GetWorldToClipMatrix());
                        if (AZ::ShapeIntersection::Overlaps(viewFrustum, worldObb))
                        {
                            volumeData = BuildVolumeGpuData(state);
                            AZ::Vector3 absRight =
                                AZ::Vector3(
                                    std::abs(volumeData.m_right[0]),
                                    std::abs(volumeData.m_right[1]),
                                    std::abs(volumeData.m_right[2])) *
                                volumeData.m_extentX;

                            AZ::Vector3 absUp =
                                AZ::Vector3(
                                    std::abs(volumeData.m_up[0]),
                                    std::abs(volumeData.m_up[1]),
                                    std::abs(volumeData.m_up[2])) *
                                volumeData.m_extentY;

                            AZ::Vector3 absForward =
                                AZ::Vector3(
                                    std::abs(volumeData.m_forward[0]),
                                    std::abs(volumeData.m_forward[1]),
                                    std::abs(volumeData.m_forward[2])) *
                                volumeData.m_extentZ;

                            AZ::Vector3 halfExtents = absRight + absUp + absForward;
                            AZ::Vector3 center = AZ::Vector3::CreateFromFloat3(volumeData.m_center.data());
                            AZ::Aabb worldAabb = AZ::Aabb::CreateFromMinMax(center - halfExtents, center + halfExtents);

                            const auto range = ComputeSliceRange(worldAabb, view->GetCameraTransform(), fogNear, fogFar, sliceCount);
                            volumeData.m_sliceMin = range.min;
                            volumeData.m_sliceMax = range.max;
                        }
                    }
                }
            };

            AZ::Job* job = aznew AZ::JobFunction<decltype(jobLambda)>(jobLambda, true, nullptr); // auto-deletes
            job->SetDependent(&jobCompletion);
            job->Start();
        }

        jobCompletion.StartAndWaitForCompletion();
    }

    void FogVolumeFeatureProcessor::SortAndPartitionVolumes()
    {
        AZStd::sort(m_currentFrameVolumes.begin(), m_currentFrameVolumes.end(),
            [](const LocalVolumeData& a, const LocalVolumeData& b)
            {
                if (a.m_blendMode != b.m_blendMode) return a.m_blendMode < b.m_blendMode;
                if (a.m_shape     != b.m_shape)     return a.m_shape     < b.m_shape;
                return a.m_priority < b.m_priority;
            });

        m_batches.clear();
        if (m_currentFrameVolumes.empty())
        {
            return;
        }

        Batch current{};
        current.m_mode  = static_cast<FogVolumeBlendMode>(m_currentFrameVolumes[0].m_blendMode);
        current.m_shape = static_cast<FogVolumeShape>(m_currentFrameVolumes[0].m_shape);
        current.m_volumeStart = 0;

        for (uint32_t i = 1; i < static_cast<uint32_t>(m_currentFrameVolumes.size()); ++i)
        {
            const auto mode  = static_cast<FogVolumeBlendMode>(m_currentFrameVolumes[i].m_blendMode);
            const auto shape = static_cast<FogVolumeShape>(m_currentFrameVolumes[i].m_shape);
            if (mode != current.m_mode || shape != current.m_shape)
            {
                current.m_volumeCount = i - current.m_volumeStart;
                m_batches.push_back(current);
                current = {};
                current.m_mode  = mode;
                current.m_shape = shape;
                current.m_volumeStart = i;
            }
        }

        current.m_volumeCount = static_cast<uint32_t>(m_currentFrameVolumes.size()) - current.m_volumeStart;
        m_batches.push_back(current);
    }

    void FogVolumeFeatureProcessor::BuildInstanceMappingBuffer()
    {
        m_instanceMapping.clear();
        for (auto& batch : m_batches)
        {
            batch.m_instanceStart = static_cast<uint32_t>(m_instanceMapping.size());
            for (uint32_t index = 0; index < batch.m_volumeCount; ++index)
            {
                const uint32_t globalIndex = batch.m_volumeStart + index;
                const auto& volume = m_currentFrameVolumes[globalIndex];
                if (volume.m_shape == static_cast<uint32_t>(FogVolumeShape::Unknown) ||
                    volume.m_sliceMax < volume.m_sliceMin)
                {
                    continue;
                }

                for (uint32_t slice = volume.m_sliceMin; slice <= volume.m_sliceMax; ++slice)
                {
                    m_instanceMapping.emplace_back(globalIndex, slice);
                }
            }
            batch.m_instanceCount = static_cast<uint32_t>(m_instanceMapping.size()) - batch.m_instanceStart;
        }

        if (m_instanceMappingCapacity < m_instanceMapping.size())
        {
            m_instanceMappingCapacity = RHI::AlignUp(static_cast<uint32_t>(m_instanceMapping.size()), 256u);
            RPI::CommonBufferDescriptor desc;
            desc.m_poolType    = RPI::CommonBufferPoolType::ReadOnly;
            desc.m_bufferName  = "FogVolumeInstanceMapping";
            desc.m_byteCount   = m_instanceMappingCapacity * sizeof(uint32_t) * 2;
            desc.m_elementSize = sizeof(uint32_t) * 2;
            desc.m_elementFormat = RHI::Format::R32G32_UINT;
            m_instanceMappingBuffer = RPI::BufferSystemInterface::Get()->CreateBufferFromCommonPool(desc);
        }
        if (!m_instanceMapping.empty())
        {
            m_instanceMappingBuffer->UpdateData(
                m_instanceMapping.data(),
                m_instanceMapping.size() * sizeof(uint32_t) * 2,
                0);
        }
    }

    void FogVolumeFeatureProcessor::UploadVolumeBuffer()
    {
        if (m_volumeBufferCapacity < m_currentFrameVolumes.size())
        {
            m_volumeBufferCapacity = RHI::AlignUp(static_cast<uint32_t>(m_currentFrameVolumes.size()), 32u);
            RPI::CommonBufferDescriptor desc;
            desc.m_poolType    = RPI::CommonBufferPoolType::ReadOnly;
            desc.m_bufferName  = "FogVolumeData";
            desc.m_byteCount   = m_volumeBufferCapacity * sizeof(LocalVolumeData);
            desc.m_elementSize = sizeof(LocalVolumeData);
            m_volumeBuffer = RPI::BufferSystemInterface::Get()->CreateBufferFromCommonPool(desc);
        }
        if (!m_currentFrameVolumes.empty())
        {
            m_volumeBuffer->UpdateData(
                m_currentFrameVolumes.data(),
                m_currentFrameVolumes.size() * sizeof(LocalVolumeData),
                0);
        }
    }

    void FogVolumeFeatureProcessor::BuildNoiseTextureArray()
    {
        const auto& resourceList = m_noiseImageAssetsId.GetResourceList();
        const auto& indirectionLis = m_noiseImageAssetsId.GetIndirectionList();
        for (auto& volume : m_currentFrameVolumes)
        {
            if (volume.m_noiseTextureIndex >= 0 && volume.m_noiseTextureIndex < indirectionLis.size())
            {
                volume.m_noiseTextureIndex = indirectionLis[volume.m_noiseTextureIndex];
            }
            else
            {
                volume.m_noiseTextureIndex = -1;
            }
        }

        m_noiseImageImages.resize(resourceList.size());
        for (uint32_t i = 0; i < resourceList.size(); ++i)
        {
            m_noiseImageImages[i] = nullptr;
            const AZ::Data::AssetId& assetId = resourceList[i];
            if (assetId.IsValid())
            {
                auto textureAsset = AZ::Data::AssetManager::Instance().GetAsset<AZ::RPI::StreamingImageAsset>(
                    assetId, AZ::Data::AssetLoadBehavior::PreLoad);
                if (textureAsset.IsReady())
                {
                    m_noiseImageImages[i] = RPI::StreamingImage::FindOrCreate(textureAsset);
                    continue;
                }
            }
        }        
    }

    // -------------------------------------------------------------------------
    // GPU data construction
    // -------------------------------------------------------------------------

    float FogVolumeFeatureProcessor::EdgeFadeToRcp(float edgeFraction)
    {
        return (edgeFraction > 1e-5f) ? 1.0f / edgeFraction : 1e6f;
    }

    FogVolumeFeatureProcessor::LocalVolumeData FogVolumeFeatureProcessor::BuildVolumeGpuData(const VolumeState& state)
    {
        LocalVolumeData data{};

        state.m_transform.GetTranslation().StoreToFloat3(data.m_center.data());
        state.m_transform.GetBasisX().GetNormalized().StoreToFloat3(data.m_right.data());
        state.m_transform.GetBasisZ().GetNormalized().StoreToFloat3(data.m_up.data());      // Z is up in O3DE
        state.m_transform.GetBasisY().GetNormalized().StoreToFloat3(data.m_forward.data()); // Y is forward in O3DE

        data.m_extentX = state.m_halfExtents.GetX();
        data.m_extentY = state.m_halfExtents.GetZ(); // paired with m_up = BasisZ
        data.m_extentZ = state.m_halfExtents.GetY(); // paired with m_forward = BasisY

        data.m_shape = static_cast<uint32_t>(state.m_shape);
        data.m_blendMode = static_cast<uint32_t>(state.m_blendMode);

        const float rcpX = EdgeFadeToRcp(state.m_edgeFade.GetX());
        const float rcpY = EdgeFadeToRcp(state.m_edgeFade.GetY());
        const float rcpZ = EdgeFadeToRcp(state.m_edgeFade.GetZ());
        AZ::Vector3(rcpX, rcpY, rcpZ).StoreToFloat3(data.m_rcpFadePos.data());
        data.m_rcpFadeNeg = data.m_rcpFadePos;

        data.m_priority          = state.m_priority;
        data.m_noiseTextureIndex = state.m_noiseTextureIndex;

        data.m_sliceMin = 0;
        data.m_sliceMax = 0;
        data.m_pad0     = 0;

 #define AZ_GFX_COMMON_PARAM(ValueType, Name, MemberName, DefaultValue)          \
        data.m_volumeData.MemberName = state.MemberName;                 \

#include <Atom/Feature/ParamMacros/MapAllCommon.inl>
#undef AZ_GFX_VEC3_PARAM
#define AZ_GFX_VEC3_PARAM(Name, MemberName, DefaultValue)                                                          \
        data.m_volumeData.MemberName = { state.MemberName.m_x, state.MemberName.m_y, state.MemberName.m_z };       \

#include <Atom/Feature/VolumetricFog/VolumetricFogVolumeSRGConstants.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

        return data;
    }
} // namespace AZ::Render
