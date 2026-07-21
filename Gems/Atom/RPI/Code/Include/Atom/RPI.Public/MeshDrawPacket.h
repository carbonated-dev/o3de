/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Public/Material/Material.h>
#include <Atom/RPI.Public/Model/ModelLod.h>
#include <Atom/RPI.Public/PipelineStateBuildQueue.h>
#include <Atom/RHI/DrawPacket.h>
#include <Atom/RHI/DrawPacketBuilder.h>

#include <AzCore/Math/Obb.h>
#include <AzCore/std/containers/fixed_vector.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>

// Enable this define to print the shader variants used by MeshDrawPacket every time the draw packet get rebuilt.
// Note: the log can be extremely long if there are too many mesh instances (for example, >5K).  
// #define DEBUG_MESH_SHADERVARIANTS

namespace AZ
{
    namespace RHI
    {
        class ConstantsLayout;
    }

    namespace RPI
    {
        class Scene;

        //! Holds and manages an RHI DrawPacket for a specific mesh, and the resources that are needed to build and maintain it.
        class MeshDrawPacket
        {
        public:
            struct ShaderData
            {
                Data::Instance<Shader> m_shader;
                Name m_materialPipelineName;
                Name m_shaderTag;
                ShaderVariantId m_requestedShaderVariantId;
                ShaderVariantId m_activeShaderVariantId;
                ShaderVariantStableId m_activeShaderVariantStableId;
            };

            using ShaderList = AZStd::vector<ShaderData>;

            class DrawPacketBuiltRequest;
            using DrawPacketBuiltRequestPtr = AZStd::shared_ptr<DrawPacketBuiltRequest>;
            class FallbackPipelineStateBundle;
            using FallbackPipelineStateBundlePtr =
                AZStd::shared_ptr<const FallbackPipelineStateBundle>;

            MeshDrawPacket() = default;
            MeshDrawPacket(
                ModelLod& modelLod,
                size_t modelLodMeshIndex,
                Data::Instance<Material> materialOverride,
                Data::Instance<ShaderResourceGroup> objectSrg,
                const MaterialModelUvOverrideMap& materialModelUvMap = {});

            AZ_DEFAULT_COPY(MeshDrawPacket);
            AZ_DEFAULT_MOVE(MeshDrawPacket);

            bool Update(const Scene& parentScene, bool forceUpdate = false);

            //! Consumes the current update state without rebuilding the draw packet.
            //! The caller must capture and build a replacement from the current state when this returns true.
            bool BeginUpdate(bool forceUpdate = false);

            //! Marks the packet for rebuilding during the next mesh update pass.
            void RequestUpdate() { m_needUpdate = true; }

            RHI::DrawPacket* GetRHIDrawPacket() { return m_drawPacket.get(); }
            const RHI::DrawPacket* GetRHIDrawPacket() const { return m_drawPacket.get(); }
            const RHI::ConstPtr<RHI::ConstantsLayout> GetRootConstantsLayout() const;

            //! Stops rendering this mesh draw packet until a replacement is applied.
            void ResetRHIDrawPacket();

            //! Performs a lightweight enabled/draw-list/scene-output check without resolving
            //! shader instances or constructing pipeline-state descriptors.
            bool HasDrawItems(const Scene& parentScene) const;

            void SetStencilRef(uint8_t stencilRef);
            void SetSortKey(RHI::DrawItemSortKey sortKey);
            bool SetShaderOption(const Name& shaderOptionName, RPI::ShaderOptionValue value);
            bool UnsetShaderOption(const Name& shaderOptionName);
            void ClearShaderOptions();

            // Enable/disable draw filter for a specific draw list tag.
            // If disabled, any draw items with this drawListTag won't be added to the DrawPacket when updated
            void SetEnableDraw(RHI::DrawListTag drawListTag, bool enableDraw);
            RHI::DrawListMask GetDrawListFilter();
            // Remove the draw list filter and enable render for all draw items
            void ClearDrawListFilter();

            Data::Instance<Material> GetMaterial() const;
            const ModelLod::Mesh& GetMesh() const;
            const ShaderList& GetActiveShaderList() const { return m_activeShaders; }

