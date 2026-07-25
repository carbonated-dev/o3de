/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/parallel/lock.h>
#include <Atom/RHI.Reflect/ShaderResourceGroupLayout.h>
#include <RHI/DescriptorSetAllocator.h>
#include <RHI/Device.h>

namespace AZ
{
    namespace Vulkan
    {
        namespace Internal
        {
            ///////////////////////////////////////////////////////////////////
            // DescriptorPoolFactory
            ///////////////////////////////////////////////////////////////////
            void DescriptorPoolFactory::Init(const Descriptor& descriptor)
            {
                m_descriptor = descriptor;
            }

            RHI::Ptr<DescriptorPool> DescriptorPoolFactory::CreateObject(const DescriptorPool::Descriptor& poolDescriptor)
            {
                RHI::Ptr<DescriptorPool> descriptorPool = DescriptorPool::Create();
                if (descriptorPool->Init(poolDescriptor) != RHI::ResultCode::Success)
                {
                    AZ_Printf("Vulkan", "Failed to initialize DescriptorSet");
                    return nullptr;
                }

                return descriptorPool;
            }

            void DescriptorPoolFactory::ResetObject(DescriptorPool& descriptorPool, const DescriptorPool::Descriptor& poolDescriptor)
            {
                AZ_UNUSED(poolDescriptor);
                descriptorPool.Reset();
            }

            void DescriptorPoolFactory::ShutdownObject(DescriptorPool& descriptorPool, bool isPoolShutdown)
            {
                AZ_UNUSED(isPoolShutdown);
                descriptorPool.Shutdown();
            }

            bool DescriptorPoolFactory::CollectObject(DescriptorPool& descriptorPool)
            {
                AZ_UNUSED(descriptorPool);
                return true;
            }

            const DescriptorPoolFactory::Descriptor& DescriptorPoolFactory::GetDescriptor() const
            {
                return m_descriptor;
            }    
            
            void DescriptorSetSubAllocator::Init(DescriptorPoolAllocator& descriptorPoolAllocator, Device& device, const DescriptorPool::Descriptor& poolDescriptor)
            {
                m_device = &device;
                m_descriptorPoolAllocator = &descriptorPoolAllocator;
                m_poolDescriptor = poolDescriptor;
            }

            RHI::Ptr<DescriptorSetSubAllocator::ObjectType> DescriptorSetSubAllocator::Allocate(DescriptorSetLayout& layout)
            {
                // Look for a pool that can allocate the descriptor set
                for (DescriptorPool* pool : m_pools)
                {
                    // Check that we don't get over the max descriptor sets count.
                    // In theory the pool would return a VK_ERROR_OUT_OF_POOL_MEMORY result but that would
                    // trigger a validation layer error that we want to avoid.
                    if ((pool->GetTotalObjectCount() + 1) > m_poolDescriptor.m_maxSets)
                    {
                        continue;
                    }

                    auto result = pool->Allocate(layout);
                    if (result.first == VK_SUCCESS)
                    {
                        return AZStd::move(result.second);
                    }
                    if (result.first != VK_ERROR_FRAGMENTED_POOL && result.first != VK_ERROR_OUT_OF_POOL_MEMORY)
                    {
                        AZ_Assert(false, "Failed to allocate descriptor set");
                        return nullptr;
                    }
                }

                DescriptorPool* newPool = m_descriptorPoolAllocator->Allocate(m_poolDescriptor);
                if (!newPool)
                {
                    return nullptr;
                }

                auto result = newPool->Allocate(layout);
                if (result.first != VK_SUCCESS)
                {
                    m_descriptorPoolAllocator->DeAllocate(newPool);
                    AZ_Assert(false, "Failed to allocate descriptor set");
                    return nullptr;
                }

                m_pools.push_front(newPool);
                return AZStd::move(result.second);
            }

