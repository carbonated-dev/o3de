/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Source/AutoGen/NetworkTransformComponent.AutoComponent.h>
#include <Multiplayer/Components/NetBindComponent.h>
#include <AzCore/Component/TransformBus.h>

namespace Multiplayer
{
    class NetworkTransformComponent
        : public NetworkTransformComponentBase
    {
    public:
        AZ_MULTIPLAYER_COMPONENT(Multiplayer::NetworkTransformComponent, s_networkTransformComponentConcreteUuid, Multiplayer::NetworkTransformComponentBase);

        static void Reflect(AZ::ReflectContext* context);

        NetworkTransformComponent();

        void OnInit() override;
        void OnActivate(Multiplayer::EntityIsMigrating entityIsMigrating) override;
        void OnDeactivate(Multiplayer::EntityIsMigrating entityIsMigrating) override;

#if defined(CARBONATED)
        // For the purposes of local prediction, if the input rate is the default (30fps), the character has jitter when the client is at a higher render fps.
        // This change allows the client to ignore syncs to the local network transform and manually request them, and
        // then interpolate the transform changes at render time instead of update time, which results in smoother movement. 
        void PauseAutoTransformSync(bool pause);
        void RefreshCachedTransform();
#endif

    private:
        void OnPreRender(float deltaTime);
        void OnCorrection();
        void OnTransformChanged();
        void OnParentChanged(NetEntityId parentId);
        
        EntityPreRenderEvent::Handler m_entityPreRenderEventHandler;
        EntityCorrectionEvent::Handler m_entityCorrectionEventHandler;
        AZ::Event<AZ::Quaternion>::Handler m_rotationChangedEventHandler;
        AZ::Event<AZ::Vector3>::Handler m_translationChangedEventHandler;
        AZ::Event<float>::Handler m_scaleChangedEventHandler;
        AZ::Event<NetEntityId>::Handler m_parentChangedEventHandler;
        AZ::Event<uint8_t>::Handler m_resetCountChangedEventHandler;

        Multiplayer::HostFrameId m_targetHostFrameId = HostFrameId{ 0 };
        bool m_syncTransformImmediate = false;
    };

    class NetworkTransformComponentController
        : public NetworkTransformComponentControllerBase
    {
    public:
        NetworkTransformComponentController(NetworkTransformComponent& parent);

        void OnActivate(Multiplayer::EntityIsMigrating entityIsMigrating) override;
        void OnDeactivate(Multiplayer::EntityIsMigrating entityIsMigrating) override;

#if AZ_TRAIT_SERVER
        void HandleMultiplayerTeleport(AzNetworking::IConnection* invokingConnection, const AZ::Vector3& teleportToPosition) override;
#endif

#if defined(CARBONATED)
        void PauseAutoTransformSync(bool pause);
        void RefreshCachedTransform();
#endif
    private:
        void OnTransformChangedEvent(const AZ::Transform& localTm, const AZ::Transform& worldTm);
        void OnParentIdChangedEvent(AZ::EntityId oldParent, AZ::EntityId newParent);

        AZ::TransformChangedEvent::Handler m_transformChangedHandler;
        AZ::ParentChangedEvent::Handler m_parentIdChangedHandler;
    };
}
