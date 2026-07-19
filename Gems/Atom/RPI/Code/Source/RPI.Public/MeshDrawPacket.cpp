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
#include <AzCore/std/containers/array.h>
#include <Atom/RPI.Public/Shader/ShaderReloadDebugTracker.h>

#if defined(CARBONATED) && defined(CARBONATED_SHADER_LOADING_TIME)
#include <AzCore/Time/ITime.h>
#endif
#if defined(CARBONATED)
#include <AzCore/Memory/MemoryMarker.h>
#endif

namespace AZ
{
    namespace RPI
    {
        AZ_CVAR(bool,
            r_forceRootShaderVariantUsage,
            false,
            [](const bool&) { AZ::Interface<AZ::IConsole>::Get()->PerformCommand("MeshFeatureProcessor.ForceRebuildDrawPackets"); },
            ConsoleFunctorFlags::Null,
            "(For Testing) Forces usage of root shader variant in the mesh draw packet level, ignoring any other shader variants that may exist."
        );

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

        bool MeshDrawPacket::Update(const Scene& parentScene, bool forceUpdate /*= false*/)
        {
            if (BeginUpdate(forceUpdate))
            {
                DoUpdate(parentScene);

                DebugOutputShaderVariants();
                return true;
            }

            return false;
        }

        bool MeshDrawPacket::BeginUpdate(bool forceUpdate /*= false*/)
        {
            // Setup the Shader variant handler when update this MeshDrawPacket the first time .
            // This is because the MeshDrawPacket data can be copied or moved right after it's created.
            // The m_shaderVariantHandler won't be copied correctly due to the capture of 'this' pointer.
            // Instead of override all the copy and move operators, this might be a better solution.
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

            if (forceUpdate || (!m_material->NeedsCompile() && m_materialChangeId != m_material->GetCurrentChangeId())
                || m_needUpdate)
            {
                m_materialChangeId = m_material->GetCurrentChangeId();
                m_needUpdate = false;
                return true;
            }

            return false;
        }

        void MeshDrawPacket::ResetRHIDrawPacket()
        {
            m_drawPacket = nullptr;
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
            ShaderReloadDebugTracker::ScopedSection reloadSection("MeshDrawPacket::DoUpdate");

            DrawPacketBuiltRequestPtr drawPacketBuiltRequest =
                CreateDrawPacketBuiltRequest(parentScene, false);
            if (!drawPacketBuiltRequest)
            {
                return false;
            }

            drawPacketBuiltRequest->GetPipelineStateBuildRequest()->Build();
            return ApplyDrawPacketBuiltRequest(AZStd::move(drawPacketBuiltRequest));
        }

        MeshDrawPacket::DrawPacketBuiltRequestPtr MeshDrawPacket::CreateDrawPacketBuiltRequest(
            const Scene& parentScene,
            bool useFallbackShaders)
        {
            return CreateDrawPacketBuiltRequestInternal(
                parentScene,
                useFallbackShaders,
                &m_drawSrgReuseData,
                true);
        }

        MeshDrawPacket::DrawPacketBuiltRequestPtr
        MeshDrawPacket::CreateDeferredDrawPacketBuiltRequest(
            const Scene& parentScene)
        {
            AZStd::shared_ptr<MeshDrawPacket> packetSnapshot(
                new MeshDrawPacket);
            packetSnapshot->m_modelLod = m_modelLod;
            packetSnapshot->m_modelLodMeshIndex = m_modelLodMeshIndex;
            packetSnapshot->m_objectSrg = m_objectSrg;
            packetSnapshot->m_material = m_material;
            packetSnapshot->m_sortKey = m_sortKey;
            packetSnapshot->m_stencilRef = m_stencilRef;
            packetSnapshot->m_materialModelUvMap = m_materialModelUvMap;
            packetSnapshot->m_shaderOptions = m_shaderOptions;
            packetSnapshot->m_drawListFilter = m_drawListFilter;
            packetSnapshot->m_drawSrgReuseData = m_drawSrgReuseData;

            DrawPacketBuiltRequestPtr drawPacketBuiltRequest(
                new DrawPacketBuiltRequest);
            AZStd::weak_ptr<DrawPacketBuiltRequest> weakRequest =
                drawPacketBuiltRequest;
            const Scene* parentScenePtr = &parentScene;
            drawPacketBuiltRequest->m_pipelineStateBuildRequest =
                PipelineStateBuildRequestPtr(
                    new PipelineStateBuildRequest(
                        [packetSnapshot, weakRequest, parentScenePtr](
                            PipelineStateBuildItemList&
                                pipelineStateBuildItems)
                        {
                            DrawPacketBuiltRequestPtr request =
                                weakRequest.lock();
                            if (!request)
                            {
                                return false;
                            }

                            return packetSnapshot
                                ->PreparePipelineStateBuildItems(
                                    *parentScenePtr,
                                    false,
                                    pipelineStateBuildItems,
                                    request->m_drawPacketBuildData,
                                    &packetSnapshot->m_drawSrgReuseData,
                                    false);
                        }));
            return drawPacketBuiltRequest;
        }

