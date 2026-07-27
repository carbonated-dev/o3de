/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RPI.Public/MeshDrawPacket.h>
#include <Atom/RPI.Public/RPIUtils.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RPI.Public/Shader/ShaderSystemInterface.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Reflect/Material/MaterialFunctor.h>
#include <Atom/RHI/DrawPacketBuilder.h>
#include <Atom/RHI/RHISystemInterface.h>
#include <AzCore/Console/Console.h>
#include <Atom/RPI.Public/Shader/ShaderReloadDebugTracker.h>

#if defined(CARBONATED)
#include <Atom/RHI/ConstantsData.h>
#include <AzCore/Memory/MemoryMarker.h>
#if defined(CARBONATED_SHADER_LOADING_TIME)
#include <AzCore/Time/ITime.h>
#endif
#endif

namespace AZ
{
    namespace RPI
    {
#if defined(CARBONATED)
        AZ_CVAR(bool,
            r_forceRootShaderVariantUsage,
            true,
            [](const bool&) { AZ::Interface<AZ::IConsole>::Get()->PerformCommand("MeshFeatureProcessor.ForceRebuildDrawPackets"); },
            ConsoleFunctorFlags::Null,
            "Forces usage of root shader variant in the mesh draw packet level, ignoring any other shader variants that may exist.\
             When using specialization constants for shader options, turn on this CVAR to avoid any lookup (since there's no variants)"
        );
#else
        AZ_CVAR(bool,
            r_forceRootShaderVariantUsage,
            false,
            [](const bool&) { AZ::Interface<AZ::IConsole>::Get()->PerformCommand("MeshFeatureProcessor.ForceRebuildDrawPackets"); },
            ConsoleFunctorFlags::Null,
            "(For Testing) Forces usage of root shader variant in the mesh draw packet level, ignoring any other shader variants that may exist."
        );
#endif

#if defined(CARBONATED)
        AZ_CVAR(
            bool,
            r_meshDrawPacketAsyncPSO,
            true,
            nullptr,
            ConsoleFunctorFlags::NeedsReload,
            "Enables asynchronous compilation of Specialized mesh pipeline states while compatible Fallback states are used. "
            "Changes require a level reload.");

        MeshDrawPacket::PendingPipelineStateBuild::RequestOwner::RequestOwner(
            RHI::PipelineStateBuildRequestPtr request)
            : m_request(AZStd::move(request))
        {
        }

        MeshDrawPacket::PendingPipelineStateBuild::RequestOwner::~RequestOwner()
        {
            if (m_request)
            {
                if (RHI::RHISystemInterface* rhiSystem = RHI::RHISystemInterface::Get())
                {
                    rhiSystem->GetPipelineStateBuildQueue()->Cancel(m_request);
                }
            }
        }

        MeshDrawPacket::PendingPipelineStateBuild::PendingPipelineStateBuild(
            RHI::PipelineStateBuildRequestPtr request,
            uint8_t drawItemIndex,
            HashValue64 descriptorHash)
            : m_requestOwner(AZStd::make_shared<RequestOwner>(AZStd::move(request)))
            , m_drawItemIndex(drawItemIndex)
            , m_descriptorHash(descriptorHash)
        {
        }

        const RHI::PipelineStateBuildRequestPtr& MeshDrawPacket::PendingPipelineStateBuild::GetRequest() const
        {
            return m_requestOwner->m_request;
        }

        void MeshDrawPacket::PendingPipelineStateBuild::ReleaseRequest()
        {
            m_requestOwner.reset();
        }

#endif
        MeshDrawPacket::MeshDrawPacket(
            ModelLod& modelLod,
            size_t modelLodMeshIndex,
            Data::Instance<Material> materialOverride,
            Data::Instance<ShaderResourceGroup> objectSrg,
            const MaterialModelUvOverrideMap& materialModelUvMap
        )
            : m_modelLod(&modelLod)
            , m_modelLodMeshIndex(modelLodMeshIndex)
            , m_objectSrg(objectSrg)
            , m_material(materialOverride)
            , m_materialModelUvMap(materialModelUvMap)
        {
            if (!m_material)
            {
                m_material = GetMesh().m_material;
            }

            // set to all true so no items would be skipped
            m_drawListFilter.set();
        }

#if defined(CARBONATED)
        void MeshDrawPacket::SetPipelineStateBuildGroup(RHI::PipelineStateBuildGroupId groupId)
        {
            if (m_pipelineStateBuildGroupId != groupId)
            {
                m_pendingPipelineStateBuilds.clear();
                m_pipelineStateBuildGroupId = groupId;
                m_needUpdate = true;
            }
        }