            DescriptorPool::DescriptorSetList DescriptorSetSubAllocator::AllocateMultiple(
                DescriptorSetLayout& layout,
                uint32_t count)
            {
                if (count == 0 ||
                    count > RHI::Limits::Device::FrameCountMax ||
                    count > m_poolDescriptor.m_maxSets)
                {
                    AZ_Assert(false, "Invalid descriptor-set batch size.");
                    return {};
                }

                DescriptorPool::DescriptorSetList descriptorSets;
                for (uint32_t descriptorSetIndex = 0; descriptorSetIndex < count; ++descriptorSetIndex)
                {
                    RHI::Ptr<ObjectType> descriptorSet = Allocate(layout);
                    if (!descriptorSet)
                    {
                        for (RHI::Ptr<ObjectType>& allocatedDescriptorSet : descriptorSets)
                        {
                            DeAllocate(AZStd::move(allocatedDescriptorSet));
                        }
                        return {};
                    }

                    descriptorSets.emplace_back(AZStd::move(descriptorSet));
                }
                return descriptorSets;
            }

            void DescriptorSetSubAllocator::DeAllocate(RHI::Ptr<ObjectType> descriptorSet)
            {
                DescriptorPool* descriptorPool = const_cast<DescriptorPool*>(descriptorSet->GetDescriptor().m_descriptorPool);
                descriptorPool->DeAllocate(descriptorSet);
            }

            void DescriptorSetSubAllocator::Reset()
            {
                for (DescriptorPool* pool : m_pools)
                {
                    m_descriptorPoolAllocator->DeAllocate(pool);
                }

                m_descriptorPoolAllocator = nullptr;
            }

