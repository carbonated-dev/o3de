/*
 * Copyright(c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX - License - Identifier: Apache - 2.0 OR MIT
 *
 */
#pragma once

#include <AzFramework/AzFrameworkAPI.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/function/function_template.h>

namespace AZ
{
    class Entity;
    class EntityId;
}

namespace AzFramework
{
    /// <summary>
    /// Helpers evaluating Entities: Find, Get, Activate, Deactivate, Disable, Destroy.
    /// </summary>
    // Refactored as compared to the original legacy methods in System/Utils/Utils:
    //  - Detailed logs added.
    //  - GetEntityAndAllDescendants() optimized: Children got using TransformBus request instead of enumerating all Entities.
    //  - Reverse order implemented for  Deactivate, Disable, Destroy.
    class AZF_API EntityHelpers
    {
    public:

        /// Returns pointer to an Entity found by entityId, or nullptr otherwise (with logs).
        static AZ::Entity* GetEntity(const AZ::EntityId& entityId);

        /// Returns pointer to an Entity found by exact name, or nullptr otherwise.
        static AZ::Entity* FindEntity(const char* name);

        /// Returns vector of pointers to Entities containing a Component with the given typeId.
        static AZStd::vector<AZ::Entity*> FindEntitiesByComponent(const AZ::Uuid& typeId);

        /// Returns vector of pointers to Entities containing a Component of the given type.
        template<typename COMPONENT>
        static AZStd::vector<AZ::Entity*> FindEntitiesByComponent()
        {
            return FindEntitiesByComponent(COMPONENT::TYPEINFO_Uuid());
        }

        /// Returns vector of EntityIds of Entities containing a Component with given typeId.
        static AZStd::vector<AZ::EntityId> FindEntityIdsByComponent(const AZ::Uuid& typeId);

        /// Returns vector of EntityIds of Entities containing a Component of the given type.
        template<typename COMPONENT>
        static AZStd::vector<AZ::EntityId> FindEntityIdsByComponent()
        {
            return FindEntityIdsByComponent(COMPONENT::TYPEINFO_Uuid());
        }

        /// Returns pointer to a Component if an Entity with the given entityId is found and
        /// contains the Component of the given type, or nullptr otherwise.
        template<typename COMPONENT>
        static COMPONENT* GetComponent(const AZ::EntityId& entityId)
        {
            AZ::Entity* pEntity = GetEntity(entityId);
            if (pEntity == nullptr)
                return nullptr;

            return pEntity->FindComponent<COMPONENT>();
        }

        /// Returns vector with EntityIds of Parent and then deeper Children, if these are got by TransformHierarchyInformationBus request.
        static AZStd::vector<AZ::EntityId> GetEntityAndAllDescendants(const AZ::EntityId& id, bool skipLogs = false); // 2nd argument renamed

        /// Returns true if an Entity with the given entityId is found and is Active, or false otherwise.
        static bool IsEntityActive(const AZ::EntityId& entityId);

        /// Currently simply Deactivates Children Entities, if these are got by TransformHierarchyInformationBus request,
        /// in reverse order, and finally Deactivates the Parent Entity.
        /// Suitable both for GameContext and UI Entities.
        static void DisableEntity(const AZ::EntityId& entityId, bool skipLogs = false);

        /// Activates single Entity directly, without using the GameEntityContextRequestBus,
        /// and thus suitable both for GameContext and UI Entities.
        /// Returns true if the Entity is not Destroyed or in the process of Destroying.
        static bool ActivateEntity(AZ::Entity* entity, bool skipLogs = false);

        /// Activates the Parent Entity, and the Children Entities, if these are got by TransformHierarchyInformationBus request
        /// Suitable both for GameContext and UI Entities.
        static void ActivateEntity(const AZ::EntityId& entityId, bool skipLogs = false);

        /// Deactivates single Entity directly, without using the GameEntityContextRequestBus,
        /// and thus suitable both for GameContext and UI Entities.
        /// Returns true if the Entity is not in the process of Activating.
        static bool DeactivateEntity(AZ::Entity* entity, bool skipLogs = false);

        /// Deactivates Children Entities, if these are got by TransformHierarchyInformationBus request,
        /// in reverse order, and finally Deactivates the Parent Entity.
        /// Suitable both for GameContext and UI Entities.
        static void DeactivateEntity(const AZ::EntityId& entityId, bool skipLogs = false);

        /// Function transfers to the callback EntityIds of the Parent and then deeper Children, if these are got by TransformHierarchyInformationBus request
        static void ForeachInDescendants(const AZ::EntityId& id, AZStd::function< void(const AZ::EntityId&) > func, bool skipLogs = false); // Gruber patch begin // STA : 3rd argument renamed

        /// Function transfers to the callback (2nd argument):
        /// - first EntityIds of deepest Children, if these are got by TransformHierarchyInformationBus request,
        /// - and then finally the Parent itself.
        /// The reverse order is needed for deactivation / disabling / deletion, - as parents may destroy children while deactivating.
        // Gruber patch begin // STA : added for deactivation / disabling / deletion
        static void ForeachInDescendantsInReverseOrder(const AZ::EntityId& id, AZStd::function< void(const AZ::EntityId&) > func, bool skipLogs = false);

        /// Destroys the single Entity instance using
        /// AZ::ComponentApplicationBus::BroadcastResult(result, &AZ::ComponentApplicationBus::Events::DeleteEntity, id);
        static bool DestroyEntityInstance(const AZ::EntityId& id, bool skipLogs = false);

        /// Destroys a hierarchy of Entity instances, for example the claimed Entity instance of a spawnable and its descendants.
        /// Use to destroy claimed Entites instantiated with:
        /// - SpawnableInstantiator::Instantiate(), or
        /// - SpawnableInstantiator::InstantiateAsset(),
        /// instead of issuing AzFramework::GameEntityContextRequestBus::Events::DestroyGameEntity requests.
        static bool DestroyEntityInstanceAndDescendants(const AZ::EntityId& id, bool skipLogs = false);

// Gruber patch begin // VMED
        /// Detach an entity from its parent.
        /// Use to detach dynamic entity from their parents instantiated with:
        /// - SpawnableInstantiator::Instantiate(), or
        /// - SpawnableInstantiator::InstantiateAsset(),
        /// to avoid problems with parents for entities with network binding components (MADPORT-440)
        static bool DetachEntityFromParent(const AZ::EntityId& entityId);
// Gruber patch end // VMED
    };
} // namespace AzFramework