        MeshDrawPacket::DrawPacketBuiltRequestPtr MeshDrawPacket::CreateDrawPacketBuiltRequestInternal(
            const Scene& parentScene,
            bool useFallbackShaders,
            const DrawSrgReuseDataList* drawSrgSourceData,
            bool updateReusedDrawSrgs)
        {
            DrawPacketBuiltRequestPtr drawPacketBuiltRequest(new DrawPacketBuiltRequest);
            drawPacketBuiltRequest->m_useFallbackShaders = useFallbackShaders;
            PipelineStateBuildItemList pipelineStateBuildItems;
            if (!PreparePipelineStateBuildItems(
                    parentScene,
                    useFallbackShaders,
                    pipelineStateBuildItems,
                    drawPacketBuiltRequest->m_drawPacketBuildData,
                    drawSrgSourceData,
                    updateReusedDrawSrgs))
            {
                return nullptr;
            }

            drawPacketBuiltRequest->m_pipelineStateBuildRequest =
                PipelineStateBuildRequestPtr(
                    new PipelineStateBuildRequest(AZStd::move(pipelineStateBuildItems)));
            return drawPacketBuiltRequest;
        }

        bool MeshDrawPacket::ApplyDrawPacketBuiltRequest(
            DrawPacketBuiltRequestPtr drawPacketBuiltRequest)
        {
            if (!drawPacketBuiltRequest ||
                !drawPacketBuiltRequest->m_pipelineStateBuildRequest ||
                !drawPacketBuiltRequest->m_pipelineStateBuildRequest->IsSuccessful())
            {
                return false;
            }

            PipelineStateBuildItemList pipelineStateBuildItems =
                drawPacketBuiltRequest->m_pipelineStateBuildRequest->TakeBuildItems();
            return BuildDrawPacket(
                AZStd::move(drawPacketBuiltRequest->m_drawPacketBuildData),
                AZStd::move(pipelineStateBuildItems),
                drawPacketBuiltRequest->m_pipelineStateBuildRequest->GetPipelineStates(),
                drawPacketBuiltRequest->m_useFallbackShaders);
        }

        MeshDrawPacket::DrawPacketBuiltRequest::~DrawPacketBuiltRequest()
        {
            if (m_pipelineStateBuildRequest)
            {
                m_pipelineStateBuildRequest->Cancel();
            }
        }

        const PipelineStateBuildRequestPtr&
        MeshDrawPacket::DrawPacketBuiltRequest::GetPipelineStateBuildRequest() const
        {
            return m_pipelineStateBuildRequest;
        }

        bool MeshDrawPacket::PreparePipelineStateBuildItems(
            const Scene& parentScene,
            bool useFallbackShaders,
            PipelineStateBuildItemList& pipelineStateBuildItems,
            DrawPacketBuildData& drawPacketBuildData,
            const DrawSrgReuseDataList* drawSrgSourceData,
            bool updateReusedDrawSrgs)
        {
            const ModelLod::Mesh& mesh = GetMesh();

            if (!m_material)
            {
                AZ_Warning("MeshDrawPacket", false, "No material provided for mesh. Skipping.");
                return false;
            }

            pipelineStateBuildItems.clear();
            pipelineStateBuildItems.reserve(m_activeShaders.size());
            drawPacketBuildData = {};
            drawPacketBuildData.m_drawArguments = mesh.m_drawArguments;
            drawPacketBuildData.m_indexBufferView = mesh.m_indexBufferView;
            drawPacketBuildData.m_objectSrg = m_objectSrg->GetRHIShaderResourceGroup();
            drawPacketBuildData.m_materialSrg = m_material->GetRHIShaderResourceGroup();

            AZStd::array<bool, RHI::DrawPacketBuilder::DrawItemCountMax> drawSrgSourceDataUsed{};
            auto appendShader = [&](const ShaderCollection::Item& shaderItem, const Name& materialPipelineName) -> bool
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
                    return true;
                }

