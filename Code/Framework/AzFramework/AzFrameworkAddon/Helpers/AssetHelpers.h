/*
 * Copyright(c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX - License - Identifier: Apache - 2.0 OR MIT
 *
 */
#pragma once

#include <AzFramework/AzFrameworkAPI.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Slice/SliceAsset.h>
#include <AzCore/std/string/string.h>

namespace AZ
{
    class EntityId;
}

namespace AzFramework
{
    class Spawnable;

    /// <summary>
    /// Helpers evaluating spawnables and dynamic slices : Find / Load, Destroy Instantance.
    /// </summary>
    // Refactored as compared to the original legacy methods in System/Utils/Utils:
    //  - Detailed logs added.
    //  - Original invalid relative paths to assets fixed, extensions changed as needed.
    //  - Separate helper methods to Destroy instances of:
    //    * UI Slices;
    //    * Legacy Dynamic Slices: assumed to never be used;
    //    * Spawnables: assumed to never be used. 
    class AZF_API AssetHelpers
    {
    public:

        /// Currently only removes leading slashes in the given relative path, if any, shifting the C-string address
        static const char* CleanupRelativePath(const char* path);

        /// Replaces '.dynamicslice' extension with '.spawnable' extension in the given path.
        static void ReplaceExtensionToSpawnable(AZStd::string& path);

        /// Returns Dynamic Slice Asset Data, if a ".dynamicslice" is found with given path.
        /// Works for both:
        /// - UI Slices, which are used (see
        ///   SliceInstantiator::InstantiateUISlice() and SliceInstantiator::InstantiateUIAndNotify() ), and
        /// - Legacy Dynamic Slices, which are not supposed to be used (see
        ///   SliceInstantiator::Instantiatiate(), SliceInstantiator::InstantiateAsset() and
        ///   SliceInstantiator::InstantiateAndNotify() )
        static bool GetDynamicSliceAsset(const char* path, AZ::Data::Asset<AZ::DynamicSliceAsset>& asset);

        /// Destroys the Legacy Dynamic Slice Asset Instance.
        /// Use only for Legacy Dynamic Slices instantiated with:
        /// - SliceInstantiator::Instantiatiate(), or
        /// - SliceInstantiator::InstantiateAsset(), or
        /// - SliceInstantiator::InstantiateAndNotify().
        static bool DestroyDynamicSliceInstance(const AZ::EntityId& id);

        /// Destroys the  Legacy Dynamic Slice Asset Instance.
        /// Use for UI Dynamic Slices instantiated with: 
        /// - SliceInstantiator::InstantiateUISlice(), or
        /// - SliceInstantiator::InstantiateUIAndNotify().
        static bool DestroyUISliceInstance(const AZ::EntityId& id);

        /// Returns Spawnable Asset Data, if a '.spawnable' is found with given path.
        /// Path is reevaluated inside:
        /// - leading slashes are removed to make valid O3DE relative path,
        /// - extension is replaced by the '.spawnable'.
        static bool GetSpawnableAsset(const char* path, AZ::Data::Asset<AzFramework::Spawnable>& asset);

        /// Destroys the Entity instance of a spawnable and despawns it.
        /// Use with caution when really needed, or consider using instead:
        /// - EntityHelpers::DestroyEntityInstance() for deleting a single Entity instance, or
        /// - EntityHelpers::DestroyEntityInstanceAndDescendants() for deleting a hierarchy of claimed instances of spawnables.
        /// 
        /// Requests
        ///  EBUS_EVENT(GameEntityContextRequestBus, DestroyGameEntity, id);
        /// or, without Macro, 
        ///  AzFramework::GameEntityContextRequestBus::Broadcast(&AzFramework::GameEntityContextRequestBus::Events::DestroyGameEntity, id);
        /// and thus also despawns original Entity, but does not clear the spawned ticket, and thus interferes with
        /// SpawnableEntitiesManager logic, raising asserts at the game exit.
        static bool DestroySpawnableEntity(const AZ::EntityId& id);
    };
} // namespace AzFramework