            //! Captures the mesh-specific packet data and a renderer-wide pipeline-state build request.
            //! The returned build does not retain a pointer to this MeshDrawPacket.
            DrawPacketBuiltRequestPtr CreateDrawPacketBuiltRequest(
                const Scene& parentScene,
                bool useFallbackShaders);

            //! Captures the state needed to prepare a specialized packet without retaining this
            //! MeshDrawPacket. Preparation and PSO creation both occur on the pipeline build queue.
            DrawPacketBuiltRequestPtr CreateDeferredDrawPacketBuiltRequest(
                const Scene& parentScene);

            //! Captures a fallback-only PSO warm-up request. Descriptor preparation and PSO
            //! creation occur on the pipeline-state queue; no draw packet or Draw SRG is built.
            DrawPacketBuiltRequestPtr CreateDeferredFallbackPipelineStateRequest(
                const Scene& parentScene);

            //! Creates the one queue-side request used to populate a shared fallback bundle.
            //! It contains no packet-specific Draw SRGs and is safe to share across packets
            //! with the same fallback structural key.
            DrawPacketBuiltRequestPtr CreateDeferredFallbackPipelineStateBundleRequest(
                const Scene& parentScene);

            //! Captures a complete fallback packet for queue-side preparation. Used when no
            //! compatible shared fallback bundle has completed yet.
            DrawPacketBuiltRequestPtr CreateDeferredFallbackDrawPacketBuiltRequest(
                const Scene& parentScene);

            //! Returns a specialization-value-independent key for fallback PSO preparation.
            AZ::HashValue64 GetFallbackPipelineStateKey(const Scene& parentScene) const;

            //! Creates a reusable immutable bundle from a completed fallback request.
            static FallbackPipelineStateBundlePtr CreateFallbackPipelineStateBundle(
                const DrawPacketBuiltRequestPtr& drawPacketBuiltRequest);

            //! Refreshes packet-specific fallback keys/SRG state and completes the request from
            //! a previously prepared shared bundle without rebuilding PSO descriptors.
            DrawPacketBuiltRequestPtr CreateDrawPacketBuiltRequestFromFallbackBundle(
                const Scene& parentScene,
                const FallbackPipelineStateBundlePtr& fallbackBundle);

            //! Applies a successfully completed build to this MeshDrawPacket.
            bool ApplyDrawPacketBuiltRequest(DrawPacketBuiltRequestPtr drawPacketBuiltRequest);

            //! Retains a completed fallback warm-up so its shader pipeline libraries and PSOs
            //! cannot be compacted before this packet needs them.
            bool RetainFallbackPipelineStates(DrawPacketBuiltRequestPtr drawPacketBuiltRequest);

            //! Returns whether the current packet revision has neither a retained fallback nor
            //! an in-flight fallback warm-up.
            bool NeedsFallbackPipelineStateWarmup() const;

            //! Publishes a fallback request directly from the exact PSOs retained for an earlier
            //! revision when its prepared descriptors have the same fingerprint.
            bool TryApplyRetainedFallbackPipelineStates(
                DrawPacketBuiltRequestPtr drawPacketBuiltRequest);

            void DebugOutputShaderVariants();

        private:
            struct DrawItemBuildData
            {
                Name m_materialPipelineName;
                ShaderVariantId m_requestedShaderVariantId;
                ShaderVariantId m_activeShaderVariantId;
                ShaderVariantStableId m_activeShaderVariantStableId;
                Name m_shaderTag;
                RHI::DrawListTag m_drawListTag;
                ModelLod::StreamBufferViewList m_streamBufferViews;
                Data::Instance<ShaderResourceGroup> m_drawSrg;
                RHI::DrawFilterMask m_drawFilterMask = RHI::DrawFilterMaskDefaultValue;
                RHI::DrawItemSortKey m_sortKey = 0;
                uint8_t m_stencilRef = 0;
            };

            struct DrawSrgReuseData
            {
                Data::AssetId m_shaderAssetId;
                Name m_materialPipelineName;
                Name m_shaderTag;
                RHI::DrawListTag m_drawListTag;
                Data::Instance<ShaderResourceGroup> m_drawSrg;
                bool m_isDummy = false;
            };