                if (!parentScene.HasOutputForPipelineState(drawListTag))
                {
                    // drawListTag not found in this scene, so don't render this item
                    return true;
                }

                Data::Instance<Shader> shader = RPI::Shader::FindOrCreate(shaderItem.GetShaderAsset());
                if (!shader)
                {
                    AZ_Error("MeshDrawPacket", false, "Shader '%s'. Failed to find or create instance", shaderItem.GetShaderAsset()->GetName().GetCStr());
                    return false;
                }

                if (useFallbackShaders && shader->GetAsset()->UseSpecializationConstants(shader->GetSupervariantIndex()))
                {
                    const SupervariantIndex specializedSupervariantIndex = shader->GetSupervariantIndex();
                    shader = shader->FindOrCreateShaderOptionFallback();
                    if (!shader)
                    {
                        AZ_Error(
                            "MeshDrawPacket",
                            false,
                            "Shader '%s' has no non-specialized fallback for supervariant '%s'",
                            shaderItem.GetShaderAsset()->GetName().GetCStr(),
                            shaderItem.GetShaderAsset()->GetSupervariantName(specializedSupervariantIndex).GetCStr());
                        return false;
                    }
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
                const ShaderVariant& variant =
                    (useFallbackShaders || r_forceRootShaderVariantUsage)
                    ? shader->GetRootVariant()
                    : shader->GetVariant(requestedVariantId);

#ifdef DEBUG_MESH_SHADERVARIANTS
                drawPacketBuildData.m_shaderVariantNames.push_back(variant.GetShaderVariantAsset().GetHint());
#endif

                RHI::PipelineStateDescriptorForDraw pipelineStateDescriptor;
                variant.ConfigurePipelineState(pipelineStateDescriptor, shaderOptions);

                // Render states need to merge the runtime variation.
                // This allows materials to customize the render states that the shader uses.
                const RHI::RenderStates& renderStatesOverlay = *shaderItem.GetRenderStatesOverlay();
                RHI::MergeStateInto(renderStatesOverlay, pipelineStateDescriptor.m_renderStates);

                DrawItemBuildData drawItemBuildData;
                UvStreamTangentBitmask uvStreamTangentBitmask;

                if (!m_modelLod->GetStreamsForMesh(
                    pipelineStateDescriptor.m_inputStreamLayout,
                    drawItemBuildData.m_streamBufferViews,
                    &uvStreamTangentBitmask,
                    shader->GetInputContract(),
                    m_modelLodMeshIndex,
                    m_materialModelUvMap,
                    m_material->GetAsset()->GetMaterialTypeAsset()->GetUvNameMap()))
                {
                    return false;
                }

                Data::Instance<ShaderResourceGroup> drawSrg;
                bool drawSrgReused = false;
                if (drawSrgSourceData)
                {
                    const RHI::Ptr<RHI::ShaderResourceGroupLayout>&
                        targetDrawSrgLayout =
                            shader->GetAsset()->GetDrawSrgLayout(
                                shader->GetSupervariantIndex());

                    for (size_t sourceIndex = 0;
                         sourceIndex < drawSrgSourceData->size();
                         ++sourceIndex)
                    {
                        if (drawSrgSourceDataUsed[sourceIndex])
                        {
                            continue;
                        }

                        const DrawSrgReuseData& sourceDrawSrgData =
                            (*drawSrgSourceData)[sourceIndex];
                        const RHI::ShaderResourceGroupLayout* sourceDrawSrgLayout =
                            sourceDrawSrgData.m_drawSrg
                            ? sourceDrawSrgData.m_drawSrg->GetLayout()
                            : nullptr;

                        const bool drawItemsMatch =
                            sourceDrawSrgData.m_shaderAssetId ==
                                shaderItem.GetShaderAsset().GetId() &&
                            sourceDrawSrgData.m_materialPipelineName ==
                                materialPipelineName &&
                            sourceDrawSrgData.m_shaderTag ==
                                shaderItem.GetShaderTag() &&
                            sourceDrawSrgData.m_drawListTag == drawListTag;
                        const bool drawSrgLayoutsMatch =
                            (!sourceDrawSrgLayout && !targetDrawSrgLayout) ||
                            (sourceDrawSrgLayout && targetDrawSrgLayout &&
                             sourceDrawSrgLayout->GetHash() ==
                                 targetDrawSrgLayout->GetHash());

                        if (drawItemsMatch && drawSrgLayoutsMatch)
                        {
                            drawSrg = sourceDrawSrgData.m_drawSrg;
                            drawSrgSourceDataUsed[sourceIndex] = true;
                            drawSrgReused = true;
                            break;
                        }
                    }
                }

                if (!drawSrgReused)
                {
                    drawSrg = shader->CreateDrawSrgForShaderVariant(shaderOptions, false);
                }
                if (drawSrg && (!drawSrgReused || updateReusedDrawSrgs))
                {
                    if (drawSrgReused &&
                        drawSrg->HasShaderVariantKeyFallbackEntry())
                    {
                        drawSrg->SetShaderVariantKeyFallbackValue(
                            shaderOptions.GetShaderVariantKeyFallbackValue());
                    }

                    // Pass UvStreamTangentBitmask to the shader if the draw SRG has it.

                    AZ::Name shaderUvStreamTangentBitmask = AZ::Name(UvStreamTangentBitmask::SrgName);
                    auto index = drawSrg->FindShaderInputConstantIndex(shaderUvStreamTangentBitmask);

                    if (index.IsValid())
                    {
                        drawSrg->SetConstant(index, uvStreamTangentBitmask.GetFullTangentBitmask());
                    }

                    if (!drawSrg->IsQueuedForCompile())
                    {
                        drawSrg->Compile();
                    }
                }

                parentScene.ConfigurePipelineState(drawListTag, pipelineStateDescriptor);

                const RHI::ConstantsLayout* rootConstantsLayout =
                    pipelineStateDescriptor.m_pipelineLayoutDescriptor->GetRootConstantsLayout();
                if (pipelineStateBuildItems.empty())
                {
                    if (HasRootConstants(rootConstantsLayout))
                    {
                        drawPacketBuildData.m_rootConstantsLayout = rootConstantsLayout;
                    }
                }
                else
                {
                    AZ_Error(
                        "MeshDrawPacket",
                        (!drawPacketBuildData.m_rootConstantsLayout && !HasRootConstants(rootConstantsLayout)) ||
                        (drawPacketBuildData.m_rootConstantsLayout && rootConstantsLayout &&
                         drawPacketBuildData.m_rootConstantsLayout->GetHash() == rootConstantsLayout->GetHash()),
                        "Shader %s has mis-matched root constant layout in material %s. "
                        "All draw items in a draw packet need to share the same root constants layout. This means that each pass "
                        "(e.g. Depth, Shadows, Forward, MotionVectors) for a given materialtype should use the same layout.",
                        shaderItem.GetShaderAsset()->GetName().GetCStr(),
                        m_material->GetAsset().ToString<AZStd::string>().c_str());
                }

                drawItemBuildData.m_materialPipelineName = materialPipelineName;
                drawItemBuildData.m_requestedShaderVariantId = requestedVariantId;
                drawItemBuildData.m_activeShaderVariantId = variant.GetShaderVariantId();
                drawItemBuildData.m_activeShaderVariantStableId = variant.GetStableId();
                drawItemBuildData.m_shaderTag = shaderItem.GetShaderTag();
                drawItemBuildData.m_drawListTag = drawListTag;
                drawItemBuildData.m_drawSrg = AZStd::move(drawSrg);
                drawItemBuildData.m_stencilRef = m_stencilRef;
                drawItemBuildData.m_sortKey = m_sortKey;
#if defined(CARBONATED) 
                drawItemBuildData.m_stencilRef |= shaderItem.GetStencilRefOverride();
#endif

                DrawSrgReuseData drawSrgReuseData;
                drawSrgReuseData.m_shaderAssetId =
                    shaderItem.GetShaderAsset().GetId();
                drawSrgReuseData.m_materialPipelineName = materialPipelineName;
                drawSrgReuseData.m_shaderTag = shaderItem.GetShaderTag();
                drawSrgReuseData.m_drawListTag = drawListTag;
                drawSrgReuseData.m_drawSrg = drawItemBuildData.m_drawSrg;

                if (materialPipelineName != MaterialPipelineNone)
                {
                    RHI::DrawFilterTag pipelineTag = parentScene.GetDrawFilterTagRegistry()->AcquireTag(materialPipelineName);
                    AZ_Assert(pipelineTag.IsValid(), "Could not acquire pipeline filter tag '%s'.", materialPipelineName.GetCStr());
                    drawItemBuildData.m_drawFilterMask = 1 << pipelineTag.GetIndex();
                }

                PipelineStateBuildItem pipelineStateBuildItem;
                pipelineStateBuildItem.m_shader = AZStd::move(shader);
                pipelineStateBuildItem.m_descriptor = AZStd::move(pipelineStateDescriptor);

                drawPacketBuildData.m_drawItems.emplace_back(AZStd::move(drawItemBuildData));
                drawPacketBuildData.m_drawSrgReuseData.emplace_back(AZStd::move(drawSrgReuseData));
                pipelineStateBuildItems.emplace_back(AZStd::move(pipelineStateBuildItem));
                return true;
            };

