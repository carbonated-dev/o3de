/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#if defined(CARBONATED) && defined(CARBONATED_EMOTIONFX_PREFAB_SYSTEM)

// include the required headers
#include <AzCore/std/containers/vector.h>
#include "EMotionFXConfig.h"
#include <MCore/Source/RefCounted.h>
#include "MemoryCategories.h"
#include <MCore/Source/MultiThreadManager.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/weak_ptr.h>
#include <Source/Integration/Assets/ActorAsset.h>
#include <Source/Integration/System/SystemCommon.h>
#include <AzFramework/Spawnable/SpawnableEntitiesInterface.h>
#include <AzFramework/Spawnable/Spawnable.h>
#include <Integration/Components/ActorComponent.h>
#include <MCore/Source/IDGenerator.h>

namespace EMotionFX
{

    using PrefabAssetData = AZ::Data::Asset<AzFramework::Spawnable>;

    struct PrefabData
    {
        AzFramework::EntitySpawnTicket m_spawnTicket;
        PrefabAssetData                m_prefabAsset;
        bool                           m_isSpawned;
        uint32                         m_id;
        AZ::Data::AssetId              m_actorAssetId;

        PrefabData(const PrefabAssetData& prefabAsset)
            : m_spawnTicket(prefabAsset)
            , m_prefabAsset(prefabAsset)
            , m_isSpawned(false)
        {
            m_id = aznumeric_caster(MCore::GetIDGenerator().GenerateID());
        }
    };

    /**
     * The prefab manager.
     * This class maintains a list of registered prefabs and prefab instances that have been created.
     * Also it stores a list of root prefab instances, which are roots in the chains of attachments.
     * For example if you attach a cowboy to a horse, the horse is the root prefab instance.
     */
    class EMFX_API PrefabManager
        : public MCore::RefCounted
    {
        AZ_CLASS_ALLOCATOR_DECL
        friend class Initializer;
        friend class EMotionFXManager;

    public:
        static PrefabManager* Create();

        /**
         * Register a prefab.
         * @param The prefab to register.
         */
        void RegisterPrefab(PrefabData const& prefabData);
        void RegisterWeaponPrefab(PrefabData const& prefabData);

        /**
         * Unregister all prefabs.
         * This does not release/delete the actual prefab objects, but just clears the internal array of prefab instances.
         * This method is automatically called at shutdown of your application.
         */
        void UnregisterAllPrefabs();

        /**
         * Unregister a specific prefab.
         * @param prefab The prefab you passed to the RegisterPrefab function sometime before.
         */
        void UnregisterPrefab(AZ::Data::AssetId const& prefabAssetID);
        void UnregisterWeaponPrefab();

        /**
         * Get the number of registered prefabs.
         * This does not include the clones that have been optionally created.
         * @result The number of registered prefabs.
         */
        MCORE_INLINE int GetNumPrefabs() const { return static_cast<int>(m_prefabDatas.size()); }

        bool HasWeaponPrefab() const { return m_weaponPrefabData.m_prefabAsset.IsReady(); }
        /**
         * Get a given prefab.
         * This will return a Prefab object that contains an array of Prefab objects.
         * The first Prefab in this array will be the prefab you passed to RegisterPrefab.
         * The following Prefab objects in the array will be the created clones, if there are any.
         * @param nr The prefab number, which must be in range of [0..GetNumPrefabs()-1].
         * @result A reference to the prefab object that contains the array of Prefab objects.
         */
        PrefabData& GetPrefabData(size_t nr);
        PrefabData const& GetPrefabData(size_t nr) const;

        PrefabData& GetWeaponPrefabData() { return m_weaponPrefabData; }
        PrefabData const& GetWeaponPrefabData() const { return m_weaponPrefabData; }

        /**
         * Find the given prefab by assetId.
         */
        AZ::Data::AssetId FindAssetIdByPrefabId(uint32 prefabId) const;

        const PrefabData* FindPrefabByID(uint32 prefabId) const;

        /**
         * Find the prefab number for a given Prefab object.
         * This will find the prefab number for the Prefab object that you passed to RegisterPrefab before.
         * @param prefab The prefab object you once passed to RegisterPrefab.
         * @result Returns the prefab number, which is in range of [0..GetNumPrefabs()-1], or returns MCORE_INVALIDINDEX32 when not found.
         */
        size_t FindPrefabIndex(AZ::Data::AssetId assetId) const;
        size_t FindPrefabIndex(AzFramework::Spawnable* prefab) const;

        void DestroyAllPrefabs();

        void LockPrefabs();
        void UnlockPrefabs();

        AZ::Component* GetComponentByUUID(size_t nr, const AZ::Uuid& type) const;
        AZ::Component* GetMaterialComponent(size_t nr) const;
        AZ::Component* GetAttachmentsComponent(size_t nr) const;
        EMotionFX::Integration::ActorComponent* GetActorComponent(size_t nr) const;

    private:
        AZStd::vector<PrefabData> m_prefabDatas;
        PrefabData                m_weaponPrefabData;
        MCore::MutexRecursive     m_prefabLock; /**< The multithread lock for touching the prefabs array. */

        /**
         * The constructor, which initializes using the multi processor scheduler.
         */
        PrefabManager();

        /**
         * The destructor. Automatically deletes the callback and scheduler.
         */
        ~PrefabManager() override;
    };
}   // namespace EMotionFX

#endif // CARBONATED_EMOTIONFX_PREFAB_SYSTEM
