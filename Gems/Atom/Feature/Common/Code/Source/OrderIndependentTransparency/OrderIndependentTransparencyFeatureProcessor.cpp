/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <OrderIndependentTransparency/OrderIndependentTransparencyFeatureProcessor.h>

#include <Atom/RHI/Device.h>
#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h>
#include <Atom/RPI.Reflect/Pass/PassRequest.h>
#include <Atom/RPI.Reflect/System/AnyAsset.h>
#include <Atom/RPI.Public/Pass/Pass.h>
#include <Atom/RPI.Public/Pass/PassFilter.h>
#include <Atom/RPI.Public/Pass/ParentPass.h>
#include <Atom/RPI.Public/Pass/PassSystemInterface.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RPI.Public/Shader/ShaderSystemInterface.h>
#include <AzCore/Console/IConsole.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>
#include <AzFramework/Entity/GameEntityContextBus.h>

namespace AZ
{
    namespace Render
    {
        namespace
        {
            void NotifyOitSettingsChanged()
            {
                // CVars live outside the feature processor, so resolve the game scene each time a setting changes.
                AzFramework::EntityContextId entityContextId;
                AzFramework::GameEntityContextRequestBus::BroadcastResult(
                    entityContextId, &AzFramework::GameEntityContextRequestBus::Events::GetGameEntityContextId);

                if (auto scene = AZ::RPI::Scene::GetSceneForEntityContextId(entityContextId); scene != nullptr)
                {
                    auto featureProcessor = scene->GetFeatureProcessor<AZ::Render::OrderIndependentTransparencyFeatureProcessor>();
                    if (featureProcessor)
                    {
                        featureProcessor->OnSettingsCVarChanged();
                    }
                }
            }

            void OnOitUint32CVarChanged([[maybe_unused]] const uint32_t& value)
            {
                NotifyOitSettingsChanged();
            }

            void OnOitFloatCVarChanged([[maybe_unused]] const float& value)
            {
                NotifyOitSettingsChanged();
            }

        } // namespace

        AZ_CVAR(uint32_t, r_oitMethod, 0, OnOitUint32CVarChanged, AZ::ConsoleFunctorFlags::Null, "Order independent transparency method. 0=Off, 1=MLAB, 2=WBOIT, 3=MBOIT.");
        AZ_CVAR(uint32_t, r_oitMlabLayerCount, 4, OnOitUint32CVarChanged, AZ::ConsoleFunctorFlags::Null, "MLAB fragment count per pixel. Valid range is 1-8.");
        AZ_CVAR(uint32_t, r_oitMlabDebugMode, 0, OnOitUint32CVarChanged, AZ::ConsoleFunctorFlags::Null, "MLAB debug output mode. 0=normal, 1=debug.");
        AZ_CVAR(float, r_oitWboitWeightScale, 1.0f, OnOitFloatCVarChanged, AZ::ConsoleFunctorFlags::Null, "WBOIT fragment weight scale.");
        AZ_CVAR(float, r_oitWboitWeightBias, 0.01f, OnOitFloatCVarChanged, AZ::ConsoleFunctorFlags::Null, "WBOIT minimum fragment weight bias.");
        AZ_CVAR(float, r_oitWboitWeightMax, 3000.0f, OnOitFloatCVarChanged, AZ::ConsoleFunctorFlags::Null, "WBOIT maximum fragment weight.");
        AZ_CVAR(uint32_t, r_oitWboitDebugMode, 0, OnOitUint32CVarChanged, AZ::ConsoleFunctorFlags::Null, "WBOIT debug output mode. 0=normal, 1=accum weight, 2=revealage.");
        AZ_CVAR(uint32_t, r_oitMboitMomentCount, 4, OnOitUint32CVarChanged, AZ::ConsoleFunctorFlags::Null, "MBOIT moment count. Valid values are 4 or 6.");
        AZ_CVAR(float, r_oitMboitMomentBias, 0.001f, OnOitFloatCVarChanged, AZ::ConsoleFunctorFlags::Null, "MBOIT moment reconstruction bias.");
        AZ_CVAR(float, r_oitMboitOverestimation, 0.25f, OnOitFloatCVarChanged, AZ::ConsoleFunctorFlags::Null, "MBOIT overestimation factor used during resolve.");
        AZ_CVAR(uint32_t, r_oitMboitDebugMode, 0, OnOitUint32CVarChanged, AZ::ConsoleFunctorFlags::Null, "MBOIT debug output mode. 0=normal, 1=total optical depth, 2=moment variance.");

        void OrderIndependentTransparencyFeatureProcessor::Reflect(ReflectContext* context)
        {
            if (auto* serializeContext = azrtti_cast<SerializeContext*>(context))
            {
                serializeContext
                    ->Class<OrderIndependentTransparencyFeatureProcessor, RPI::FeatureProcessor>()
                    ->Version(0);
            }
        }

