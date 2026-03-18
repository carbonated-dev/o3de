/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Gruber patch begin // VMED
#ifdef CARBONATED

#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzCore/std/parallel/mutex.h>
#include <AzFramework/Spawnable/Spawnable.h>

namespace AzFramework
{
    struct EntitySpawnParams // TODO add required params here, use struct SEntitySpawnParams as reference
    {
        AZ::EntityId id;
        // Previously used entityId, in the case of reloading
        AZ::EntityId prevId;
        // Entity Flags.
        AZ::u32 nFlags; // e.g. ENTITY_FLAG_CASTSHADOW

        EntitySpawnParams()
            : id(0)
            , prevId(0)
            , nFlags(0)
        {
        }
    };

    class SpawnableEntityNotification : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        typedef AZ::Data::AssetType BusIdType;

        virtual void OnSpawn(const AZ::Entity* pEntity, const AZStd::shared_ptr<AzFramework::EntitySpawnParams>& params) = 0;
        virtual bool OnRemove(const AZ::Entity* pEntity) = 0;
    };

    using SpawnableEntityNotificationBus = AZ::EBus<SpawnableEntityNotification>;
} // namespace AzFramework

#endif
// Gruber patch end // VMED
