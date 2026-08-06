/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Component/Entity.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Asset/AssetManager.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <Integration/Assets/ActorAsset.h>
#include <Integration/Editor/Components/EditorAnimMeshComponent.h>
#include <Integration/SimpleMotionComponentBus.h>
#include <EMotionFX/Source/MotionSystem.h>
#include <EMotionFX/Source/MotionInstance.h>
#include <EMotionFX/Source/MotionLayerSystem.h>

#if !defined(_RELEASE) && defined(AZ_PLATFORM_WINDOWS)
#pragma optimize("", off)
#endif

namespace EMotionFX
{
    namespace Integration
    {
        void EditorAnimMeshComponent::Reflect(AZ::ReflectContext* context)
        {
            auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
            if (serializeContext)
            {
                serializeContext->Class<EditorAnimMeshComponent, AzToolsFramework::Components::EditorComponentBase>()
                    ->Version(3)
                    ->Field("PreviewInEditor", &EditorAnimMeshComponent::m_previewInEditor)
                    ->Field("Configuration", &EditorAnimMeshComponent::m_configuration)
                    ;

                AZ::EditContext* editContext = serializeContext->GetEditContext();
                if (editContext)
                {
                    editContext->Class<EditorAnimMeshComponent>(
                        "Anim Mesh", "The Anim Mesh component")
                        ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Category, "Animation")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Icons/Components/SimpleMotion.svg")
                        ->Attribute(AZ::Edit::Attributes::PrimaryAssetType, azrtti_typeid<MotionAsset>())
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Icons/Components/Viewport/SimpleMotion.svg")
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game", 0x232b318c))
                        ->Attribute(AZ::Edit::Attributes::HelpPageURL, "https://o3de.org/docs/user-guide/components/reference/animation/simple-motion/")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->DataElement(0, &EditorAnimMeshComponent::m_previewInEditor, "Preview In Editor", "Plays motion in Editor")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorAnimMeshComponent::OnEditorPropertyChanged)
                        ->DataElement(0, &EditorAnimMeshComponent::m_configuration, "Configuration", "Settings for this Simple Motion")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorAnimMeshComponent::OnEditorPropertyChanged)
                        ;
                }
            }

            /*AZ::BehaviorContext* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
            if (behaviorContext)
            {
                behaviorContext->EBus<EditorAnimMeshComponentRequestBus>("EditorAnimMeshComponentRequestBus")
                    ->Event("SetPreviewInEditor", &EditorAnimMeshComponentRequestBus::Events::SetPreviewInEditor)
                    ->Event("GetPreviewInEditor", &EditorAnimMeshComponentRequestBus::Events::GetPreviewInEditor)
                    ->Attribute("Hidden", AZ::Edit::Attributes::PropertyHidden)
                    ->VirtualProperty("PreviewInEditor", "GetPreviewInEditor", "SetPreviewInEditor")
                    ->Event("GetAssetDuration", &EditorAnimMeshComponentRequestBus::Events::GetAssetDuration)
                    ;

                behaviorContext->Class<EditorAnimMeshComponent>()
                    ->RequestBus("SimpleMotionComponentRequestBus")
                    ->RequestBus("EditorAnimMeshComponentRequestBus")
                    ;
            }*/
        }

        EditorAnimMeshComponent::EditorAnimMeshComponent()
            : m_previewInEditor(false)
            , m_configuration()
            , m_actorInstance(nullptr)
            , m_motionInstance(nullptr)
            , m_lastMotionInstance(nullptr)
        {
        }

        EditorAnimMeshComponent::~EditorAnimMeshComponent()
        {
        }

        void EditorAnimMeshComponent::Activate()
        {
            AZ::Data::AssetBus::MultiHandler::BusDisconnect();
            SimpleMotionComponentRequestBus::Handler::BusConnect(GetEntityId());
            //EditorAnimMeshComponentRequestBus::Handler::BusConnect(GetEntityId());

            //check if our motion has changed
            VerifyMotionAssetState();

            AZ::Render::MeshComponentNotificationBus::Handler::BusConnect(GetEntityId());
            ActorComponentNotificationBus::Handler::BusConnect(GetEntityId());
        }

        void EditorAnimMeshComponent::Deactivate()
        {
            AZ::Render::MeshComponentNotificationBus::Handler::BusDisconnect();
            ActorComponentNotificationBus::Handler::BusDisconnect();
            //EditorAnimMeshComponentRequestBus::Handler::BusDisconnect();
            SimpleMotionComponentRequestBus::Handler::BusDisconnect();
            AZ::Data::AssetBus::MultiHandler::BusDisconnect();

            RemoveMotionInstanceFromActor(m_lastMotionInstance);
            m_lastMotionInstance = nullptr;
            RemoveMotionInstanceFromActor(m_motionInstance);
            m_motionInstance = nullptr;

            m_configuration.m_motionAsset.Release();
            m_lastMotionAsset.Release();

            m_actorInstance = nullptr;
        }

        void EditorAnimMeshComponent::VerifyMotionAssetState()
        {
            AZ::Data::AssetBus::MultiHandler::BusDisconnect();
            if (m_configuration.m_motionAsset.GetId().IsValid())
            {
                AZ::Data::AssetBus::MultiHandler::BusConnect(m_configuration.m_motionAsset.GetId());
                m_configuration.m_motionAsset.QueueLoad();
            }
        }

        void EditorAnimMeshComponent::OnModelReady([[maybe_unused]] const AZ::Data::Asset<AZ::RPI::ModelAsset>& modelAsset, [[maybe_unused]] const AZ::Data::Instance<AZ::RPI::Model>& model)
        {
            m_actor = aznew EMotionFX::Actor("EditorAnimMeshComponentActor");
            // m_actor->SetModel(modelAsset, model);
            m_actorInstance = EMotionFX::ActorInstance::Create(m_actor, GetEntity());
            PlayMotion();
        }

        void EditorAnimMeshComponent::OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset)
        {
            if (asset == m_configuration.m_motionAsset)
            {
                m_configuration.m_motionAsset = asset;
                PlayMotion();
            }
        }

        void EditorAnimMeshComponent::OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset)
        {
            OnAssetReady(asset);
        }

        void EditorAnimMeshComponent::OnActorInstanceCreated([[maybe_unused]] EMotionFX::ActorInstance* actorInstance)
        {
            m_actorInstance = actorInstance;
            PlayMotion();
        }

        void EditorAnimMeshComponent::OnActorInstanceDestroyed([[maybe_unused]] EMotionFX::ActorInstance* actorInstance)
        {
            RemoveMotionInstanceFromActor(m_lastMotionInstance);
            m_lastMotionInstance = nullptr;
            RemoveMotionInstanceFromActor(m_motionInstance);
            m_motionInstance = nullptr;

            m_actorInstance = nullptr;
        }

        void EditorAnimMeshComponent::PlayMotion()
        {
            if (m_previewInEditor)
            {
                m_motionInstance = AnimMeshComponent::PlayMotionInternal(m_actorInstance, m_configuration, /*deleteOnZeroWeight*/ false);
            }
        }

        void EditorAnimMeshComponent::RemoveMotionInstanceFromActor(EMotionFX::MotionInstance* motionInstance)
        {
            if (motionInstance)
            {
                if (m_actorInstance && m_actorInstance->GetMotionSystem())
                {
                    m_actorInstance->GetMotionSystem()->RemoveMotionInstance(motionInstance);
                }
            }
        }

        void EditorAnimMeshComponent::BuildGameEntity(AZ::Entity* gameEntity)
        {
            gameEntity->AddComponent(aznew AnimMeshComponent(&m_configuration));
        }

        void EditorAnimMeshComponent::LoopMotion(bool enable)
        {
            m_configuration.m_loop = enable;
            if (m_motionInstance)
            {
                m_motionInstance->SetMaxLoops(enable ? EMFX_LOOPFOREVER : 1);
            }
        }

        bool EditorAnimMeshComponent::GetLoopMotion() const
        {
            return m_configuration.m_loop;
        }

        void EditorAnimMeshComponent::RetargetMotion(bool enable)
        {
            m_configuration.m_retarget = enable;
            if (m_motionInstance)
            {
                m_motionInstance->SetRetargetingEnabled(enable);
            }
        }

        void EditorAnimMeshComponent::ReverseMotion(bool enable)
        {
            m_configuration.m_reverse = enable;
            if (m_motionInstance)
            {
                m_motionInstance->SetPlayMode(enable ? EMotionFX::EPlayMode::PLAYMODE_BACKWARD : EMotionFX::EPlayMode::PLAYMODE_FORWARD);
            }
        }
        
        void EditorAnimMeshComponent::MirrorMotion(bool enable)
        {
            m_configuration.m_mirror = enable;
            if (m_motionInstance)
            {
                m_motionInstance->SetMirrorMotion(enable);
            }
        }

        void EditorAnimMeshComponent::SetPlaySpeed(float speed)
        {
            m_configuration.m_playspeed = speed;
            if (m_motionInstance)
            {
                m_motionInstance->SetPlaySpeed(speed);
            }
        }

        float EditorAnimMeshComponent::GetPlaySpeed() const
        {
            return m_configuration.m_playspeed;
        }
        
        float EditorAnimMeshComponent::GetDuration() const
        {
            return m_motionInstance ? m_motionInstance->GetDuration() : 0.0f;
        }

        float EditorAnimMeshComponent::GetAssetDuration(const AZ::Data::AssetId& assetId)
        {
            float result = 1.0f;

            // Do a blocking load of the asset.
            AZ::Data::Asset<MotionAsset> motionAsset = AZ::Data::AssetManager::Instance().GetAsset<MotionAsset>(assetId, AZ::Data::AssetLoadBehavior::Default);
            motionAsset.BlockUntilLoadComplete();

            if (motionAsset && motionAsset.Get()->m_emfxMotion)
            {
                result = motionAsset.Get()->m_emfxMotion.get()->GetDuration();
            }

            motionAsset.Release();

            return result;
        }

        void EditorAnimMeshComponent::PlayTime(float time)
        {
            if (m_motionInstance)
            {
                float delta = time - m_motionInstance->GetLastCurrentTime();
                m_motionInstance->SetCurrentTime(time, false);

                // Apply the same time step to the last animation
                // so blend out will be good. Otherwise we are just blending
                // from the last frame played of the last animation.
                if (m_lastMotionInstance && m_lastMotionInstance->GetIsBlending())
                {
                    m_lastMotionInstance->SetCurrentTime(m_lastMotionInstance->GetLastCurrentTime() + delta, false);
                }
            }
        }

        float EditorAnimMeshComponent::GetPlayTime() const
        {
            float result = 0.0f;
            if (m_motionInstance)
            {
                result = m_motionInstance->GetCurrentTimeNormalized();
            }
            return result;
        }

        void EditorAnimMeshComponent::Motion(AZ::Data::AssetId assetId)
        {
            if (m_configuration.m_motionAsset.GetId() != assetId)
            {
                // Disconnect the old asset bus
                if (AZ::Data::AssetBus::MultiHandler::BusIsConnectedId(m_configuration.m_motionAsset.GetId()))
                {
                    AZ::Data::AssetBus::MultiHandler::BusDisconnect(m_configuration.m_motionAsset.GetId());
                }

                // Save the motion asset that we are about to be remove in case it can be reused.
                AZ::Data::Asset<MotionAsset> oldLastMotionAsset = m_lastMotionAsset;

                if (m_lastMotionInstance)
                {
                    RemoveMotionInstanceFromActor(m_lastMotionInstance);
                }

                // Store the current motion asset as the last one for possible blending.
                // If we don't keep a reference to the motion asset, the motion instance will be
                // automatically released.
                if (m_configuration.m_motionAsset.GetId().IsValid())
                {
                    m_lastMotionAsset = m_configuration.m_motionAsset;
                }

                // Set the current motion instance as the last motion instance. The new current motion
                // instance will then be set when the load is complete.
                m_lastMotionInstance = m_motionInstance;
                m_motionInstance = nullptr;

                // Start the fade out if there is a blend out time. Otherwise just leave the
                // m_lastMotionInstance where it is at so the next anim can blend from that frame.
                if (m_lastMotionInstance && m_configuration.m_blendOutTime > 0.0f)
                {
                    m_lastMotionInstance->Stop(m_configuration.m_blendOutTime);
                }

                // Reuse the old, last motion asset if possible. Otherwise, request a load.
                if (assetId.IsValid() && oldLastMotionAsset.GetData() && assetId == oldLastMotionAsset.GetId())
                {
                    // Even though we are not calling GetAsset here, OnAssetReady
                    // will be fired when the bus is connected because this asset is already loaded.
                    m_configuration.m_motionAsset = oldLastMotionAsset;
                }
                else
                {
                    // Won't be able to reuse oldLastMotionAsset, release it.
                    oldLastMotionAsset.Release();

                    // Clear the old asset.
                    m_configuration.m_motionAsset.Release();

                    // Create a new asset
                    if (assetId.IsValid())
                    {
                        m_configuration.m_motionAsset = AZ::Data::AssetManager::Instance().GetAsset<MotionAsset>(assetId, m_configuration.m_motionAsset.GetAutoLoadBehavior());
                    }
                }

                // Connect the bus if the asset is valid.
                if (m_configuration.m_motionAsset.GetId().IsValid())
                {
                    AZ::Data::AssetBus::MultiHandler::BusConnect(m_configuration.m_motionAsset.GetId());
                }
            }
        }

        AZ::Data::AssetId EditorAnimMeshComponent::GetMotion() const
        {
            return m_configuration.m_motionAsset.GetId();
        }

        void EditorAnimMeshComponent::SetPreviewInEditor(bool enable)
        {
            if (m_previewInEditor != enable)
            {
                m_previewInEditor = enable;
                OnEditorPropertyChanged();
            }
        }

        bool EditorAnimMeshComponent::GetPreviewInEditor() const
        {
            return m_previewInEditor;
        }

        void EditorAnimMeshComponent::BlendInTime(float time)
        {
            m_configuration.m_blendInTime = time;
        }

        float EditorAnimMeshComponent::GetBlendInTime() const
        {
            return m_configuration.m_blendInTime;
        }

        void EditorAnimMeshComponent::BlendOutTime(float time)
        {
            m_configuration.m_blendOutTime = time;
        }

        float EditorAnimMeshComponent::GetBlendOutTime() const
        {
            return m_configuration.m_blendOutTime;
        }

        AZ::Crc32 EditorAnimMeshComponent::OnEditorPropertyChanged()
        {
            RemoveMotionInstanceFromActor(m_lastMotionInstance);
            m_lastMotionInstance = nullptr;
            RemoveMotionInstanceFromActor(m_motionInstance);
            m_motionInstance = nullptr;
            m_configuration.m_motionAsset.Release();
            VerifyMotionAssetState();
            return AZ::Edit::PropertyRefreshLevels::None;
        }
    }
} // namespace EMotionFX

#if !defined(_RELEASE) && defined(AZ_PLATFORM_WINDOWS)
#pragma optimize("", on)
#endif