        void OrderIndependentTransparencyFeatureProcessor::Activate()
        {
            m_sceneSrg = GetParentScene()->GetShaderResourceGroup();
            EnableSceneNotification();
            UpdatePasses();
        }

        void OrderIndependentTransparencyFeatureProcessor::Deactivate()
        {
            RPI::ShaderSystemInterface::Get()->SetGlobalShaderOption(m_oitMethodShaderOptionName, RPI::ShaderOptionValue{ static_cast<uint32_t>(OitMethod::Off) });
            DisableSceneNotification();
            m_sceneSrg = nullptr;
        }

        void OrderIndependentTransparencyFeatureProcessor::Render([[maybe_unused]] const RenderPacket& packet)
        {
            const Settings settings = ReadSettings();
            const OitMethod activeMethod = GetActiveMethod(settings);

            if (settings != m_lastAppliedSettings || activeMethod != m_lastActiveMethod)
            {
                UpdatePasses();
            }
        }

        bool OrderIndependentTransparencyFeatureProcessor::Settings::operator==(const Settings& rhs) const
        {
            return m_method == rhs.m_method
                && m_mlabLayerCount == rhs.m_mlabLayerCount
                && m_mlabDebugMode == rhs.m_mlabDebugMode
                && m_wboitWeightScale == rhs.m_wboitWeightScale
                && m_wboitWeightBias == rhs.m_wboitWeightBias
                && m_wboitWeightMax == rhs.m_wboitWeightMax
                && m_wboitDebugMode == rhs.m_wboitDebugMode
                && m_mboitMomentCount == rhs.m_mboitMomentCount
                && m_mboitMomentBias == rhs.m_mboitMomentBias
                && m_mboitOverestimation == rhs.m_mboitOverestimation
                && m_mboitDebugMode == rhs.m_mboitDebugMode;
        }

        bool OrderIndependentTransparencyFeatureProcessor::Settings::operator!=(const Settings& rhs) const
        {
            return !(*this == rhs);
        }

        void OrderIndependentTransparencyFeatureProcessor::OnRenderPipelineChanged(
            [[maybe_unused]] RPI::RenderPipeline* pipeline,
            RPI::SceneNotification::RenderPipelineChangeType changeType)
        {
            if (changeType == RPI::SceneNotification::RenderPipelineChangeType::Added
                || changeType == RPI::SceneNotification::RenderPipelineChangeType::PassChanged)
            {
                UpdatePasses();
            }
        }

        void OrderIndependentTransparencyFeatureProcessor::OnSettingsCVarChanged()
        {
            UpdatePasses();
        }

        uint32_t OrderIndependentTransparencyFeatureProcessor::GetMlabLayerCount()
        {
            return AZStd::clamp<uint32_t>(r_oitMlabLayerCount, 1, 8);
        }

        uint32_t OrderIndependentTransparencyFeatureProcessor::GetMboitMomentCount()
        {
            return r_oitMboitMomentCount == 6u ? 6u : 4u;
        }

        OrderIndependentTransparencyFeatureProcessor::Settings OrderIndependentTransparencyFeatureProcessor::ReadSettings() const
        {
            Settings settings;
            settings.m_method = r_oitMethod == static_cast<uint32_t>(OitMethod::Mlab)
                ? OitMethod::Mlab
                : r_oitMethod == static_cast<uint32_t>(OitMethod::Wboit)
                    ? OitMethod::Wboit
                    : r_oitMethod == static_cast<uint32_t>(OitMethod::Mboit)
                        ? OitMethod::Mboit
                        : OitMethod::Off;
            settings.m_mlabLayerCount = GetMlabLayerCount();
            settings.m_mlabDebugMode = r_oitMlabDebugMode;
            settings.m_wboitWeightScale = AZStd::max(0.0f, static_cast<float>(r_oitWboitWeightScale));
            settings.m_wboitWeightBias = AZStd::max(0.0f, static_cast<float>(r_oitWboitWeightBias));
            settings.m_wboitWeightMax = AZStd::max(settings.m_wboitWeightBias, static_cast<float>(r_oitWboitWeightMax));
            settings.m_wboitDebugMode = r_oitWboitDebugMode;
            settings.m_mboitMomentCount = GetMboitMomentCount();
            settings.m_mboitMomentBias = AZStd::max(0.0f, static_cast<float>(r_oitMboitMomentBias));
            settings.m_mboitOverestimation = AZStd::max(0.0f, static_cast<float>(r_oitMboitOverestimation));
            settings.m_mboitDebugMode = r_oitMboitDebugMode;
            return settings;
        }