            m_material->ApplyGlobalShaderOptions();

            bool preparationSucceeded = true;
            // TODO(MaterialPipeline): We might want to detect duplicate ShaderItem objects here, and merge them to avoid redundant RHI DrawItems.
            m_material->ForAllShaderItems(
                [&](const Name& materialPipelineName, const ShaderCollection::Item& shaderItem)
                {
                    if (shaderItem.IsEnabled())
                    {
                        if (pipelineStateBuildItems.size() == RHI::DrawPacketBuilder::DrawItemCountMax)
                        {
                            AZ_Error("MeshDrawPacket", false, "Material has more than the limit of %d active shader items.", RHI::DrawPacketBuilder::DrawItemCountMax);
                            preparationSucceeded = false;
                            return false;
                        }
#if defined(CARBONATED) && defined(CARBONATED_SHADER_LOADING_TIME)
                        const int64_t startTime = static_cast<int64_t>(AZ::GetRealElapsedTimeMs());
                        preparationSucceeded = appendShader(shaderItem, materialPipelineName);
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
                        preparationSucceeded = appendShader(shaderItem, materialPipelineName);
#endif
                        if (!preparationSucceeded)
                        {
                            return false;
                        }
                    }

                    return true;
                });

            return preparationSucceeded;
        }

        bool MeshDrawPacket::BuildDrawPacket(
            DrawPacketBuildData&& drawPacketBuildData,
            PipelineStateBuildItemList&& pipelineStateBuildItems,
            AZStd::span<const RHI::ConstPtr<RHI::PipelineState>> pipelineStates,
            bool useFallbackShaders)
        {
            if (drawPacketBuildData.m_drawItems.size() != pipelineStateBuildItems.size() ||
                pipelineStateBuildItems.size() != pipelineStates.size())
            {
                AZ_Error("MeshDrawPacket", false, "Draw packet build data and pipeline state counts do not match.");
                return false;
            }

            if (pipelineStateBuildItems.empty())
            {
                // An empty item list is a valid result when every material shader item is
                // disabled or filtered out. Publish an empty packet state and release resources
                // retained by the previously renderable packet.
                if (useFallbackShaders)
                {
                    m_retainedFallbackShaders.clear();
                }
                m_drawPacket = nullptr;
                m_activeShaders.clear();
                m_drawSrgReuseData.clear();
                m_materialSrg = nullptr;
                m_rootConstantsLayout = nullptr;
#ifdef DEBUG_MESH_SHADERVARIANTS
                m_shaderVariantNames.clear();
#endif
                return true;
            }

            RHI::DrawPacketBuilder drawPacketBuilder;
            drawPacketBuilder.Begin(nullptr);
            drawPacketBuilder.SetDrawArguments(drawPacketBuildData.m_drawArguments);
            drawPacketBuilder.SetIndexBufferView(drawPacketBuildData.m_indexBufferView);
            drawPacketBuilder.AddShaderResourceGroup(drawPacketBuildData.m_objectSrg.get());
            drawPacketBuilder.AddShaderResourceGroup(drawPacketBuildData.m_materialSrg.get());

            AZStd::vector<uint8_t> rootConstants;
            if (HasRootConstants(drawPacketBuildData.m_rootConstantsLayout.get()))
            {
                rootConstants.resize(drawPacketBuildData.m_rootConstantsLayout->GetDataSize());
                drawPacketBuilder.SetRootConstants(rootConstants);
            }

            for (size_t index = 0; index < pipelineStates.size(); ++index)
            {
                const DrawItemBuildData& drawItemBuildData = drawPacketBuildData.m_drawItems[index];
                RHI::DrawPacketBuilder::DrawRequest drawRequest;
                drawRequest.m_listTag = drawItemBuildData.m_drawListTag;
                drawRequest.m_pipelineState = pipelineStates[index].get();
                drawRequest.m_streamBufferViews = drawItemBuildData.m_streamBufferViews;
                drawRequest.m_stencilRef = drawItemBuildData.m_stencilRef;
                drawRequest.m_sortKey = drawItemBuildData.m_sortKey;
                drawRequest.m_drawFilterMask = drawItemBuildData.m_drawFilterMask;
                if (drawItemBuildData.m_drawSrg)
                {
                    drawRequest.m_uniqueShaderResourceGroup =
                        drawItemBuildData.m_drawSrg->GetRHIShaderResourceGroup();
                }
                drawPacketBuilder.AddDrawItem(drawRequest);
            }

            Ptr<RHI::DrawPacket> drawPacket = drawPacketBuilder.End();
            if (!drawPacket)
            {
                return false;
            }

            if (useFallbackShaders)
            {
                AZStd::fixed_vector<Data::Instance<Shader>, RHI::DrawPacketBuilder::DrawItemCountMax>
                    retainedFallbackShaders;
                for (const PipelineStateBuildItem& pipelineStateBuildItem : pipelineStateBuildItems)
                {
                    retainedFallbackShaders.emplace_back(pipelineStateBuildItem.m_shader);
                }
                m_retainedFallbackShaders = AZStd::move(retainedFallbackShaders);
            }

            ShaderList shaderList;
            shaderList.reserve(pipelineStateBuildItems.size());
            for (size_t index = 0; index < pipelineStateBuildItems.size(); ++index)
            {
                PipelineStateBuildItem& pipelineStateBuildItem = pipelineStateBuildItems[index];
                const DrawItemBuildData& drawItemBuildData = drawPacketBuildData.m_drawItems[index];

                ShaderData shaderData;
                shaderData.m_shader = AZStd::move(pipelineStateBuildItem.m_shader);
                shaderData.m_materialPipelineName = drawItemBuildData.m_materialPipelineName;
                shaderData.m_shaderTag = drawItemBuildData.m_shaderTag;
                shaderData.m_requestedShaderVariantId = drawItemBuildData.m_requestedShaderVariantId;
                shaderData.m_activeShaderVariantId = drawItemBuildData.m_activeShaderVariantId;
                shaderData.m_activeShaderVariantStableId = drawItemBuildData.m_activeShaderVariantStableId;
                shaderList.emplace_back(AZStd::move(shaderData));
            }

            m_drawPacket = AZStd::move(drawPacket);
            m_activeShaders = AZStd::move(shaderList);
            m_drawSrgReuseData = AZStd::move(drawPacketBuildData.m_drawSrgReuseData);
            m_materialSrg = AZStd::move(drawPacketBuildData.m_materialSrg);
            m_rootConstantsLayout = AZStd::move(drawPacketBuildData.m_rootConstantsLayout);
#ifdef DEBUG_MESH_SHADERVARIANTS
            m_shaderVariantNames = AZStd::move(drawPacketBuildData.m_shaderVariantNames);
#endif
            return true;
        }

        const RHI::ConstPtr<RHI::ConstantsLayout> MeshDrawPacket::GetRootConstantsLayout() const
        {
            return m_rootConstantsLayout;
        }
    } // namespace RPI
} // namespace AZ

