/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#if defined(CARBONATED_EMOTIONFX_PREFAB_SYSTEM)

// include the required headers
#include "EMotionFXConfig.h"
#include "PrefabManager.h"
#include "ActorManager.h"
#include "ActorInstance.h"
#include "MultiThreadScheduler.h"
#include <MCore/Source/LogManager.h>
#include <MCore/Source/StringConversions.h>
#include <EMotionFX/Source/Allocators.h>
#include <EMotionFX/Source/Actor.h>
#include <EMotionFX/Source/EMotionFXManager.h>
#include <AzFramework/Spawnable/Spawnable.h>
#include <AtomLyIntegration/CommonFeatures/Material/MaterialComponentConstants.h>
#include <Integration/Components/ActorComponent.h>

namespace EMotionFX
{
    AZ_CLASS_ALLOCATOR_IMPL(PrefabManager, GeneralAllocator)

    // constructor
    PrefabManager::PrefabManager()
        : MCore::RefCounted()
    {
    }

    // destructor
    PrefabManager::~PrefabManager()
    {
    }

    // create
    PrefabManager* PrefabManager::Create()
    {
        return aznew PrefabManager();
    }

    void PrefabManager::DestroyAllPrefabs()
    {
        UnregisterAllPrefabs();
    }

    // register the prefab
    void PrefabManager::RegisterPrefab(PrefabData const& prefabData)
    {
        LockPrefabs();

        // check if we already registered
        if (FindPrefabIndex(prefabData.m_prefabAsset.GetId()) != InvalidIndex)
        {
            MCore::LogWarning("EMotionFX::PrefabManager::RegisterPrefab() - The prefab %s has already been registered as prefab", prefabData.m_prefabAsset.GetHint().c_str());
            UnlockPrefabs();
            return;
        }

        // register it
        m_prefabDatas.emplace_back(AZStd::move(prefabData));

        UnlockPrefabs();
    }

    EMotionFX::Integration::ActorComponent* PrefabManager::GetActorComponent(size_t nr) const
    {
        PrefabAssetData asset = m_prefabDatas[nr].m_prefabAsset;
        AzFramework::Spawnable* prefab = asset.Get();
        auto& entities = prefab->GetEntities();
        for (auto& e : entities)
        {
            AZ::Component* actor = e->FindComponent(azrtti_typeid<EMotionFX::Integration::ActorComponent>());
            if (actor)
            {
                return e->FindComponent<EMotionFX::Integration::ActorComponent>();
            }
        }
        return nullptr;
    }

    AZ::Component* PrefabManager::GetAttachmentsComponent(size_t nr) const
    {
        return GetComponentByUUID(nr, AZ::TypeId{ "{2D17A64A-7AC5-4C02-AC36-C5E8141FFDDF}" });
    }

    AZ::Component* PrefabManager::GetComponentByUUID(size_t nr, const AZ::Uuid& type) const
    {
        PrefabAssetData asset = m_prefabDatas[nr].m_prefabAsset;
        AzFramework::Spawnable* prefab = asset.Get();
        auto& entities = prefab->GetEntities();
        for (auto& e : entities)
        {
            AZ::Component* act = e->FindComponent(azrtti_typeid<EMotionFX::Integration::ActorComponent>());
            if (act)
            {
                AZ::Component* mat = e->FindComponent(type);
                if (mat)
                {
                    return mat;
                }
            }
        }
        return nullptr;
    }

    AZ::Component* PrefabManager::GetMaterialComponent(size_t nr) const
    {
        return GetComponentByUUID(nr, AZ::Render::MaterialComponentTypeId);
    }

    const PrefabData* PrefabManager::FindPrefabByID(uint32 prefabId) const
    {
        const auto found = AZStd::find_if(m_prefabDatas.begin(), m_prefabDatas.end(),
            [prefabId](const PrefabData& a)
            {
                return a.m_id == prefabId;
            });
        if (found != m_prefabDatas.end())
        {
            return &(*found);
        }
        return nullptr;
    }

    AZ::Data::AssetId PrefabManager::FindAssetIdByPrefabId(uint32 prefabId) const
    {
        const auto found = AZStd::find_if(m_prefabDatas.begin(), m_prefabDatas.end(),
            [prefabId](const PrefabData& a)
            {
                return a.m_id == prefabId;
            });
        if (found != m_prefabDatas.end())
        {
            return found->m_prefabAsset.GetId();
        }
        return AZ::Data::AssetId();
    }

    size_t PrefabManager::FindPrefabIndex(AZ::Data::AssetId assetId) const
    {
        const auto found = AZStd::find_if(m_prefabDatas.begin(), m_prefabDatas.end(),
            [assetId](const PrefabData& a)
            {
                return a.m_prefabAsset.GetId() == assetId;
            });

        return (found != m_prefabDatas.end()) ? AZStd::distance(m_prefabDatas.begin(), found) : InvalidIndex;
    }

    size_t PrefabManager::FindPrefabIndex(AzFramework::Spawnable* prefab) const
    {
        const auto found = AZStd::find_if(m_prefabDatas.begin(), m_prefabDatas.end(),
            [prefab](const PrefabData& a)
            {
                return a.m_prefabAsset.Get() == prefab;
            });

        return (found != m_prefabDatas.end()) ? AZStd::distance(m_prefabDatas.begin(), found) : InvalidIndex;
    }

    void PrefabManager::UnregisterPrefab(AZ::Data::AssetId assetID)
    {
        LockPrefabs();

        auto found = AZStd::find_if(m_prefabDatas.begin(), m_prefabDatas.end(),
            [assetID](const PrefabData& a)
            {
                return a.m_prefabAsset.GetId() == assetID;
            });

        if (found != m_prefabDatas.end())
        {
            GetActorManager().UnregisterActor(found->m_actorAssetId);
            m_prefabDatas.erase(found);
        }

        UnlockPrefabs();
    }

    void PrefabManager::UnregisterAllPrefabs()
    {
        LockPrefabs();

        for (PrefabData& prefabData : m_prefabDatas)
        {
            GetActorManager().UnregisterActor(prefabData.m_actorAssetId);
            if (prefabData.m_isSpawned)
            {
                if (prefabData.m_spawnTicket.IsValid())
                {
                    AzFramework::SpawnableEntitiesInterface::Get()->DespawnAllEntitiesImmediately(prefabData.m_spawnTicket);
                }
            }
        }

        m_prefabDatas.clear();

        UnlockPrefabs();
    }

    void PrefabManager::LockPrefabs()
    {
        m_prefabLock.Lock();
    }


    void PrefabManager::UnlockPrefabs()
    {
        m_prefabLock.Unlock();
    }


    PrefabData& PrefabManager::GetPrefabData(size_t nr)
    {
        return m_prefabDatas[nr];
    }

    PrefabData const& PrefabManager::GetPrefabData(size_t nr) const
    {
        return m_prefabDatas[nr];
    }
}   // namespace EMotionFX

#endif // CARBONATED_EMOTIONFX_PREFAB_SYSTEM