        bool OrderIndependentTransparencyFeatureProcessor::IsRovSupported() const
        {
            RHI::RHISystemInterface* rhiSystem = RHI::RHISystemInterface::Get();
            if (!rhiSystem)
            {
                return false;
            }

            RHI::Ptr<RHI::Device> device = rhiSystem->GetDevice();
            return device && device->GetFeatures().m_rasterizerOrderedViews;
        }

        bool OrderIndependentTransparencyFeatureProcessor::HasTransparentParentPass() const
        {
            bool hasTransparentParentPass = false;
            RPI::PassFilter passFilter = RPI::PassFilter::CreateWithTemplateName(m_transparentParentTemplateName, GetParentScene());
            RPI::PassSystemInterface::Get()->ForEachPass(
                passFilter,
                [&hasTransparentParentPass](RPI::Pass* pass) -> RPI::PassFilterExecutionFlow
                {
                    if (pass && pass->AsParent())
                    {
                        hasTransparentParentPass = true;
                        return RPI::PassFilterExecutionFlow::StopVisitingPasses;
                    }

                    return RPI::PassFilterExecutionFlow::ContinueVisitingPasses;
                });

            return hasTransparentParentPass;
        }

        OitMethod OrderIndependentTransparencyFeatureProcessor::GetActiveMethod(const Settings& settings)
        {
            if (settings.m_method == OitMethod::Off)
            {
                return OitMethod::Off;
            }

            // Every current OIT mode writes through rasterizer ordered views. Keep sorted transparency if the backend cannot serialize writes.
            if (!IsRovSupported())
            {
                AZ_WarningOnce(
                    "OrderIndependentTransparency",
                    false,
                    "OIT method %u requested, but this RHI device does not expose rasterizer ordered fragment writes. Falling back to sorted transparency.",
                    static_cast<uint32_t>(settings.m_method));

                return OitMethod::Off;
            }

            // The pass requests are children of TransparentParentTemplate; without it there is nowhere safe to attach OIT.
            if (!HasTransparentParentPass())
            {
                AZ_WarningOnce(
                    "OrderIndependentTransparency",
                    false,
                    "OIT method %u requested, but no parent pass using %s was found in the active render pipeline. Falling back to sorted transparency.",
                    static_cast<uint32_t>(settings.m_method),
                    m_transparentParentTemplateName.GetCStr());

                return OitMethod::Off;
            }

            return settings.m_method;
        }

        void OrderIndependentTransparencyFeatureProcessor::UpdatePasses()
        {
            const Settings settings = ReadSettings();
            const OitMethod activeMethod = GetActiveMethod(settings);

            // Publish Off when fallback is active so shader variants, custom shaders, and PopcornFX all stay on sorted transparency.
            UpdateSceneSrgConstants(settings);
            RPI::ShaderSystemInterface::Get()->SetGlobalShaderOption(m_oitMethodShaderOptionName, RPI::ShaderOptionValue{ static_cast<uint32_t>(activeMethod) });
            UpdateOitParentPasses(activeMethod);
            if (activeMethod == OitMethod::Mlab && settings.m_mlabLayerCount != m_lastAppliedSettings.m_mlabLayerCount)
            {
                QueueOitParentPassesForBuild(m_oitMlabParentTemplateName);
            }
            if (activeMethod == OitMethod::Mboit && settings.m_mboitMomentCount != m_lastAppliedSettings.m_mboitMomentCount)
            {
                QueueOitParentPassesForBuild(m_oitMboitParentTemplateName);
            }
            SetPassesEnabled(m_transparentPassTemplateName, activeMethod == OitMethod::Off);

            m_lastAppliedSettings = settings;
            m_lastActiveMethod = activeMethod;
        }

        void OrderIndependentTransparencyFeatureProcessor::UpdateOitParentPasses(OitMethod activeMethod)
        {
            RPI::PassFilter passFilter = RPI::PassFilter::CreateWithTemplateName(m_transparentParentTemplateName, GetParentScene());
            RPI::PassSystemInterface::Get()->ForEachPass(
                passFilter,
                [this, activeMethod](RPI::Pass* pass) -> RPI::PassFilterExecutionFlow
                {
                    RPI::ParentPass* transparentParentPass = pass ? pass->AsParent() : nullptr;
                    if (!transparentParentPass)
                    {
                        AZ_Warning(
                            "OrderIndependentTransparency",
                            false,
                            "Pass %s uses %s but is not a parent pass.",
                            pass ? pass->GetName().GetCStr() : "<null>",
                            m_transparentParentTemplateName.GetCStr());
                        return RPI::PassFilterExecutionFlow::ContinueVisitingPasses;
                    }

                    // Keep only one OIT mode parent alive under each transparent parent.
                    UpdateOitParentPass(transparentParentPass, activeMethod, OitMethod::Mlab, m_oitMlabParentPassName, m_oitMlabParentPassRequestAssetPath);
                    UpdateOitParentPass(transparentParentPass, activeMethod, OitMethod::Wboit, m_oitWboitParentPassName, m_oitWboitParentPassRequestAssetPath);
                    UpdateOitParentPass(transparentParentPass, activeMethod, OitMethod::Mboit, m_oitMboitParentPassName, m_oitMboitParentPassRequestAssetPath);

                    return RPI::PassFilterExecutionFlow::ContinueVisitingPasses;
                });
        }

