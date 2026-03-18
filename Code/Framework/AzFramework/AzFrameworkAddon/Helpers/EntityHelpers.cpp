#include "EntityHelpers.h"

//#include <ISystem.h>

#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Debug/Trace.h>

#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/Entity/GameEntityContextBus.h>
#include <AzFramework/Entity/EntityContextBus.h>

// Debugging methods for Pure Virtual Call exception catching
// TODO: remove when releasing product
namespace {
#if defined(WIN64)
    static _purecall_handler defaultPureVirtualCallHandler = nullptr;
    void LocalPureVirtualCallHandler(void)
    {
        AZ_Assert(false, "EntityHelpers: Pure Virtual Call!");
        exit(0);
    }
    bool SetLocalPureVirtualCallHandler()
    {
        if (defaultPureVirtualCallHandler != nullptr)
            return true; // already set
        defaultPureVirtualCallHandler = _set_purecall_handler(LocalPureVirtualCallHandler);
        return defaultPureVirtualCallHandler != nullptr;
    }
#else
    // TODO: make platform-independent code:
    // This is to work for Linux and Android
    extern "C" void __cxa_pure_virtual()
    {  
        AZ_Assert(false, "EntityHelpers: Pure Virtual Call!");
        std::terminate();
    }
    // For macOS and iOS here should be smth like
    // #include "cxxabi.h"
    // #include "abort_message.h"
    // namespace __cxxabiv1 {
    //    extern "C" {
    //        _LIBCXXABI_FUNC_VIS _LIBCXXABI_NORETURN
    //        void __cxa_pure_virtual(void) {
    //            abort_message("Pure virtual function called!");
    //        }
    //        _LIBCXXABI_FUNC_VIS _LIBCXXABI_NORETURN
    //        void __cxa_deleted_virtual(void) {
    //            abort_message("Deleted virtual function called!");
    //        }
    //     }
    // }
#endif // WIN64
}

namespace AzFramework
{

    AZ::Entity* EntityHelpers::GetEntity(const AZ::EntityId& entityId)
    {
        if (!entityId.IsValid())
        {
            AZ_Error("EntityHelpers", false, "GetEntity('INVALID')!");
            return nullptr;
        }
        AZ::Entity* pEntity;
        AZ::ComponentApplicationBus::BroadcastResult(pEntity, &AZ::ComponentApplicationBus::Events::FindEntity, entityId);
        return pEntity;
    }

    AZ::Entity* EntityHelpers::FindEntity(const char* name)
    {
        AZ::Entity* pResult = nullptr;

        EBUS_EVENT(AZ::ComponentApplicationBus, EnumerateEntities,
            [name, &pResult](AZ::Entity* entity) mutable
            {
                if (strcmp(entity->GetName().c_str(), name) == 0)
                {
                    pResult = entity;
                }
            }
        );

        return pResult;
    }

    AZStd::vector<AZ::Entity*> EntityHelpers::FindEntitiesByComponent(const AZ::Uuid& typeId)
    {
        AZStd::vector<AZ::Entity*> result;

        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationBus::Events::EnumerateEntities,
            [&result, typeId](AZ::Entity* entity) mutable
            {
                if (entity->FindComponent(typeId))
                    result.push_back(entity);
            }
        );