            void DescriptorSetSubAllocator::Collect()
            {
                auto it = m_pools.begin();
                while (it != m_pools.end())
                {
                    DescriptorPool* pool = *it;
                    pool->Collect();
                    if (pool->GetTotalObjectCount() == 0)
                    {
                        m_descriptorPoolAllocator->DeAllocate(pool);
                        it = m_pools.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
        }

        RHI::ResultCode DescriptorSetAllocator::Init(const Descriptor& descriptor)
        {
            AZ_Assert(m_isInitialized == false, "DescriptorSetAllocator already initialized!");
            m_descriptor = descriptor;
            Base::Init(*m_descriptor.m_device);

            m_poolDescriptor = {};
            m_poolDescriptor.m_device = m_descriptor.m_device;
            m_poolDescriptor.m_maxSets = m_descriptor.m_poolSize;
            m_poolDescriptor.m_constantDataPool = m_descriptor.m_constantDataPool;
            m_poolDescriptor.m_collectLatency = descriptor.m_frameCountMax;
            AZStd::unordered_map<VkDescriptorType, VkDescriptorPoolSize> sizesByType;
            for (const auto& layoutBinding : descriptor.m_layout->GetNativeLayoutBindings())
            {
                sizesByType[layoutBinding.descriptorType].descriptorCount += layoutBinding.descriptorCount * m_descriptor.m_poolSize;
            }
            m_poolDescriptor.m_descriptorPoolSizes.reserve(sizesByType.size());
            AZStd::transform(sizesByType.begin(), sizesByType.end(), AZStd::back_inserter(m_poolDescriptor.m_descriptorPoolSizes), [](auto &it)
            {
                it.second.type = it.first;
                return it.second; 
            });

            m_threadAllocationLaneContext.SetInitFunction(
                [this](AllocationLane*& threadLane)
                {
                    AZStd::lock_guard<AZStd::mutex> registryLock(m_allocationLaneRegistryMutex);
                    const uint32_t laneIndex = aznumeric_cast<uint32_t>(m_allocationLanes.size());
                    auto lane = AZStd::make_unique<AllocationLane>();

                    Internal::DescriptorPoolAllocator::Descriptor poolAllocatorDescriptor;
                    poolAllocatorDescriptor.m_device = m_descriptor.m_device;
                    poolAllocatorDescriptor.m_collectLatency = m_descriptor.m_frameCountMax;
                    lane->m_poolAllocator.Init(poolAllocatorDescriptor);

                    DescriptorPool::Descriptor lanePoolDescriptor = m_poolDescriptor;
                    lanePoolDescriptor.m_allocatorLaneIndex = laneIndex;
                    lane->m_subAllocator.Init(
                        lane->m_poolAllocator,
                        *m_descriptor.m_device,
                        lanePoolDescriptor);

                    threadLane = lane.get();
                    m_allocationLanes.push_back(AZStd::move(lane));
                });
            
            m_isInitialized = true;
            return RHI::ResultCode::Success;
        }

        DescriptorSetAllocator::AllocationLane& DescriptorSetAllocator::GetOrCreateThreadAllocationLane()
        {
            AllocationLane* lane = m_threadAllocationLaneContext.GetStorage();
            AZ_Assert(lane, "Failed to initialize a descriptor-set allocation lane.");
            return *lane;
        }

        DescriptorSetAllocator::AllocationLane* DescriptorSetAllocator::GetAllocationLane(uint32_t laneIndex)
        {
            AZStd::lock_guard<AZStd::mutex> registryLock(m_allocationLaneRegistryMutex);
            return laneIndex < m_allocationLanes.size() ? m_allocationLanes[laneIndex].get() : nullptr;
        }

        RHI::Ptr<DescriptorSetAllocator::ObjectType> DescriptorSetAllocator::Allocate(DescriptorSetLayout& layout)
        {
            AllocationLane& lane = GetOrCreateThreadAllocationLane();
            AZStd::lock_guard<AZStd::mutex> laneLock(lane.m_mutex);
            return lane.m_subAllocator.Allocate(layout);
        }

        DescriptorPool::DescriptorSetList DescriptorSetAllocator::AllocateMultiple(
            DescriptorSetLayout& layout,
            uint32_t count)
        {
            if (count == 0 || count > RHI::Limits::Device::FrameCountMax)
            {
                AZ_Assert(false, "Invalid descriptor-set batch size.");
                return {};
            }

            AllocationLane& lane = GetOrCreateThreadAllocationLane();
            AZStd::lock_guard<AZStd::mutex> laneLock(lane.m_mutex);
            return lane.m_subAllocator.AllocateMultiple(layout, count);
        }

        void DescriptorSetAllocator::DeAllocate(RHI::Ptr<ObjectType> descriptorSet)
        {
            const DescriptorPool* descriptorPool = descriptorSet->GetDescriptor().m_descriptorPool;
            AZ_Assert(descriptorPool, "Descriptor set has no owning descriptor pool.");
            AllocationLane* lane = GetAllocationLane(descriptorPool->GetDescriptor().m_allocatorLaneIndex);
            AZ_Assert(lane, "Descriptor set has an invalid allocator lane.");
            if (lane)
            {
                AZStd::lock_guard<AZStd::mutex> laneLock(lane->m_mutex);
                lane->m_subAllocator.DeAllocate(AZStd::move(descriptorSet));
            }
        }

        void DescriptorSetAllocator::Collect()
        {
            AZStd::lock_guard<AZStd::mutex> registryLock(m_allocationLaneRegistryMutex);
            for (const AZStd::unique_ptr<AllocationLane>& lane : m_allocationLanes)
            {
                AZStd::lock_guard<AZStd::mutex> laneLock(lane->m_mutex);
                lane->m_subAllocator.Collect();
                lane->m_poolAllocator.Collect();
            }
        }

        void DescriptorSetAllocator::Shutdown()
        {
            if (m_isInitialized)
            {
                m_threadAllocationLaneContext.Clear();
                AZStd::lock_guard<AZStd::mutex> registryLock(m_allocationLaneRegistryMutex);
                for (const AZStd::unique_ptr<AllocationLane>& lane : m_allocationLanes)
                {
                    AZStd::lock_guard<AZStd::mutex> laneLock(lane->m_mutex);
                    lane->m_subAllocator.Reset();
                    lane->m_poolAllocator.Shutdown();
                }
                m_allocationLanes.clear();
                m_isInitialized = false;
            }
        }    
    }
}