        bool MeshDrawPacket::PublishPipelineStateBuildResults(
            const RHI::PipelineStateBuildRequestSet& completedRequests)
        {
            bool specializedPipelineStatePublished = false;

            for (auto buildIt = m_pendingPipelineStateBuilds.begin(); buildIt != m_pendingPipelineStateBuilds.end();)
            {
                const RHI::PipelineStateBuildRequestPtr& request = buildIt->GetRequest();
                if (!completedRequests.contains(request.get()))
                {
                    ++buildIt;
                    continue;
                }

                RHI::ConstPtr<RHI::PipelineState> pipelineState = request->GetPipelineState();
                const bool usesSpecializationConstants = request->UsesSpecializationConstants();
                DrawItemPipelineState* drawItemPipelineState = buildIt->m_drawItemIndex < m_drawItemPipelineStates.size()
                    ? &m_drawItemPipelineStates[buildIt->m_drawItemIndex]
                    : nullptr;
                const HashValue64 expectedDescriptorHash = drawItemPipelineState
                    ? (usesSpecializationConstants
                        ? drawItemPipelineState->m_specializedDescriptorHash
                        : drawItemPipelineState->m_fallbackDescriptorHash)
                    : HashValue64{};
                const bool isCompatible = drawItemPipelineState && buildIt->m_descriptorHash == expectedDescriptorHash;

                if (pipelineState && isCompatible)
                {
                    if (usesSpecializationConstants)
                    {
                        if (m_drawPacket &&
                            buildIt->m_drawItemIndex < m_drawPacket->GetDrawItemCount())
                        {
                            drawItemPipelineState->m_currentPipelineState = pipelineState;
                            m_drawPacket->GetDrawItem(buildIt->m_drawItemIndex)->m_pipelineState = pipelineState.get();
                            specializedPipelineStatePublished = true;
                        }
                    }
                    else
                    {
                        auto fallbackIt = AZStd::find_if(
                            m_fallbackPipelineStates.begin(),
                            m_fallbackPipelineStates.end(),
                            [descriptorHash = buildIt->m_descriptorHash](const PipelineStateReference& entry)
                            {
                                return entry.m_descriptorHash == descriptorHash;
                            });
                        if (fallbackIt != m_fallbackPipelineStates.end())
                        {
                            fallbackIt->m_pipelineState = pipelineState;
                        }
                        else
                        {
                            m_fallbackPipelineStates.push_back({ buildIt->m_descriptorHash, pipelineState });
                        }
                    }
                }

                buildIt->ReleaseRequest();
                buildIt = m_pendingPipelineStateBuilds.erase(buildIt);
            }

            return specializedPipelineStatePublished;
        }

#endif
        Data::Instance<Material> MeshDrawPacket::GetMaterial() const
        {
            return m_material;
        }

        const ModelLod::Mesh& MeshDrawPacket::GetMesh() const
        {
            AZ_Assert(m_modelLodMeshIndex < m_modelLod->GetMeshes().size(), "m_modelLodMeshIndex %zu is out of range %zu", m_modelLodMeshIndex, m_modelLod->GetMeshes().size());
            return m_modelLod->GetMeshes()[m_modelLodMeshIndex];
        }

        void MeshDrawPacket::ForValidShaderOptionName(const Name& shaderOptionName, const AZStd::function<bool(const ShaderCollection::Item&, ShaderOptionIndex)>& callback)
        {
            m_material->ForAllShaderItems(
                [&](const Name&, const ShaderCollection::Item& shaderItem)
                {
                    const ShaderOptionGroupLayout* layout = shaderItem.GetShaderOptions()->GetShaderOptionLayout();
                    ShaderOptionIndex index = layout->FindShaderOptionIndex(shaderOptionName);
                    if (index.IsValid())
                    {
                        bool shouldContinue = callback(shaderItem, index);
                        if (!shouldContinue)
                        {
                            return false;
                        }
                    }
                    return true;
                });
        }

        void MeshDrawPacket::SetStencilRef(uint8_t stencilRef)
        {
            if (m_stencilRef != stencilRef)
            {
                m_needUpdate = true;
                m_stencilRef = stencilRef;
            }
        }

        void MeshDrawPacket::SetSortKey(RHI::DrawItemSortKey sortKey)
        {
            if (m_sortKey != sortKey)
            {
                m_needUpdate = true;
                m_sortKey = sortKey;
            }
        }

