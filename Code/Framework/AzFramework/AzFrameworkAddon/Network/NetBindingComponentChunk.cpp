/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <AzFrameworkAddon/Network/NetBindingComponentChunk.h>
#include <AzFrameworkAddon/Network/NetBindingComponent.h>
#include <AzFrameworkAddon/Network/NetBindingSystemBus.h>
#include <AzFrameworkAddon/Network/NetBindingEventsBus.h>
#include <AzFramework/Entity/EntityContextBus.h>
#include <AzFramework/Slice/SliceEntityBus.h>
#include <GridMate/Serialize/Buffer.h>
#include <GridMate/Serialize/UuidMarshal.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Slice/SliceComponent.h>
#include <AzCore/Serialization/ObjectStream.h>
#include <AzCore/IO/ByteContainerStream.h>
#include <AzFramework/Spawnable/SpawnableEntitiesInterface.h>

#include <AzFrameworkAddon/Helpers/EntityHelpers.h> // Helper methods to evaluate Entities moved here

namespace AzFramework
{
    NetBindingComponentChunk::SpawnInfo::SpawnInfo()
        : m_runtimeEntityId(AZ::EntityId::InvalidEntityId)
        , m_owningContextId(UnspecifiedNetBindingContextSequence)
        , m_staticEntityId(AZ::EntityId::InvalidEntityId)
        , m_spawnableInstanceId(UnspecifiedSliceInstanceId)     // Gruber patch. LVB. Was m_sliceInstanceId
        , m_spawnableAssetId(UnspecifiedSliceInstanceId, 0)     // Gruber patch. LVB. Was m_sliceAssetId
    {
    }

    bool NetBindingComponentChunk::SpawnInfo::operator==(const SpawnInfo& rhs) const
    {
        return m_owningContextId == rhs.m_owningContextId
            && m_runtimeEntityId == rhs.m_runtimeEntityId
            && m_staticEntityId == rhs.m_staticEntityId
            && m_serializedState == rhs.m_serializedState
            && m_spawnableAssetId == rhs.m_spawnableAssetId;    // Gruber patch. LVB. Was m_sliceAssetId
    }

    bool NetBindingComponentChunk::SpawnInfo::ContainsSerializedState() const
    {
        return !m_serializedState.empty();
    }

    void NetBindingComponentChunk::SpawnInfo::Marshaler::Marshal(GridMate::WriteBuffer& wb, const SpawnInfo& data)
    {
        wb.Write(data.m_owningContextId, GridMate::VlqU32Marshaler());
        wb.Write(data.m_runtimeEntityId);

        bool useSerializedState = data.ContainsSerializedState();
#if defined(CARBONATED_ENGINE_LOG)
        AZ_Printf("NetBindingComponentChunk", "Marshal m_owningContextId=%u m_runtimeEntityId=%llu, m_staticEntityId=%llu, useSerializedState=%i", data.m_owningContextId, data.m_runtimeEntityId, data.m_staticEntityId, useSerializedState);
#endif
        wb.Write(useSerializedState);
        if (useSerializedState)
        {
            wb.Write(data.m_serializedState);
        }
        else
        {
            wb.Write(data.m_spawnableAssetId);  // Gruber patch. LVB. Was m_sliceAssetId
            wb.Write(data.m_staticEntityId);
            wb.Write(data.m_spawnableInstanceId);   // Gruber patch. LVB. Was m_sliceInstanceId
        }
    }

    void NetBindingComponentChunk::SpawnInfo::Marshaler::Unmarshal(SpawnInfo& data, GridMate::ReadBuffer& rb)
    {
        rb.Read(data.m_owningContextId, GridMate::VlqU32Marshaler());
        rb.Read(data.m_runtimeEntityId);

        bool hasSerializedState = false;
        rb.Read(hasSerializedState);
        if (hasSerializedState)
        {
            rb.Read(data.m_serializedState);
        }
        else
        {
            rb.Read(data.m_spawnableAssetId);   // Gruber patch. LVB. Was m_sliceAssetId
            rb.Read(data.m_staticEntityId);
            rb.Read(data.m_spawnableInstanceId);    // Gruber patch. LVB. Was m_sliceInstanceId
        }
#if defined(CARBONATED_ENGINE_LOG)
        AZ_Printf("NetBindingComponentChunk", "Unmarshal m_owningContextId=%u, m_runtimeEntityId=%llu, m_staticEntityId=%llu", data.m_owningContextId, data.m_runtimeEntityId, data.m_staticEntityId);
#endif
    }

