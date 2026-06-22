/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/Feature/OrderIndependentTransparency/OitMethod.h>
#include <Atom/RHI.Reflect/ShaderInputNameIndex.h>
#include <Atom/RPI.Public/FeatureProcessor.h>
#include <AtomCore/Instance/Instance.h>

namespace AZ
{
    namespace RPI
    {
        class ParentPass;
        class Pass;
        class ShaderResourceGroup;
    }

    namespace Render
    {
        //! Controls the transparent pass graph for opt-in order independent transparency.
        //! This feature processor intentionally does not submit draw packets; transparent items continue to
        //! enter Atom through the existing transparent draw list.
        class OrderIndependentTransparencyFeatureProcessor final
            : public RPI::FeatureProcessor
        {
        public:
            AZ_CLASS_ALLOCATOR(OrderIndependentTransparencyFeatureProcessor, SystemAllocator)
            AZ_RTTI(
                AZ::Render::OrderIndependentTransparencyFeatureProcessor,
                "{E7F4E2D2-501E-4AA4-A398-776FE3C5BB54}",
                AZ::RPI::FeatureProcessor);

            static void Reflect(AZ::ReflectContext* context);
            static uint32_t GetMlabLayerCount();
            static uint32_t GetMboitMomentCount();

            OrderIndependentTransparencyFeatureProcessor() = default;
            ~OrderIndependentTransparencyFeatureProcessor() override = default;

            void Activate() override;
            void Deactivate() override;
            void Render(const RenderPacket& packet) override;
            void OnSettingsCVarChanged();

        private:
            AZ_DISABLE_COPY_MOVE(OrderIndependentTransparencyFeatureProcessor);

            struct Settings
            {
                // Requested OIT mode from r_oitMethod. The active mode may still fall back to Off if the pass graph or RHI cannot support it.
                OitMethod m_method = OitMethod::Off;
                uint32_t m_mlabLayerCount = 4;
                uint32_t m_mlabDebugMode = 0;
                float m_wboitWeightScale = 1.0f;
                float m_wboitWeightBias = 0.01f;
                float m_wboitWeightMax = 3000.0f;
                uint32_t m_wboitDebugMode = 0;
                uint32_t m_mboitMomentCount = 4;
                float m_mboitMomentBias = 0.001f;
                float m_mboitOverestimation = 0.25f;
                uint32_t m_mboitDebugMode = 0;

                bool operator==(const Settings& rhs) const;
                bool operator!=(const Settings& rhs) const;
            };

            void OnRenderPipelineChanged(
                RPI::RenderPipeline* pipeline,
                RPI::SceneNotification::RenderPipelineChangeType changeType) override;

            Settings ReadSettings() const;
            bool IsRovSupported() const;
            // OIT pass requests are inserted under TransparentParentTemplate, so keep OIT disabled if the active pipeline does not expose it.
            bool HasTransparentParentPass() const;
            OitMethod GetActiveMethod(const Settings& settings);
            void UpdatePasses();
            // Maintains the mode-exclusive OIT parent pass under each TransparentParentTemplate instance.
            void UpdateOitParentPasses(OitMethod activeMethod);
            void UpdateOitParentPass(RPI::ParentPass* transparentParentPass, OitMethod activeMethod, OitMethod method, AZ::Name passName, const char* passRequestAssetPath);
            void QueueOitParentPassesForBuild(AZ::Name templateName);
            void SetPassesEnabled(AZ::Name templateName, bool enabled);
            void UpdateSceneSrgConstants(const Settings& settings) const;

            Settings m_lastAppliedSettings;
            // Last mode that actually reached the pass graph and global shader option after fallback checks.
            OitMethod m_lastActiveMethod = OitMethod::Off;
            Data::Instance<RPI::ShaderResourceGroup> m_sceneSrg = nullptr;

            const AZ::Name m_transparentParentTemplateName = AZ::Name("TransparentParentTemplate");
            const AZ::Name m_transparentPassTemplateName = AZ::Name("TransparentPassTemplate");
            const AZ::Name m_oitMlabParentPassName = AZ::Name("OitMlabParentPass");
            const AZ::Name m_oitMlabParentTemplateName = AZ::Name("OitMlabParentTemplate");
            const AZ::Name m_oitWboitParentPassName = AZ::Name("OitWboitParentPass");
            const AZ::Name m_oitWboitParentTemplateName = AZ::Name("OitWboitParentTemplate");
            const AZ::Name m_oitMboitParentPassName = AZ::Name("OitMboitParentPass");
            const AZ::Name m_oitMboitParentTemplateName = AZ::Name("OitMboitParentTemplate");
            const AZ::Name m_oitMethodShaderOptionName = AZ::Name("o_oitMethod");
            const char* m_oitMlabParentPassRequestAssetPath = "Passes/OitMlabParentPassRequest.azasset";
            const char* m_oitWboitParentPassRequestAssetPath = "Passes/OitWboitParentPassRequest.azasset";
            const char* m_oitMboitParentPassRequestAssetPath = "Passes/OitMboitParentPassRequest.azasset";
            mutable RHI::ShaderInputNameIndex m_oitMlabLayerCountIndex = RHI::ShaderInputNameIndex("m_oitMlabLayerCount");
            mutable RHI::ShaderInputNameIndex m_oitMlabDebugModeIndex = RHI::ShaderInputNameIndex("m_oitMlabDebugMode");
            mutable RHI::ShaderInputNameIndex m_oitWboitWeightScaleIndex = RHI::ShaderInputNameIndex("m_oitWboitWeightScale");
            mutable RHI::ShaderInputNameIndex m_oitWboitWeightBiasIndex = RHI::ShaderInputNameIndex("m_oitWboitWeightBias");
            mutable RHI::ShaderInputNameIndex m_oitWboitWeightMaxIndex = RHI::ShaderInputNameIndex("m_oitWboitWeightMax");
            mutable RHI::ShaderInputNameIndex m_oitWboitDebugModeIndex = RHI::ShaderInputNameIndex("m_oitWboitDebugMode");
            mutable RHI::ShaderInputNameIndex m_oitMboitMomentCountIndex = RHI::ShaderInputNameIndex("m_oitMboitMomentCount");
            mutable RHI::ShaderInputNameIndex m_oitMboitMomentBiasIndex = RHI::ShaderInputNameIndex("m_oitMboitMomentBias");
            mutable RHI::ShaderInputNameIndex m_oitMboitOverestimationIndex = RHI::ShaderInputNameIndex("m_oitMboitOverestimation");
            mutable RHI::ShaderInputNameIndex m_oitMboitDebugModeIndex = RHI::ShaderInputNameIndex("m_oitMboitDebugMode");
        };
    } // namespace Render
} // namespace AZ