        bool MeshDrawPacket::SetShaderOption(const Name& shaderOptionName, ShaderOptionValue value)
        {
            // check if the material owns this option in any of its shaders, if so it can't be set externally
            if (m_material->MaterialOwnsShaderOption(shaderOptionName))
            {
                return false;
            }

            // Try to find an existing option entry in the list
            for (ShaderOptionPair& shaderOptionPair : m_shaderOptions)
            {
                if (shaderOptionPair.first == shaderOptionName)
                {
                    shaderOptionPair.second = value;
                    m_needUpdate = true;
                    return true;
                }
            }

            // Shader option isn't on the list, look to see if it's even valid for at least one shader item, and if so, add it.
            ForValidShaderOptionName(shaderOptionName,
                [&]([[maybe_unused]] const ShaderCollection::Item& shaderItem, [[maybe_unused]] ShaderOptionIndex index)
                {
                    // Store the option name and value, they will be used in DoUpdate() to select the appropriate shader variant
                    m_shaderOptions.push_back({ shaderOptionName, value });
                    return false; // stop checking other shader items.
                }
            );

            m_needUpdate = true;
            return true;
        }

        bool MeshDrawPacket::UnsetShaderOption(const Name& shaderOptionName)
        {
            // try to find an existing option entry in the list, then remove it by swapping it with the back.
            for (ShaderOptionPair& shaderOptionPair : m_shaderOptions)
            {
                if (shaderOptionPair.first == shaderOptionName)
                {
                    shaderOptionPair = m_shaderOptions.back();
                    m_shaderOptions.pop_back();
                    m_needUpdate = true;
                    return true;
                }
            }
            return false;
        }

        void MeshDrawPacket::ClearShaderOptions()
        {
            m_needUpdate = m_shaderOptions.size() > 0;
            m_shaderOptions.clear();
        }

        void MeshDrawPacket::SetEnableDraw(RHI::DrawListTag drawListTag, bool enableDraw)
        {
            if (drawListTag.IsNull())
            {
                return;
            }

            uint8_t index = drawListTag.GetIndex();
            if (m_drawListFilter[index] != enableDraw)
            {
                m_needUpdate = true;
                m_drawListFilter[index] = enableDraw;
            }
        }

        RHI::DrawListMask MeshDrawPacket::GetDrawListFilter()
        {
            return m_drawListFilter;
        }

        void MeshDrawPacket::ClearDrawListFilter()
        {
            m_drawListFilter.set();
            m_needUpdate = true;
        }

#if defined(CARBONATED)
        bool MeshDrawPacket::NeedsUpdate() const
        {
            return !m_shaderVariantHandler.IsConnected()
                || (!m_material->NeedsCompile() && m_materialChangeId != m_material->GetCurrentChangeId())
                || m_needUpdate;
        }

#endif
        bool MeshDrawPacket::Update(const Scene& parentScene, bool forceUpdate /*= false*/)
        {
            // Setup the Shader variant handler when update this MeshDrawPacket the first time .
            // This is because the MeshDrawPacket data can be copied or moved right after it's created.
#if defined(CARBONATED)
            // The m_shaderVariantHandler would retain a capture of the old 'this' pointer across that operation.
#else
            // The m_shaderVariantHandler won't be copied correctly due to the capture of 'this' pointer.
            // Instead of override all the copy and move operators, this might be a better solution.
#endif
            if (!m_shaderVariantHandler.IsConnected())
            {
                m_shaderVariantHandler = Material::OnMaterialShaderVariantReadyEvent::Handler(
                    [this]()
                    {
                        this->m_needUpdate = true;
                    });
                m_material->ConnectEvent(m_shaderVariantHandler);
            }

            // Why we need to check "!m_material->NeedsCompile()"...
            //    Frame A:
            //      - Material::SetPropertyValue("foo",...). This bumps the material's CurrentChangeId()
            //      - Material::Compile() updates all the material's outputs (SRG data, shader selection, shader options, etc).
            //      - Material::SetPropertyValue("bar",...). This bumps the materials' CurrentChangeId() again.
            //      - We do not process Material::Compile() a second time because you can only call SRG::Compile() once per frame. Material::Compile()
            //        will be processed on the next frame. (See implementation of Material::Compile())
            //      - MeshDrawPacket::Update() is called. It runs DoUpdate() to rebuild the draw packet, but everything is still in the state when "foo" was
            //        set. The "bar" changes haven't been applied yet. It also sets m_materialChangeId to GetCurrentChangeId(), which corresponds to "bar" not "foo".
            //    Frame B:
            //      - Something calls Material::Compile(). This finally updates the material's outputs with the latest data corresponding to "bar".
            //      - MeshDrawPacket::Update() is called. But since the GetCurrentChangeId() hasn't changed since last time, DoUpdate() is not called.
            //      - The mesh continues rendering with only the "foo" change applied, indefinitely.

#if defined(CARBONATED)
            if (forceUpdate || NeedsUpdate())
#else
            if (forceUpdate || (!m_material->NeedsCompile() && m_materialChangeId != m_material->GetCurrentChangeId())
                || m_needUpdate)
#endif
            {
                DoUpdate(parentScene);
                m_materialChangeId = m_material->GetCurrentChangeId();
                m_needUpdate = false;

                DebugOutputShaderVariants();
                return true;
            }

            return false;
        }