        return result;
    }

    AZStd::vector<AZ::EntityId> EntityHelpers::FindEntityIdsByComponent(const AZ::Uuid& typeId)
    {
        AZStd::vector<AZ::Entity*> entities = FindEntitiesByComponent(typeId);
        AZStd::vector<AZ::EntityId> result;

        for (const auto& it : entities)
            result.push_back(it->GetId());

        return result;
    }

    AZStd::vector<AZ::EntityId> EntityHelpers::GetEntityAndAllDescendants(const AZ::EntityId& entityId, bool skipLogs/*=false*/) // 2nd argumnent renamed
    {
        AZStd::vector<AZ::EntityId> entitiesIdsList = {};
        // Entity Id should be valid and Entity should exist in the ComponentApplication's unordered map
        const auto& pEntity = GetEntity(entityId);
        if (!pEntity)
        {
            AZ_Error("EntityHelpers", false, "GetEntityAndAllDescendants(%s): Entity not found.", entityId.ToString().c_str());
            return entitiesIdsList;
        }

        // At least the Parent Entity is valid, so get all TransformComponent children requesting through TransformComponent Bus
        AZ::TransformBus::EventResult(entitiesIdsList, entityId, &AZ::TransformBus::Events::GetEntityAndAllDescendants);
        if (!entitiesIdsList.size())
        {
            // At least the Parent Entity should be in the list, if Entity has TransformComponent
            // Otherwise return at least the Parent Entity, having no TransformComponent
            entitiesIdsList.push_back(entityId);
        }

        if (!skipLogs)
        {
            AZ_Info("EntityHelpers", "GetEntityAndAllDescendants(%s,'%s') : %llu descendants",
                entityId.ToString().c_str(), pEntity->GetName().c_str(), static_cast<unsigned long long>(entitiesIdsList.size() - 1));
        }

        return entitiesIdsList;
    }

    bool EntityHelpers::IsEntityActive(const AZ::EntityId& entityId)
    {
        const auto entity = GetEntity(entityId);
        if (!entity)
            return false;
        return entity->GetState() == AZ::Entity::State::Active;
    }

    // Currently simply Deactivates Children and then Parent Entities
    void EntityHelpers::DisableEntity(const AZ::EntityId& entityId, bool skipLogs)
    {
        const auto pParentEntity = GetEntity(entityId);
        if (!pParentEntity)
        {
            AZ_Error("EntityHelpers", false, "DisableEntity(%s): No Entity found!", entityId.ToString().c_str());
            return;
        }
        if (!skipLogs)
        {
            AZ_Info("EntityHelpers", "DisableEntity(%s),'%s', and all children",
                entityId.ToString().c_str(), pParentEntity->GetName().c_str());
        }
        DeactivateEntity(entityId, skipLogs);
    }

    void EntityHelpers::ForeachInDescendants(const AZ::EntityId& id, AZStd::function<void(const AZ::EntityId&)> func, bool skipLogs)
    {
        const auto& descendantsIds = GetEntityAndAllDescendants(id, skipLogs);
        for (const auto& descendantId : descendantsIds)
        {
            if (descendantId.IsValid())
                func(descendantId);
        }
    }

    // Deactivation / disabling / deletion - reverse order with deeper Children first and Parent last is needeed.
    void EntityHelpers::ForeachInDescendantsInReverseOrder(const AZ::EntityId& id, AZStd::function<void(const AZ::EntityId&)> func, bool skipLogs)
    {
        const auto& descendantsIds = GetEntityAndAllDescendants(id, skipLogs);
        for (auto it = descendantsIds.rbegin(); it != descendantsIds.rend(); ++it)
        {
            if ((*it).IsValid())
                func((*it));
        }
    }

    bool EntityHelpers::ActivateEntity(AZ::Entity* entity, bool skipLogs)
    {
        AZ_Assert(entity, "(EntityHelpers) - ActivateEntity(nullptr).");
        if (!entity)
        {
            return false;
        }
        
        // Some entities do not belong to GameEntityContext: e.g. UI entities, - 
        // so we do not use the GameEntityContextRequestBus method which checks for GameEntityContext:
        // AzFramework::GameEntityContextRequestBus::Broadcast(&AzFramework::GameEntityContextRequestBus::Events::ActivateGameEntity, id);
        
        // Safety Check: Is the entity initialized?
        if (entity->GetState() == AZ::Entity::State::Constructed)
        {
            entity->Init();
        }
        const auto& state = entity->GetState();
        switch (state)
        {
        case AZ::Entity::State::Init:
            // Activate immediately
            entity->Activate();
            if (!skipLogs)
            {
                AZ_Info("EntityHelpers", "ActivateEntity(Entity* localEntity): Entity %s,'%s' Activated",
                    entity->GetId().ToString().c_str(), entity->GetName().c_str());
            }
            return true;

        case AZ::Entity::State::Initializing:
        case AZ::Entity::State::Deactivating:
            // Queue activate to trigger next frame
            AZ::TickBus::QueueFunction(&AZ::Entity::Activate, entity);
            if (!skipLogs)
            {
                AZ_Info("EntityHelpers", "ActivateEntity(Entity* localEntity): Entity %s,'%s' Queued for Activation",
                    entity->GetId().ToString().c_str(), entity->GetName().c_str());
            }
            return true;

        case AZ::Entity::State::Active:
        case AZ::Entity::State::Activating:
            // Do nothing: already active or activating
            return true;

        default: // Destroyed or in the process of Destroying.
            break;
        }
        return false;
    }


    void EntityHelpers::ActivateEntity(const AZ::EntityId& entityId, bool skipLogs)
    {
        const auto& entity = GetEntity(entityId);
        AZ_Error("EntityHelpers", entity, "ActivateEntity(%s) Entity not found!", entityId.ToString().c_str());
        if (!entity)
        {
            return;
        }

        // Some entities do not belong to GameEntityContext: e.g. UI entities, - 
        // so we do not use the GameEntityContextRequestBus method which checks for GameEntityContext:
        // AzFramework::GameEntityContextRequestBus::Broadcast(&AzFramework::GameEntityContextRequestBus::Events::ActivateGameEntity, id);

        const auto& entitiesIds = GetEntityAndAllDescendants(entityId, true);
        for (const auto& id : entitiesIds)
        {
            if (const auto& localEntity = GetEntity(id))
            {
                if (!ActivateEntity(localEntity, skipLogs))
                {
                    if (id == entityId) // Log Error when attempting to Activate the Destroyed Parent Entity, skip Error Log for Children Entities
                    {
                        const auto& state = localEntity->GetState();
                        const char* str = state == AZ::Entity::State::Destroying ? "Destroying"
                            : state == AZ::Entity::State::Destroyed ? "Destroyed"
                            : "(unexpected)";
                            AZ_Error("EntityHelpers", false, "ActivateEntity(%s,'%s'): FAILED, Entity is in State = %s.",
                                id.ToString().c_str(), localEntity->GetName().c_str(), str);
                    }
                }
            }
        }
    }

    bool EntityHelpers::DeactivateEntity(AZ::Entity* entity, bool skipLogs)
    {
        AZ_Assert(entity, "(EntityHelpers) - DeactivateEntity(nullptr).");
        if (!entity)
        {
            return false;
        }
        const auto& state = entity->GetState();
        switch (state)
        {
        case AZ::Entity::State::Activating:
            // Queue deactivate to trigger next frame
            AZ::TickBus::QueueFunction(&AZ::Entity::Deactivate, entity);
            if (!skipLogs)
            {
                AZ_Info("EntityHelpers", "DeactivateEntity(%s,'%s'): Entity Queued for Deactivation",
                    entity->GetId().ToString().c_str(), entity->GetName().c_str());
            }
            return false;

        case AZ::Entity::State::Active:
            // Deactivate immediately
            entity->Deactivate();
            if (!skipLogs)
            {
                AZ_Info("EntityHelpers", "DeactivateEntity(%s,'%s'): Entity Deactivated",
                    entity->GetId().ToString().c_str(), entity->GetName().c_str());
            }
            break;

        default:
            // Don't do anything, it's not even active.
            break;
        }
        return true;
    }

    void EntityHelpers::DeactivateEntity(const AZ::EntityId& entityId, bool skipLogs)
    {
        if (!entityId.IsValid())
        {
            return;
        }
        const auto& entity = GetEntity(entityId);
        AZ_Error("EntityHelpers", entity, "DeactivateEntity(%s) Entity not found!", entityId.ToString().c_str());
        if (!entity)
        {
            return;
        }

        // Some entities do not belong to GameEntityContext: e.g. UI entities, - 
        // so we do not use the GameEntityContextRequestBus method which checks for GameEntityContext:
        // AzFramework::GameEntityContextRequestBus::Broadcast(&AzFramework::GameEntityContextRequestBus::Events::DectivateGameEntity, id);

        const auto& entitiesIds = GetEntityAndAllDescendants(entityId, true); // Gruber patch // STA : Skip inner Log, const auto&
        if (!skipLogs)
        {
            AZ_Info("EntityHelpers", "DeactivateEntity(%s,'%s'): deactivating %llu Children and then Parent Entity",
                entityId.ToString().c_str(), entity->GetName().c_str(), static_cast<unsigned long long>(entitiesIds.size() - 1));
        }

        // Deactivate Children in reverse order, Parent being the last one, because Deactivate() may destroy Children
        for (auto it = entitiesIds.rbegin(); it != entitiesIds.rend(); ++it)
        {
            if (const auto& localEntity = GetEntity(*it))
            {
                DeactivateEntity(localEntity, skipLogs);
            }
        }
    }

    // Destroys the single Entity instance
    bool EntityHelpers::DestroyEntityInstance(const AZ::EntityId& id, bool skipLogs)
    {
#ifdef WIN64
        // TODO: remove when releasing product
        SetLocalPureVirtualCallHandler();
#endif // WIN64

        const auto& entity = GetEntity(id);
        AZ_Error("EntityHelpers", entity, "DestroyEntityInstance(%s) Entity not found!", id.ToString().c_str());
        if (!entity)
        {
            return false;
        }
        const auto name = entity->GetName(); // here we need the copy, as objects are to be destroyed
        // Explicit check: Entity is to be inactive
        if (!DeactivateEntity(entity))
        {
            AZ_Error("EntityHelpers", false, "DestroyEntityInstance(%s,'%s') Entity not deactivated yet, try next frame!",
                id.ToString().c_str(), name.c_str());
            return false;
        }
        entity->SetRuntimeActiveByDefault(false); // Don't allow to activate the entity

        bool result = false;
        // Try to delete it in the GameContext
        auto gameContextId = AzFramework::EntityContextId::CreateNull();
        AzFramework::GameEntityContextRequestBus::BroadcastResult(gameContextId, &AzFramework::GameEntityContextRequestBus::Events::GetGameEntityContextId);
        if (!gameContextId.IsNull())
        {
            AzFramework::EntityContextRequestBus::EventResult(result, gameContextId, &AzFramework::EntityContextRequestBus::Events::DestroyEntityById, id);
            if (result)
            {
                if (!skipLogs)
                {
                    AZ_Info("EntityHelpers", "DestroyEntityInstance(%s,'%s'): successfully destroyed in GameContext",
                        id.ToString().c_str(), name.c_str());
                }
                return result;
            }
        }
        // Finally, try to simply delete it
        AZ::ComponentApplicationBus::BroadcastResult(result, &AZ::ComponentApplicationBus::Events::DeleteEntity, id);
        AZ_Error("EntityHelpers", result, "DestroyEntityInstance(%s,'%s'): FAILED to delete!", id.ToString().c_str(), name.c_str());
        if (result && !skipLogs)
        {
            AZ_Info("EntityHelpers", "DestroyEntityInstance(%s,'%s'): successfully deleted",
                id.ToString().c_str(), name.c_str());
        }
        return result;
    }

    // Destroys the claimed Entity instance of a spawnable and its descendants.
    bool EntityHelpers::DestroyEntityInstanceAndDescendants(const AZ::EntityId& id, bool skipLogs)
    {
        const auto& entity = GetEntity(id);
        AZ_Error("EntityHelpers", entity, "DestroyEntityInstanceAndDescendants(%s) Entity not found!", id.ToString().c_str());
        if (!entity)
        {
            return false;
        }

        const auto& entities = GetEntityAndAllDescendants(id, true); // Skip inner Log
        if (!skipLogs)
        {
            AZ_Info("EntityHelpers", "DestroyEntityInstanceAndDescendants(%s,'%s'): destroying %llu Children and then Parent Entity",
                id.ToString().c_str(), entity->GetName().c_str(), static_cast<unsigned long long>(entities.size() - 1));
        }

        // Destroy Children in reverse order, skipping Parent for now
        for (auto it = entities.rbegin(); it != entities.rend(); ++it)
        {
            if ((*it) == id) // ensure that final result got from destroying the Parent
                continue;
            DestroyEntityInstance((*it), skipLogs);
        }
        return DestroyEntityInstance(id, skipLogs);
    }

// Gruber patch begin // VMED
    bool EntityHelpers::DetachEntityFromParent(const AZ::EntityId& entityId)
    {
        if (!entityId.IsValid())
        {
            return false;
        }
        const auto& entity = GetEntity(entityId);
        AZ_Error("EntityHelpers", entity, "DetachEntityFromParent(%s): Entity not found!", entityId.ToString().c_str());

        if (!entity)
        {
            return false;
        }

        AZ::TransformInterface* transform = entity->GetTransform();
        if (transform)
        {
            AZ::EntityId parentId = transform->GetParentId();
            if (parentId.IsValid())
            {
                transform->SetParent(AZ::EntityId()); // it also removes this entity from parent childrens
                return true;
            }
        }

        return false;
    }
// Gruber patch end // VMED

} //namespace AzFramework