    NetBindingComponentChunk::NetBindingComponentChunk()
        : m_bindingComponent(nullptr)
        , m_spawnInfo("SpawnInfo")
        , m_bindMap("ComponentBindMap")
    {
        m_spawnInfo.SetMaxIdleTime(0.f);
        m_bindMap.SetMaxIdleTime(0.f);
    }

    void NetBindingComponentChunk::OnReplicaActivate(const GridMate::ReplicaContext& rc)
    {
        (void)rc;
        if (IsMaster())
        {
            // Get and store entity spawn data
            AZ_Assert(m_bindingComponent, "Entity binding is invalid!");

            m_spawnInfo.Modify([&](SpawnInfo& spawnInfo)
                {
                    spawnInfo.m_runtimeEntityId = static_cast<AZ::u64>(m_bindingComponent->GetEntity()->GetId());

                    bool isProceduralEntity = true;
                    // Gruber patch begin. // LVB. // Using spawnableInstanceId
                    //AZ::SliceComponent::SliceInstanceAddress sliceInfo;   
                    AZStd::shared_ptr<SpawnableInstanceDescriptor> spawnableInfo;

                    EntityContextId contextId = EntityContextId::CreateNull();
                    EBUS_EVENT_ID_RESULT(contextId, m_bindingComponent->GetEntityId(), EntityIdContextQueryBus, GetOwningContextId);
                    if (!contextId.IsNull())
                    {
                        EBUS_EVENT_RESULT(spawnInfo.m_owningContextId, NetBindingSystemBus, GetCurrentContextSequence);

                        // Gruber patch begin // LVB
                        // EntityContext::GetOwningSlice() is not supported
                        // was EBUS_EVENT_ID_RESULT(sliceInfo, m_bindingComponent->GetEntityId(), EntityIdContextQueryBus, GetOwningSlice);
                        // MADPORT-259 Convert info in GetOwningSlice from Slice to Prefab
                        //AzFramework::SliceEntityRequestBus::EventResult(sliceInfo, m_bindingComponent->GetEntityId(),
                            //&AzFramework::SliceEntityRequestBus::Events::GetOwningSlice);
                        // Gruber patch end // LVB

                        spawnableInfo = SpawnableEntitiesInterface::Get()->GetOwningSpawnable(m_bindingComponent->GetEntityId());

                        bool isDynamicSliceEntity = spawnableInfo && spawnableInfo->IsValid();

                        isProceduralEntity = !m_bindingComponent->IsLevelSliceEntity() && !isDynamicSliceEntity;
                        // Gruber patch end. // LVB. // Using spawnableInstanceId
#if defined(CARBONATED_ENGINE_LOG)
                        AZ_Printf("NetBindingComponentChunk", "m_runtimeEntityId=%llu, isDynamicSliceEntity=%i, m_bindingComponent->IsLevelSliceEntity()=%i", spawnInfo.m_runtimeEntityId, isDynamicSliceEntity, m_bindingComponent->IsLevelSliceEntity());
#endif
                    }
                    else
                    {
#if defined(CARBONATED_ENGINE_LOG)
                        AZ_Printf("NetBindingComponentChunk", "m_runtimeEntityId=%llu, contextId=NULL", spawnInfo.m_runtimeEntityId);
#endif
                    }

                    if (isProceduralEntity)
                    {
#if defined(CARBONATED_ENGINE_LOG)
                        AZ_Printf("NetBindingComponentChunk", "As master m_spawnInfo. m_runtimeEntityId=%llu, isProceduralEntity=TRUE", spawnInfo.m_runtimeEntityId);
#endif
                        // write cloning info
                        AZ::SerializeContext* sc = nullptr;
                        EBUS_EVENT_RESULT(sc, AZ::ComponentApplicationBus, GetSerializeContext);
                        AZ_Assert(sc, "Can't find SerializeContext!");
                        AZ::IO::ByteContainerStream<AZStd::vector<AZ::u8>> spawnDataStream(&spawnInfo.m_serializedState);
                        AZ::ObjectStream* objStream = AZ::ObjectStream::Create(&spawnDataStream, *sc, AZ::DataStream::ST_BINARY);
                        objStream->WriteClass(m_bindingComponent->GetEntity());
                        objStream->Finalize();
                    }
                    else
                    {
#if defined(CARBONATED_ENGINE_LOG)
                        AZ_Printf("NetBindingComponentChunk", "As master m_spawnInfo. m_runtimeEntityId=%llu, isProceduralEntity=FALSE, spawnableInfo.IsValid()=%i, name='%s'", spawnInfo.m_runtimeEntityId, spawnableInfo->IsValid(), m_bindingComponent->GetEntity()->GetName().c_str());
#endif
                        // write slice info
                        if (spawnableInfo && spawnableInfo->IsValid())
                        {
                            spawnInfo.m_spawnableAssetId = AZStd::make_pair(spawnableInfo->GetAssetId().m_guid, spawnableInfo->GetAssetId().m_subId);   // Gruber patch. LVB. Was m_sliceAssetId
                            spawnInfo.m_spawnableInstanceId = spawnableInfo->GetInstanceId();  // Gruber patch. LVB. Was m_sliceInstanceId
                        }

                        AZ::EntityId staticEntityId;
                        EBUS_EVENT_RESULT(staticEntityId, NetBindingSystemBus, GetStaticIdFromEntityId, m_bindingComponent->GetEntity()->GetId());
                        spawnInfo.m_staticEntityId = static_cast<AZ::u64>(staticEntityId);

#if defined(CARBONATED_ENGINE_LOG) && !defined(_RELEASE)
                        AZ_Printf("NetBindingComponentChunk", "BoundSpawnInfo for Entity (%s.'%s') - staticEntityId=%s. SpawnableAsset:%s SpawnableInstance %s",
                            m_bindingComponent->GetEntityId().ToString().c_str(),
                            m_bindingComponent->GetEntity()->GetName().c_str(),
                            staticEntityId.ToString().c_str(),
                            spawnInfo.m_spawnableAssetId.first.ToFixedString().c_str(),
                            spawnInfo.m_spawnableInstanceId.ToFixedString().c_str());
#endif

                    }

                    return true;
                });
        }
        else
        {
            AZ::EntityId runtimeEntityId(m_spawnInfo.Get().m_runtimeEntityId);
            NetBindingContextSequence owningContextId = m_spawnInfo.Get().m_owningContextId;
#if defined(CARBONATED_ENGINE_LOG)
            {
                const AZ::Entity* runtimeEntity = AzFramework::EntityHelpers::GetEntity(AZ::EntityId(runtimeEntityId));
                if (runtimeEntity)
                {
                    AZ_Printf("NetBindingComponentChunk", "As slave. owningContextId=%u, m_runtimeEntityId=%llu, name=%s", owningContextId, runtimeEntityId, runtimeEntity->GetName().c_str());
                }
                else
                {
                    AZ_Printf("NetBindingComponentChunk", "As slave. owningContextId=%u, m_runtimeEntityId=%llu", owningContextId, runtimeEntityId);
                }
            }
#endif
            //TODO Move to Filter Hook
            //      Reject and cancel sessions with duplicate MachineIds?
            //      Reject and cancel sessions with duplicate entity ID creation requests?
            //Check MachineId collision
            bool collision = AZ::Entity::GetProcessSignature() == (m_spawnInfo.Get().m_runtimeEntityId & 0xFFFFFFFF);
            AZ_Error("GridMate", !collision, "Replica received with duplicate Entity Machine IDs. Ignoring");

            if (!collision)
            {
                //Check EntityID collision
                AZ::Entity* localEntity = nullptr;
                EBUS_EVENT_RESULT(localEntity, AZ::ComponentApplicationBus, FindEntity, runtimeEntityId);

                /*
                 * Only false if no machine ID collision and no entity ID collision
                 * And the entity is already active, it's possible the entity already exists in deactivated state as a cache mechanism
                 */
                collision = (localEntity != nullptr) && (localEntity->GetState() == AZ::Entity::State::Active);
            }

            /**
             * Special case - static entities should not count as duplicates.
             * Static entities are loaded with the level and will be bounded here.
             */
            if (collision)
            {
                AZ::EntityId staticEntityId;
                EBUS_EVENT_RESULT(staticEntityId, NetBindingSystemBus, GetStaticIdFromEntityId, runtimeEntityId);
                if (staticEntityId == runtimeEntityId)
                {
                    collision = false;
                }
            }

            if (!collision)    //Ignore duplicate runtime entity IDs
            {
                if (m_spawnInfo.Get().ContainsSerializedState())
                {
#if defined(CARBONATED_ENGINE_LOG)
                    AZ_Printf("NetBinding", "OnReplicaActivate. Slave. ContainsSerializedState. GetReplicaId()=%u", GetReplicaId());
#endif
                    // Spawn the entity from stream input data
                    AZ::IO::MemoryStream spawnData(m_spawnInfo.Get().m_serializedState.data(), m_spawnInfo.Get().m_serializedState.size());
                    EBUS_EVENT(NetBindingSystemBus, SpawnEntityFromStream, spawnData, runtimeEntityId, GetReplicaId(), owningContextId);
                }
                else
                {
                    NetBindingSliceContext spawnContext;
                    spawnContext.m_contextSequence = owningContextId;
                    spawnContext.m_spawnableAssetId = AZ::Data::AssetId(m_spawnInfo.Get().m_spawnableAssetId.first, m_spawnInfo.Get().m_spawnableAssetId.second);   // Gruber patch. LVB. Was m_sliceAssetId
                    spawnContext.m_runtimeEntityId = runtimeEntityId;
                    spawnContext.m_staticEntityId = AZ::EntityId(m_spawnInfo.Get().m_staticEntityId);
                    spawnContext.m_spawnableInstanceId = m_spawnInfo.Get().m_spawnableInstanceId;   // Gruber patch. LVB. Was m_sliceInstanceId

#if defined(CARBONATED_ENGINE_LOG)
                    AZ_Printf("NetBinding", "OnReplicaActivate. Slave. !ContainsSerializedState. m_staticEntityId=%s, GetReplicaId()=%u", spawnContext.m_staticEntityId.ToString().c_str(), GetReplicaId());
#endif
                    EBUS_EVENT(NetBindingSystemBus, SpawnEntityFromSlice, GetReplicaId(), spawnContext);
                }
            }
            else    //Fail early to prevent unnecessary spawning of duplicate entity IDs
            {
                //Misconfiguration or potential cheating/DoS?
                AZ_Warning("NetBinding", false, "Received duplicate Entity ID %llu. Ignoring.", runtimeEntityId);
            }
        }
    }

    void NetBindingComponentChunk::OnReplicaDeactivate(const GridMate::ReplicaContext& rc)
    {
        (void)rc;
        if (m_bindingComponent)
        {
            m_bindingComponent->UnbindFromNetwork();
        }
    }

    bool NetBindingComponentChunk::AcceptChangeOwnership(GridMate::PeerId requestor, const GridMate::ReplicaContext& rc)
    {
        bool result = true;

        if (m_bindingComponent)
        {
            EBUS_EVENT_ID_RESULT(result, m_bindingComponent->GetEntityId(), NetBindingEventsBus, OnEntityAcceptChangeOwnership, requestor, rc);
        }

        return result;
    }

    void NetBindingComponentChunk::OnReplicaChangeOwnership(const GridMate::ReplicaContext& rc)
    {
        if (m_bindingComponent)
        {
            EBUS_EVENT_ID(m_bindingComponent->GetEntityId(), NetBindingEventsBus, OnEntityChangeOwnership, rc);
        }
    }
}   // namespace AzFramework