        static bool HasRootConstants(const RHI::ConstantsLayout* rootConstantsLayout)
        {
            return rootConstantsLayout && rootConstantsLayout->GetDataSize() > 0;
        }

        void MeshDrawPacket::DebugOutputShaderVariants()
        {
#ifdef DEBUG_MESH_SHADERVARIANTS
            uint32_t index = 0;

            AZ::Data::AssetInfo assetInfo;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(assetInfo, &AZ::Data::AssetCatalogRequestBus::Events::GetAssetInfoById, m_modelLod->GetAssetId());

            AZ_TracePrintf("MeshDrawPacket", "Mesh: %s", assetInfo.m_relativePath.data());
            for (const auto& variant : m_shaderVariantNames)
            {
                AZ_TracePrintf("MeshDrawPacket", "%d: %s", index++, variant.data());
            }
#endif
        }

        bool MeshDrawPacket::DoUpdate(const Scene& parentScene)
        {
            const auto meshes = m_modelLod->GetMeshes();
            const ModelLod::Mesh& mesh = meshes[m_modelLodMeshIndex];

            if (!m_material)
            {
                AZ_Warning("MeshDrawPacket", false, "No material provided for mesh. Skipping.");
                return false;
            }

            ShaderReloadDebugTracker::ScopedSection reloadSection("MeshDrawPacket::DoUpdate");

            RHI::DrawPacketBuilder drawPacketBuilder;
            drawPacketBuilder.Begin(nullptr);

            drawPacketBuilder.SetDrawArguments(mesh.m_drawArguments);
            drawPacketBuilder.SetIndexBufferView(mesh.m_indexBufferView);
            drawPacketBuilder.AddShaderResourceGroup(m_objectSrg->GetRHIShaderResourceGroup());
            drawPacketBuilder.AddShaderResourceGroup(m_material->GetRHIShaderResourceGroup());

            // We build the list of used shaders in a local list rather than m_activeShaders so that
            // if DoUpdate() fails it won't modify any member data.
            MeshDrawPacket::ShaderList shaderList;
            shaderList.reserve(m_activeShaders.size());
#if defined(CARBONATED)
            const bool asyncPipelineStateCompilationEnabled = r_meshDrawPacketAsyncPSO;

            AZStd::vector<DrawItemPipelineState> drawItemPipelineStates;
            AZStd::fixed_vector<PipelineStateReference, RHI::DrawPacketBuilder::DrawItemCountMax> fallbackPipelineStates;
            AZStd::vector<PendingPipelineStateBuild> pendingPipelineStateBuilds;
            drawItemPipelineStates.reserve(m_activeShaders.size());
#endif

            // We have to keep a list of these outside the loops that collect all the shaders because the DrawPacketBuilder
            // keeps pointers to StreamBufferViews until DrawPacketBuilder::End() is called. And we use a fixed_vector to guarantee
            // that the memory won't be relocated when new entries are added.
            AZStd::fixed_vector<ModelLod::StreamBufferViewList, RHI::DrawPacketBuilder::DrawItemCountMax> streamBufferViewsPerShader;

            // The root constants are shared by all draw items in the draw packet. We must populate them with default values.
            // The draw packet builder needs to know where the data is coming from during appendShader, but it's not actually read
            // until drawPacketBuilder.End(), so store the default data out here.
#if defined(CARBONATED)
            RHI::ConstantsData rootConstants;
            RHI::ConstPtr<RHI::ConstantsLayout> rootConstantsLayoutForPacket;
#else
            AZStd::vector<uint8_t> rootConstants;
#endif
            bool isFirstShaderItem = true;

            m_perDrawSrgs.clear();

#ifdef DEBUG_MESH_SHADERVARIANTS
            m_shaderVariantNames.clear();
#endif

            auto appendShader = [&](const ShaderCollection::Item& shaderItem, const Name& materialPipelineName)
            {
#if defined(CARBONATED)
                ASSET_TAG(shaderItem.GetShaderAsset().GetHint().c_str());
#endif
                // Skip the shader item without creating the shader instance
                // if the mesh is not going to be rendered based on the draw tag
                RHI::RHISystemInterface* rhiSystem = RHI::RHISystemInterface::Get();
                RHI::DrawListTagRegistry* drawListTagRegistry = rhiSystem->GetDrawListTagRegistry();

                // Use the explicit draw list override if exists.
                RHI::DrawListTag drawListTag = shaderItem.GetDrawListTagOverride();

                if (drawListTag.IsNull())
                {
                    Data::Asset<RPI::ShaderAsset> shaderAsset = shaderItem.GetShaderAsset();
                    if (!shaderAsset.IsReady())
                    {
                        // The shader asset needs to be loaded before we can check the draw tag.
                        // If it's not loaded yet, the instance database will do a blocking load
                        // when we create the instance below, so might as well load it now.
                        shaderAsset.QueueLoad();

                        if (shaderAsset.IsLoading())
                        {
                            shaderAsset.BlockUntilLoadComplete();
                        }
                    }

                    drawListTag = drawListTagRegistry->FindTag(shaderAsset->GetDrawListName());
                }

                // draw list tag is filtered out. skip this item
                if (drawListTag.IsNull() || !m_drawListFilter[drawListTag.GetIndex()])
                {
                    return false;
                }

                if (!parentScene.HasOutputForPipelineState(drawListTag))
                {
                    // drawListTag not found in this scene, so don't render this item
                    return false;
                }

                Data::Instance<Shader> shader = RPI::Shader::FindOrCreate(shaderItem.GetShaderAsset());
                if (!shader)
                {
                    AZ_Error("MeshDrawPacket", false, "Shader '%s'. Failed to find or create instance", shaderItem.GetShaderAsset()->GetName().GetCStr());
                    return false;
                }

                RPI::ShaderOptionGroup shaderOptions = *shaderItem.GetShaderOptions();

                // Set all unspecified shader options to default values, so that we get the most specialized variant possible.
                // (because FindVariantStableId treats unspecified options as a request specifically for a variant that doesn't specify those options)
                // [GFX TODO][ATOM-3883] We should consider updating the FindVariantStableId algorithm to handle default values for us, and remove this step here.
                // This might not be necessary anymore though, since ShaderAsset::GetDefaultShaderOptions() does this when the material type builder is creating the ShaderCollection.
                shaderOptions.SetUnspecifiedToDefaultValues();

                // [GFX_TODO][ATOM-14476]: according to this usage, we should make the shader input contract uniform across all shader variants.
                m_modelLod->CheckOptionalStreams(
                    shaderOptions,
                    shader->GetInputContract(),
                    m_modelLodMeshIndex,
                    m_materialModelUvMap,
                    m_material->GetAsset()->GetMaterialTypeAsset()->GetUvNameMap());

                // apply shader options from this draw packet to the ShaderItem
                for (auto& meshShaderOption : m_shaderOptions)
                {
                    Name& name = meshShaderOption.first;
                    RPI::ShaderOptionValue& value = meshShaderOption.second;

                    ShaderOptionIndex index = shaderOptions.FindShaderOptionIndex(name);

                    // Shader options will be applied to any shader item that supports it, even if
                    // not all the shader items in the draw packet support it
                    if (index.IsValid())
                    {
                        shaderOptions.SetValue(name, value);
                    }
                }

                const ShaderVariantId requestedVariantId = shaderOptions.GetShaderVariantId();
                const ShaderVariant& variant = r_forceRootShaderVariantUsage ? shader->GetRootVariant() : shader->GetVariant(requestedVariantId);

#ifdef DEBUG_MESH_SHADERVARIANTS
                m_shaderVariantNames.push_back(variant.GetShaderVariantAsset().GetHint());
#endif

                RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;
                variant.ConfigurePipelineState(pipelineStateDescriptor, shaderOptions);

                // Render states need to merge the runtime variation.
                // This allows materials to customize the render states that the shader uses.
                const RHI::RenderStates& renderStatesOverlay = *shaderItem.GetRenderStatesOverlay();
                RHI::MergeStateInto(renderStatesOverlay, pipelineStateDescriptor.m_renderStates);

                auto& streamBufferViews = streamBufferViewsPerShader.emplace_back();

                UvStreamTangentBitmask uvStreamTangentBitmask;

                if (!m_modelLod->GetStreamsForMesh(
                    pipelineStateDescriptor.m_inputStreamLayout,
                    streamBufferViews,
                    &uvStreamTangentBitmask,
                    shader->GetInputContract(),
                    m_modelLodMeshIndex,
                    m_materialModelUvMap,
                    m_material->GetAsset()->GetMaterialTypeAsset()->GetUvNameMap()))
                {
                    return false;
                }

#if !defined(CARBONATED)
                Data::Instance<ShaderResourceGroup> drawSrg = shader->CreateDrawSrgForShaderVariant(shaderOptions, false);
                if (drawSrg)
                {
                    // Pass UvStreamTangentBitmask to the shader if the draw SRG has it.
                    AZ::Name shaderUvStreamTangentBitmask = AZ::Name(UvStreamTangentBitmask::SrgName);
                    auto index = drawSrg->FindShaderInputConstantIndex(shaderUvStreamTangentBitmask);
                    if (index.IsValid())
                    {
                        drawSrg->SetConstant(index, uvStreamTangentBitmask.GetFullTangentBitmask());
                        drawSrg->Compile();
                    }
                }
#endif

#if defined(CARBONATED)
                parentScene.ConfigurePipelineState(drawListTag, pipelineStateDescriptor);

                const uint8_t drawItemIndex = aznumeric_cast<uint8_t>(shaderList.size());
                const HashValue64 specializedDescriptorHash = pipelineStateDescriptor.GetHash();
                HashValue64 fallbackDescriptorHash = specializedDescriptorHash;
                Data::Instance<Shader> fallbackShader;
                RHI::ConstPtr<RHI::PipelineState> pipelineState;
                bool isUsingFallbackPipelineState = false;

                if (!asyncPipelineStateCompilationEnabled)
                {
                    pipelineState = shader->AcquirePipelineState(
                        pipelineStateDescriptor, RHI::PipelineStateAcquireFlags::None);
                }
                else
                {
                    fallbackShader = shader->FindOrCreateShaderOptionFallback();
                    if (!fallbackShader)
                    {
                        // A shader that does not use specialization constants is already a Fallback shader.
                        pipelineState = shader->AcquirePipelineState(pipelineStateDescriptor, RHI::PipelineStateAcquireFlags::None);
                        if (pipelineState)
                        {
                            fallbackPipelineStates.push_back({ fallbackDescriptorHash, pipelineState });
                        }
                    }
                    else
                    {
                        RHI::PipelineStateDescriptorForDraw fallbackPipelineStateDescriptor;
                        const ShaderVariant& fallbackVariant = r_forceRootShaderVariantUsage
                            ? fallbackShader->GetRootVariant()
                            : fallbackShader->GetVariant(requestedVariantId);
                        fallbackVariant.ConfigurePipelineState(fallbackPipelineStateDescriptor, shaderOptions);
                        fallbackPipelineStateDescriptor.m_inputStreamLayout = pipelineStateDescriptor.m_inputStreamLayout;
                        RHI::MergeStateInto(renderStatesOverlay, fallbackPipelineStateDescriptor.m_renderStates);
                        parentScene.ConfigurePipelineState(drawListTag, fallbackPipelineStateDescriptor);

                        fallbackDescriptorHash = fallbackPipelineStateDescriptor.GetHash();
                        auto existingFallbackIt = AZStd::find_if(
                            m_fallbackPipelineStates.begin(),
                            m_fallbackPipelineStates.end(),
                            [fallbackDescriptorHash](const PipelineStateReference& entry)
                            {
                                return entry.m_descriptorHash == fallbackDescriptorHash;
                            });
                        RHI::ConstPtr<RHI::PipelineState> fallbackPipelineState = existingFallbackIt != m_fallbackPipelineStates.end()
                            ? existingFallbackIt->m_pipelineState
                            : fallbackShader->AcquirePipelineState(
                                  fallbackPipelineStateDescriptor, RHI::PipelineStateAcquireFlags::NoCompile);

                        pipelineState = shader->AcquirePipelineState(pipelineStateDescriptor, RHI::PipelineStateAcquireFlags::NoCompile);
                        if (!pipelineState)
                        {
                            if (!fallbackPipelineState)
                            {
                                fallbackPipelineState = fallbackShader->AcquirePipelineState(
                                    fallbackPipelineStateDescriptor, RHI::PipelineStateAcquireFlags::None);
                            }
                            pipelineState = fallbackPipelineState;
                            isUsingFallbackPipelineState = true;
                            pendingPipelineStateBuilds.emplace_back(
                                shader->QueuePipelineStateBuild(m_pipelineStateBuildGroupId, pipelineStateDescriptor),
                                drawItemIndex,
                                specializedDescriptorHash);
                        }
                        else
                        {
                            if (!fallbackPipelineState)
                            {
                                pendingPipelineStateBuilds.emplace_back(
                                    fallbackShader->QueuePipelineStateBuild(m_pipelineStateBuildGroupId, fallbackPipelineStateDescriptor),
                                    drawItemIndex,
                                    fallbackDescriptorHash);
                            }
                        }
                        if (fallbackPipelineState)
                        {
                            fallbackPipelineStates.push_back({ fallbackDescriptorHash, fallbackPipelineState });
                        }
                    }
                }
#else
                parentScene.ConfigurePipelineState(drawListTag, pipelineStateDescriptor);
                const RHI::PipelineState* pipelineState = shader->AcquirePipelineState(pipelineStateDescriptor);
#endif
                if (!pipelineState)
                {
                    AZ_Error("MeshDrawPacket", false, "Shader '%s'. Failed to acquire default pipeline state", shaderItem.GetShaderAsset()->GetName().GetCStr());
                    return false;
                }

#if defined(CARBONATED)
                // A fallback PSO needs the fallback shader's DrawSrg so its fallback key is populated.
                // Shader::CreateDrawSrgForShaderVariant returns the shared dummy when the active shader is fully specialized.
                const Data::Instance<Shader>& drawSrgShader =
                    isUsingFallbackPipelineState ? fallbackShader : shader;
                Data::Instance<ShaderResourceGroup> drawSrg =
                    drawSrgShader->CreateDrawSrgForShaderVariant(shaderOptions, true);

                drawItemPipelineStates.push_back(
                    { pipelineState, specializedDescriptorHash, fallbackDescriptorHash });

#endif
                const RHI::ConstantsLayout* rootConstantsLayout =
                    pipelineStateDescriptor.m_pipelineLayoutDescriptor->GetRootConstantsLayout();
                if(isFirstShaderItem)
                {
                    if (HasRootConstants(rootConstantsLayout))
                    {
#if defined(CARBONATED)
                        rootConstantsLayoutForPacket = rootConstantsLayout;
                        rootConstants = RHI::ConstantsData(rootConstantsLayout);

                        const RHI::ShaderInputConstantIndex uvStreamTangentBitmaskIndex =
                            rootConstantsLayout->FindShaderInputIndex(Name(UvStreamTangentBitmask::RootConstantName));
                        if (uvStreamTangentBitmaskIndex.IsValid())
                        {
                            rootConstants.SetConstant(
                                uvStreamTangentBitmaskIndex,
                                uvStreamTangentBitmask.GetFullTangentBitmask());
                        }

                        drawPacketBuilder.SetRootConstants(rootConstants.GetConstantData());
#else
                        m_rootConstantsLayout = rootConstantsLayout;
                        rootConstants.resize(m_rootConstantsLayout->GetDataSize());
                        drawPacketBuilder.SetRootConstants(rootConstants);
#endif
                    }

                    isFirstShaderItem = false;
                }
                else
                {
#if defined(CARBONATED)
                    AZ_Error(
                        "MeshDrawPacket",
                        (!rootConstantsLayoutForPacket && !HasRootConstants(rootConstantsLayout)) ||
                        (rootConstantsLayoutForPacket &&
                         rootConstantsLayout &&
                         rootConstantsLayoutForPacket->GetHash() == rootConstantsLayout->GetHash()),
                        "Shader %s has mis-matched root constant layout in material %s. "
                        "All draw items in a draw packet need to share the same root constants layout. This means that each pass "
                        "(e.g. Depth, Shadows, Forward, MotionVectors) for a given materialtype should use the same layout.",
                        shaderItem.GetShaderAsset()->GetName().GetCStr(),
                        m_material->GetAsset().ToString<AZStd::string>().c_str());
#else
                    AZ_Error(
                        "MeshDrawPacket",
                        (!m_rootConstantsLayout && !HasRootConstants(rootConstantsLayout)) ||
                        (m_rootConstantsLayout && rootConstantsLayout && m_rootConstantsLayout->GetHash() == rootConstantsLayout->GetHash()),
                        "Shader %s has mis-matched root constant layout in material %s. "
                        "All draw items in a draw packet need to share the same root constants layout. This means that each pass "
                        "(e.g. Depth, Shadows, Forward, MotionVectors) for a given materialtype should use the same layout.",
                        shaderItem.GetShaderAsset()->GetName().GetCStr(),
                        m_material->GetAsset().ToString<AZStd::string>().c_str());
#endif
                }

                RHI::DrawPacketBuilder::DrawRequest drawRequest;
                drawRequest.m_listTag = drawListTag;
#if defined(CARBONATED)
                drawRequest.m_pipelineState = pipelineState.get();
#else
                drawRequest.m_pipelineState = pipelineState;
#endif
                drawRequest.m_streamBufferViews = streamBufferViews;
                drawRequest.m_stencilRef = m_stencilRef;
                drawRequest.m_sortKey = m_sortKey;
#if defined(CARBONATED) 
                drawRequest.m_stencilRef |= shaderItem.GetStencilRefOverride();
#endif
                if (drawSrg)
                {
                    drawRequest.m_uniqueShaderResourceGroup = drawSrg->GetRHIShaderResourceGroup();
                    // Hold on to a reference to the drawSrg so the refcount doesn't drop to zero
                    m_perDrawSrgs.push_back(drawSrg);
                }

                if (materialPipelineName != MaterialPipelineNone)
                {
                    RHI::DrawFilterTag pipelineTag = parentScene.GetDrawFilterTagRegistry()->AcquireTag(materialPipelineName);
                    AZ_Assert(pipelineTag.IsValid(), "Could not acquire pipeline filter tag '%s'.", materialPipelineName.GetCStr());
                    drawRequest.m_drawFilterMask = 1 << pipelineTag.GetIndex();
                }

                drawPacketBuilder.AddDrawItem(drawRequest);

                ShaderData shaderData;
                shaderData.m_shader = AZStd::move(shader);
#if defined(CARBONATED)
                shaderData.m_fallbackShader = AZStd::move(fallbackShader);
#endif
                shaderData.m_materialPipelineName = materialPipelineName;
                shaderData.m_shaderTag = shaderItem.GetShaderTag();
                shaderData.m_requestedShaderVariantId = requestedVariantId;
                shaderData.m_activeShaderVariantId = variant.GetShaderVariantId();
                shaderData.m_activeShaderVariantStableId = variant.GetStableId();
                shaderList.emplace_back(AZStd::move(shaderData));

                return true;
            };

            m_material->ApplyGlobalShaderOptions();

            // TODO(MaterialPipeline): We might want to detect duplicate ShaderItem objects here, and merge them to avoid redundant RHI DrawItems.
            m_material->ForAllShaderItems(
                [&](const Name& materialPipelineName, const ShaderCollection::Item& shaderItem)
                {
                    if (shaderItem.IsEnabled())
                    {
                        if (shaderList.size() == RHI::DrawPacketBuilder::DrawItemCountMax)
                        {
                            AZ_Error("MeshDrawPacket", false, "Material has more than the limit of %d active shader items.", RHI::DrawPacketBuilder::DrawItemCountMax);
                            return false;
                        }
#if defined(CARBONATED) && defined(CARBONATED_SHADER_LOADING_TIME)
                        const int64_t startTime = static_cast<int64_t>(AZ::GetRealElapsedTimeMs());
                        appendShader(shaderItem, materialPipelineName);
                        const int64_t dt = static_cast<int64_t>(AZ::GetRealElapsedTimeMs()) - startTime;
                        const int64_t threshold =
#if defined(AZ_PLATFORM_LINUX)
                                                  20;
#else
                                                  50;
#endif
                        if (dt > threshold)
                        {
                            AZ_Info("PrimitiveLoadTime", "appended shader '%s' for pipeline '%s' in  %d ms", shaderItem.GetShaderAsset().GetHint().c_str(), materialPipelineName.GetCStr(), dt);
                        }
#else
                        appendShader(shaderItem, materialPipelineName);
#endif

                    }

                    return true;
                });

            m_drawPacket = drawPacketBuilder.End();

            if (m_drawPacket)
            {
                m_activeShaders = shaderList;
#if defined(CARBONATED)
                m_drawItemPipelineStates = AZStd::move(drawItemPipelineStates);
                m_fallbackPipelineStates = AZStd::move(fallbackPipelineStates);
                m_pendingPipelineStateBuilds = AZStd::move(pendingPipelineStateBuilds);
#endif
                m_materialSrg = m_material->GetRHIShaderResourceGroup();
#if defined(CARBONATED)
                m_rootConstantsLayout = rootConstantsLayoutForPacket;
#endif
                return true;
            }
            else
            {
                return false;
            }
        }

        const RHI::ConstPtr<RHI::ConstantsLayout> MeshDrawPacket::GetRootConstantsLayout() const
        {
            return m_rootConstantsLayout;
        }
    } // namespace RPI
} // namespace AZ