        void OrderIndependentTransparencyFeatureProcessor::UpdateOitParentPass(
            RPI::ParentPass* transparentParentPass,
            OitMethod activeMethod,
            OitMethod method,
            AZ::Name passName,
            const char* passRequestAssetPath)
        {
            RPI::Ptr<RPI::Pass> oitParentPass = transparentParentPass->FindChildPass(passName);
            if (activeMethod == method)
            {
                if (!oitParentPass)
                {
                    // Load the pass request asset at activation time so pipelines that do not use OIT do not pay for these children.
                    Data::Asset<RPI::AnyAsset> passRequestAsset = RPI::AssetUtils::LoadAssetByProductPath<RPI::AnyAsset>(
                        passRequestAssetPath, RPI::AssetUtils::TraceLevel::Warning);
                    const RPI::PassRequest* passRequest = nullptr;
                    if (passRequestAsset && passRequestAsset->IsReady())
                    {
                        passRequest = passRequestAsset->GetDataAs<RPI::PassRequest>();
                    }

                    if (!passRequest)
                    {
                        AZ_Error("OrderIndependentTransparency", false, "Can't load PassRequest from %s.", passRequestAssetPath);
                        return;
                    }

                    oitParentPass = RPI::PassSystemInterface::Get()->CreatePassFromRequest(passRequest);
                    if (oitParentPass)
                    {
                        transparentParentPass->AddChild(oitParentPass, true);
                    }
                }

                if (oitParentPass)
                {
                    oitParentPass->SetEnabled(true);
                }
            }
            else if (oitParentPass)
            {
                oitParentPass->SetEnabled(false);
                oitParentPass->QueueForRemoval();
            }
        }

        void OrderIndependentTransparencyFeatureProcessor::QueueOitParentPassesForBuild(AZ::Name templateName)
        {
            RPI::PassFilter passFilter = RPI::PassFilter::CreateWithTemplateName(templateName, GetParentScene());
            RPI::PassSystemInterface::Get()->ForEachPass(
                passFilter,
                [](RPI::Pass* pass) -> RPI::PassFilterExecutionFlow
                {
                    pass->QueueForBuildAndInitialization();
                    return RPI::PassFilterExecutionFlow::ContinueVisitingPasses;
                });
        }

        void OrderIndependentTransparencyFeatureProcessor::SetPassesEnabled(AZ::Name templateName, bool enabled)
        {
            RPI::PassFilter passFilter = RPI::PassFilter::CreateWithTemplateName(templateName, GetParentScene());
            RPI::PassSystemInterface::Get()->ForEachPass(
                passFilter,
                [enabled](RPI::Pass* pass) -> RPI::PassFilterExecutionFlow
                {
                    pass->SetEnabled(enabled);
                    return RPI::PassFilterExecutionFlow::ContinueVisitingPasses;
                });
        }

        void OrderIndependentTransparencyFeatureProcessor::UpdateSceneSrgConstants(const Settings& settings) const
        {
            if (!m_sceneSrg)
            {
                return;
            }

            m_sceneSrg->SetConstant(m_oitMlabLayerCountIndex, settings.m_mlabLayerCount);
            m_sceneSrg->SetConstant(m_oitMlabDebugModeIndex, settings.m_mlabDebugMode);
            m_sceneSrg->SetConstant(m_oitWboitWeightScaleIndex, settings.m_wboitWeightScale);
            m_sceneSrg->SetConstant(m_oitWboitWeightBiasIndex, settings.m_wboitWeightBias);
            m_sceneSrg->SetConstant(m_oitWboitWeightMaxIndex, settings.m_wboitWeightMax);
            m_sceneSrg->SetConstant(m_oitWboitDebugModeIndex, settings.m_wboitDebugMode);
            m_sceneSrg->SetConstant(m_oitMboitMomentCountIndex, settings.m_mboitMomentCount);
            m_sceneSrg->SetConstant(m_oitMboitMomentBiasIndex, settings.m_mboitMomentBias);
            m_sceneSrg->SetConstant(m_oitMboitOverestimationIndex, settings.m_mboitOverestimation);
            m_sceneSrg->SetConstant(m_oitMboitDebugModeIndex, settings.m_mboitDebugMode);
        }
    } // namespace Render
} // namespace AZ
