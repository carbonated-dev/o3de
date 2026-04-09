/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Silhouette/EditorSilhouetteSystemComponent.h>

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>

#include <AssetBuilderSDK/AssetBuilderSDK.h>
#include <AzToolsFramework/API/EditorAssetSystemAPI.h>
#include <AzFramework/Application/Application.h>

#include <AzFramework/Entity/GameEntityContextBus.h>
#include <AzFramework/Scene/Scene.h>
#include <AzFramework/Scene/SceneSystemInterface.h>
#include <Silhouette/SilhouetteFeatureProcessor.h>
#include <Atom/RPI.Public/Scene.h>

namespace AZ::Render
{
    //! Main system component for the Atom Silhouette Feature Gem's editor/tools module.
    void EditorSilhouetteSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<EditorSilhouetteSystemComponent, AZ::Component>()
                ->Version(1)
                ->Attribute(Edit::Attributes::SystemComponentTags, AZStd::vector<Crc32>({ AssetBuilderSDK::ComponentTags::AssetBuilder }))
                ;

            if (AZ::EditContext* ec = serialize->GetEditContext())
            {
                ec->Class<EditorSilhouetteSystemComponent>("Common", "Configures editor- and tool-specific functionality for common render features.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ;
            }
        }
    }

    void EditorSilhouetteSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("EditorSilhouetteService"));
    }

    void EditorSilhouetteSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("EditorSilhouetteService"));
    }

    void EditorSilhouetteSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC("CommonService", 0x6398eec4));
    }

    void EditorSilhouetteSystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        AZ_UNUSED(dependent);
    }

    void EditorSilhouetteSystemComponent::Init()
    {
    }

    void EditorSilhouetteSystemComponent::Activate()
    {
        constexpr AZ::s32 defaultSceneEntityDisplayId = AZ_CRC_CE("MainViewportEntityDisplayId");
        AzToolsFramework::ViewportInteraction::ViewportSettingsNotificationBus::Handler::BusConnect(defaultSceneEntityDisplayId);
        AzToolsFramework::EditorEntityContextNotificationBus::Handler::BusConnect();
        AZ::RPI::ViewportContextManagerNotificationsBus::Handler::BusConnect();
    }

    void EditorSilhouetteSystemComponent::Deactivate()
    {
        AZ::RPI::ViewportContextManagerNotificationsBus::Handler::BusDisconnect();
        AzToolsFramework::EditorEntityContextNotificationBus::Handler::BusDisconnect();
        AzToolsFramework::ViewportInteraction::ViewportSettingsNotificationBus::Handler::BusDisconnect();
    }

    AZ::Render::SilhouetteFeatureProcessor* GetFeatureProcessor()
    {
        AzFramework::EntityContextId entityContextId;
        AzFramework::GameEntityContextRequestBus::BroadcastResult(
            entityContextId, &AzFramework::GameEntityContextRequestBus::Events::GetGameEntityContextId);

        if (auto scene = AZ::RPI::Scene::GetSceneForEntityContextId(entityContextId); scene != nullptr)
        {
            return scene->GetFeatureProcessor<AZ::Render::SilhouetteFeatureProcessor>();
        }

        return nullptr;
    }

    bool IsSilhouettesVisible()
    {
        bool silhouetteVisible = true;
        AzToolsFramework::ViewportInteraction::ViewportSettingsRequestBus::BroadcastResult(
            silhouetteVisible, &AzToolsFramework::ViewportInteraction::ViewportSettingsRequestBus::Events::SilhouettesVisible);
        return silhouetteVisible;
    }

    void EditorSilhouetteSystemComponent::OnStartPlayInEditor()
    {
        if (auto featureProcessor = GetFeatureProcessor())
        {
            featureProcessor->SetPassesEnabled(true);
        }
    }

    void EditorSilhouetteSystemComponent::OnStopPlayInEditor()
    {
        if (auto featureProcessor = GetFeatureProcessor())
        {               
            featureProcessor->SetPassesEnabled(IsSilhouettesVisible());
        }
    }

    void EditorSilhouetteSystemComponent::OnSilhouettesVisibilityChanged([[maybe_unused]] bool enabled)
    {
        // Check if we are in game mode.
        bool isInGameMode = true;
        AzToolsFramework::EditorEntityContextRequestBus::BroadcastResult(
            isInGameMode, &AzToolsFramework::EditorEntityContextRequestBus::Events::IsEditorRunningGame);
        if (!isInGameMode)
        {
            OnStopPlayInEditor();
        }
    }

    void EditorSilhouetteSystemComponent::OnViewportContextAdded(AZ::RPI::ViewportContextPtr viewportContext)
    {
        auto setupScene = [](RPI::ScenePtr scene)
        {
            if (scene)
            {
                if (auto fp = scene->GetFeatureProcessor<AZ::Render::SilhouetteFeatureProcessor>())
                {
                    fp->SetPassesEnabled(IsSilhouettesVisible());
                }
            }
        };
        m_sceneChangeHandler = AZ::RPI::ViewportContext::SceneChangedEvent::Handler(setupScene);
        viewportContext->ConnectSceneChangedHandler(m_sceneChangeHandler);
    }
} // namespace AZ::Render