            struct CachedDrawListTag
            {
                Data::AssetId m_shaderAssetId;
                Name m_drawListName;
                RHI::DrawListTag m_drawListTag;
            };

            struct CachedShaderInstance
            {
                Data::AssetId m_shaderAssetId;
                SupervariantIndex m_supervariantIndex;
                Data::Instance<Shader> m_shader;
            };

            using DrawSrgReuseDataList =
                AZStd::fixed_vector<DrawSrgReuseData, RHI::DrawPacketBuilder::DrawItemCountMax>;

            struct DrawPacketBuildData
            {
                RHI::DrawArguments m_drawArguments;
                RHI::IndexBufferView m_indexBufferView;
                ConstPtr<RHI::ShaderResourceGroup> m_objectSrg;
                ConstPtr<RHI::ShaderResourceGroup> m_materialSrg;
                RHI::ConstPtr<RHI::ConstantsLayout> m_rootConstantsLayout;
                uint32_t m_uvStreamTangentBitmask = 0;
                AZStd::fixed_vector<DrawItemBuildData, RHI::DrawPacketBuilder::DrawItemCountMax> m_drawItems;
                DrawSrgReuseDataList m_drawSrgReuseData;
#ifdef DEBUG_MESH_SHADERVARIANTS
                AZStd::vector<AZStd::string_view> m_shaderVariantNames;
#endif
            };

            bool DoUpdate(const Scene& parentScene);
            DrawPacketBuiltRequestPtr CreateDrawPacketBuiltRequestInternal(
                const Scene& parentScene,
                bool useFallbackShaders,
                const DrawSrgReuseDataList* drawSrgSourceData,
                bool updateReusedDrawSrgs);
            DrawPacketBuiltRequestPtr CreateDeferredDrawPacketBuiltRequestInternal(
                const Scene& parentScene,
                bool useFallbackShaders,
                bool prepareDrawSrgs);
            bool PreparePipelineStateBuildItems(
                const Scene& parentScene,
                bool useFallbackShaders,
                PipelineStateBuildItemList& pipelineStateBuildItems,
                DrawPacketBuildData& drawPacketBuildData,
                const DrawSrgReuseDataList* drawSrgSourceData,
                bool updateReusedDrawSrgs,
                bool prepareDrawSrgs = true,
                const PipelineStateBuildItemList* reusablePipelineStateBuildItems = nullptr);
            bool BuildDrawPacket(
                DrawPacketBuildData&& drawPacketBuildData,
                PipelineStateBuildItemList&& pipelineStateBuildItems,
                AZStd::span<const RHI::ConstPtr<RHI::PipelineState>> pipelineStates,
                bool useFallbackShaders);
            Data::Instance<Shader> FindOrCreateCachedShader(
                const Data::Asset<ShaderAsset>& shaderAsset,
                const Name& supervariantName = Name{});
            RHI::DrawListTag ResolveDrawListTag(const ShaderCollection::Item& shaderItem) const;
            void ForValidShaderOptionName(const Name& shaderOptionName, const AZStd::function<bool(const ShaderCollection::Item&, ShaderOptionIndex)>& callback);

            Ptr<RHI::DrawPacket> m_drawPacket;

            // Note, many of the following items are held locally in the MeshDrawPacket solely to keep them resident in memory as long as they are needed
            // for the m_drawPacket. RHI::DrawPacket uses raw pointers only, but we use smart pointers here to hold on to the data.

            // Maintains references to the shader instances to keep their PSO caches resident (see Shader::Shutdown())
            ShaderList m_activeShaders;

            // Keeps the non-specialized shader instances and their fallback PSO caches resident
            // after the specialized packet replaces the fallback packet.
            AZStd::fixed_vector<Data::Instance<Shader>, RHI::DrawPacketBuilder::DrawItemCountMax> m_retainedFallbackShaders;
            AZStd::fixed_vector<RHI::ConstPtr<RHI::PipelineState>, RHI::DrawPacketBuilder::DrawItemCountMax>
                m_retainedFallbackPipelineStates;
            DrawSrgReuseDataList m_retainedFallbackDrawSrgReuseData;
            AZ::HashValue64 m_retainedFallbackFingerprint;
            uint64_t m_pipelineStateRevision = 0;
            uint64_t m_retainedFallbackRevision = 0;
            uint64_t m_fallbackWarmupRevision = 0;

