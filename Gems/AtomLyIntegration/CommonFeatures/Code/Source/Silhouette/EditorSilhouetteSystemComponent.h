/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/Component/Component.h>
#include <AzToolsFramework/Entity/EditorEntityContextBus.h>
#include <AzToolsFramework/Viewport/ViewportMessages.h>
#include <Atom/RPI.Public/ViewportContextBus.h>
#include <Atom/RPI.Public/ViewportContext.h>

namespace AZ::Render
{
    // Handles disabling the silhouette in the Editor when not in game mode
    class EditorSilhouetteSystemComponent
        : public AZ::Component
        , protected AzToolsFramework::EditorEntityContextNotificationBus::Handler
        , protected AzToolsFramework::ViewportInteraction::ViewportSettingsNotificationBus::Handler
        , public AZ::RPI::ViewportContextManagerNotificationsBus::Handler
    {
    public:
        AZ_COMPONENT(EditorSilhouetteSystemComponent, "{719B4528-1BA9-41BD-8C18-A751DCF2EF87}");

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

    protected:
        // AZ::Component interface overrides...
        void Init() override;
        void Activate() override;
        void Deactivate() override;

        // EditorEntityContextNotificationBus overrides ...
        void OnStartPlayInEditor() override;
        void OnStopPlayInEditor() override;

        // AzToolsFramework::ViewportInteraction::ViewportSettingsNotificationBus::Handler overrides ...
        void OnSilhouettesVisibilityChanged(bool enabled) override;

        // ViewportContextManagerNotificationsBus overrides
        void OnViewportContextAdded(AZ::RPI::ViewportContextPtr viewportContext) override;

        // Handles enabling/disabling the silouehette feature processor when a new scene is added.
        AZ::RPI::ViewportContext::SceneChangedEvent::Handler m_sceneChangeHandler;
    };
} // namespace AZ::Render
