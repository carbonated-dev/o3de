#include "AssetHelpers.h"

//#include <ISystem.h>

#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Slice/SliceAsset.h>
#include <AzFramework/Entity/EntityContextBus.h>
#include <AzFramework/Entity/GameEntityContextBus.h>
#include <AzFramework/Entity/SliceGameEntityOwnershipServiceBus.h> // for legacy Dynamic Slices
#include <AzFramework/Slice/SliceInstantiationTicket.h>
#include <AzFramework/Spawnable/Spawnable.h>
#include <AzFramework/StringFunc/StringFunc.h>

#include "EntityHelpers.h"

#if defined(CARBONATED)
#include <AzCore/Memory/MemoryMarker.h>
#endif

namespace AzFramework
{
    const char* AssetHelpers::CleanupRelativePath(const char* path)
    {
        if (!path)
        {
            AZ_Assert(path, "(AssetHelpers) - CleanupRelativePath(nullptr)!");
        }
        else {
            while (*path == '/' || *path == '\\') // remove leading slashes in relative paths
            {
                ++path;
            }
            AZ_Warning("AssetHelpers", *path, "CleanupRelativePath(path) Evaluates to empty path!");
        }
        return path;
    }

    void AssetHelpers::ReplaceExtensionToSpawnable(AZStd::string& path)
    {
        const char* spawnableExt = "spawnable";
        if (!AzFramework::StringFunc::Path::IsExtension(path.c_str(), spawnableExt))
        {
            AzFramework::StringFunc::Path::ReplaceExtension(path, spawnableExt);
        }
    }

    bool AssetHelpers::GetDynamicSliceAsset(const char* path, AZ::Data::Asset<AZ::DynamicSliceAsset>& asset)
    {
        AZ::Data::AssetId            assetId;
        AzFramework::SliceInstantiationTicket ticket;

        const auto relativePath = CleanupRelativePath(path);    // remove leading slashes in relative paths

        AZ_Assert(AzFramework::StringFunc::Path::IsExtension(path, ".dynamicslice"),  "(AssetHelpers) - GetDynamicSliceAsset(%s) Is not a dynamic slice", relativePath);

        AZ::Data::AssetCatalogRequestBus::BroadcastResult(assetId,
            &AZ::Data::AssetCatalogRequestBus::Events::GetAssetIdByPath,
            relativePath,
            AZ::AzTypeInfo<AZ::DynamicSliceAsset>::Uuid(),
            false);

        AZ_Assert(assetId.IsValid(),
            "(AssetHelpers) - GetDynamicSliceAsset(%s) Tried to get the dynamic slice asset, but it is not valid or missing", relativePath);  // Gruber patch // STA
        if (!assetId.IsValid())
            return false;

        asset.Create(assetId, true);
        return true;
    }

    bool AssetHelpers::DestroyDynamicSliceInstance(const AZ::EntityId& id)
    {
        bool result = false;

        // Maybe DestroyUISliceInstance() or DestroyEntityInstanceAndDescendants() was meant to be called ?
        AZ_Assert(false, "(AssetHelpers) - DestroyDynamicSliceInstance(%s) called, supposed not to be used!", id.ToString().c_str());

        // Additional checks and Entity name for extended Logs
        const auto& entity = EntityHelpers::GetEntity(id);
        AZ_Error("AssetHelpers", entity, "DestroyDynamicSliceInstance(%s) Entity not found!", id.ToString().c_str());
        if (!entity)
        {
            return false;
        }
        const auto& name = entity->GetName();

        // Then delete the Entity as a Legacy Dynamic Slice Instance
        AzFramework::SliceGameEntityOwnershipServiceRequestBus::BroadcastResult(result,
            &AzFramework::SliceGameEntityOwnershipServiceRequests::DestroyDynamicSliceByEntity, id);
        AZ_Error("AssetHelpers", result, "DestroyDynamicSliceInstance(%s,'%s') FAILED to destroy!", id.ToString().c_str(), name.c_str());
        if (result)
        {
            AZ_Info("AssetHelpers", "DestroyDynamicSliceInstance(%s,'%s'): successfully destroyed", id.ToString().c_str(), name.c_str());
        }
        return result;
    }

    bool AssetHelpers::GetSpawnableAsset(const char* path, AZ::Data::Asset<AzFramework::Spawnable>& asset)
    {
        AZ::Data::AssetId assetId;

        const auto relativePath = CleanupRelativePath(path);

        AZStd::string fixedRelativePath(relativePath);
        ReplaceExtensionToSpawnable(fixedRelativePath);

        AZ::Data::AssetCatalogRequestBus::BroadcastResult(assetId,
            &AZ::Data::AssetCatalogRequestBus::Events::GetAssetIdByPath,
            fixedRelativePath.c_str(),
            AZ::AzTypeInfo<AzFramework::Spawnable >::Uuid(),
            false);

        if (!assetId.IsValid())
            return false;

#if defined(CARBONATED)
        ASSET_TAG(asset.GetHint().c_str());
#endif
        asset.Create(assetId, true);
        return true;
    }

    // Destroys the Entity instance of a spawnable and despawns it.
    /// Use with caution when really needed, or consider using instead:
    // - EntityHelpers::DestroyEntityInstance() for deleting a single Entity instance, or
    // - EntityHelpers::DestroyEntityInstanceAndDescendants() for deleting a hierarchy of claimed instances of spawnables.
    bool AssetHelpers::DestroySpawnableEntity(const AZ::EntityId& id)
    {
        // Add checks and logs, and Add Assert for legacy method
        const auto& entity = EntityHelpers::GetEntity(id);
        AZ_Error("AssetHelpers", entity, "DestroySpawnableEntity(%s) Entity not found!", id.ToString().c_str());
        if (!entity)
        {
            return false;
        }

        // Maybe DestroyUISliceInstance() or DestroyEntityInstanceAndDescendants() was meant to be called ?
        AZ_Assert(false, "(AssetHelpers) - DestroySpawnableEntity(%s,'%s') called to delete and despawn the Entity with TicketId: %2lu, supposed not to be used in Gruber!",
            id.ToString().c_str(), entity->GetName().c_str(), entity->GetEntitySpawnTicketId());

        AzFramework::GameEntityContextRequestBus::Broadcast(&AzFramework::GameEntityContextRequestBus::Events::DestroyGameEntity, id);
        return true;
    }

} //namespace AzFramework