            RHI::ConstPtr<RHI::ConstantsLayout> m_rootConstantsLayout;

            // The model that contains the mesh being represented by the DrawPacket
            Data::Instance<ModelLod> m_modelLod;

            // The index of the mesh within m_modelLod that is represented by the DrawPacket
            size_t m_modelLodMeshIndex;

            // The per-object shader resource group
            Data::Instance<ShaderResourceGroup> m_objectSrg;

            // We hold ConstPtr<RHI::ShaderResourceGroup> instead of Instance<RPI::ShaderResourceGroup> because the Material class
            // does not allow public access to its Instance<RPI::ShaderResourceGroup>.
            ConstPtr<RHI::ShaderResourceGroup> m_materialSrg;

            DrawSrgReuseDataList m_drawSrgReuseData;
            mutable AZStd::fixed_vector<CachedDrawListTag, RHI::DrawPacketBuilder::DrawItemCountMax>
                m_cachedDrawListTags;
            AZStd::fixed_vector<
                CachedShaderInstance,
                RHI::DrawPacketBuilder::DrawItemCountMax * 2>
                m_cachedShaderInstances;

            // A reference to the material, used to rebuild the DrawPacket if needed
            Data::Instance<Material> m_material;

            // Tracks whether the Material has change since the DrawPacket was last built
            Material::ChangeId m_materialChangeId = Material::DEFAULT_CHANGE_ID;

            // A handler which is called when a shader variant of the material is ready 
            Material::OnMaterialShaderVariantReadyEvent::Handler m_shaderVariantHandler;

            // Set the sort key for the draw packet
            RHI::DrawItemSortKey m_sortKey = 0;

            // Set the stencil value for this draw packet
            uint8_t m_stencilRef = 0;

            //! A map matches the index of UV names of this material to the custom names from the model.
            MaterialModelUvOverrideMap m_materialModelUvMap;

            //! List of shader options set for this specific draw packet
            typedef AZStd::pair<Name, RPI::ShaderOptionValue> ShaderOptionPair;
            typedef AZStd::vector<ShaderOptionPair> ShaderOptionVector;
            ShaderOptionVector m_shaderOptions;

            //! A draw list mask which is used to filter draw items which are packed into the DrawPacket
            RHI::DrawListMask m_drawListFilter;

            //! A flag to indicate if the DrawPacket need to be rebuild when updating
            bool m_needUpdate = true;

#ifdef DEBUG_MESH_SHADERVARIANTS
            // For debug shader variants
            // The list of shader variant asset names used by the DrawPackets
            AZStd::vector<AZStd::string_view> m_shaderVariantNames;
#endif
        };

        //! Pairs a generic pipeline-state build request with the mesh-specific data needed for publication.
        //! It deliberately has no pointer back to the MeshDrawPacket.
        class MeshDrawPacket::DrawPacketBuiltRequest
        {
        public:
            ~DrawPacketBuiltRequest();

            const PipelineStateBuildRequestPtr& GetPipelineStateBuildRequest() const;

        private:
            friend class MeshDrawPacket;

            DrawPacketBuiltRequest() = default;

            PipelineStateBuildRequestPtr m_pipelineStateBuildRequest;
            DrawPacketBuildData m_drawPacketBuildData;
            bool m_useFallbackShaders = false;
            uint64_t m_fallbackStateRevision = 0;
        };

        //! Immutable fallback shader/descriptor/PSO state shared by structurally equivalent
        //! mesh packets. Packet-specific SRGs and draw data are deliberately excluded.
        class MeshDrawPacket::FallbackPipelineStateBundle
        {
        private:
            friend class MeshDrawPacket;

            PipelineStateBuildItemList m_pipelineStateBuildItems;
            PipelineStateList m_pipelineStates;
            AZ::HashValue64 m_fingerprint;
        };
        
        using MeshDrawPacketList = AZStd::vector<RPI::MeshDrawPacket>;
        using MeshDrawPacketLods = AZStd::fixed_vector<MeshDrawPacketList, RPI::ModelLodAsset::LodCountMax>;

    } // namespace RPI
} // namespace AZ
