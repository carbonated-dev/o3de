/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/parallel/lock.h>
#include <AzCore/std/parallel/thread.h>
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
                DescriptorPool::DescriptorSetList descriptorSets =
                    AllocateBatch(layout, 1);
                if (descriptorSets.empty())
                {
                    return nullptr;
                }
                return AZStd::move(descriptorSets.front());
            }

            DescriptorPool::DescriptorSetList
            DescriptorSetSubAllocator::AllocateBatch(
                DescriptorSetLayout& layout,
                uint32_t count)
            {
                m_allocationCount += count;

                auto allocateFromPool =
                    [this, &layout, count](
                        DescriptorPool* pool)
                    -> DescriptorPool::DescriptorSetList
                {
                    // Check that we don't get over the max descriptor sets count.
                    // In theory the pool would return a VK_ERROR_OUT_OF_POOL_MEMORY result but that would
                    // trigger a validation layer error that we want to avoid.
                    if ((pool->GetTotalObjectCount() + count) >
                        m_poolDescriptor.m_maxSets)
                    {
                        m_poolState.MarkFull(pool);
                        return {};
                    }

                    ++m_poolAttemptCount;
                    DescriptorPool::BatchAllocResult result =
                        pool->AllocateBatch(layout, count);
                    VkResult vkResult = result.first;
                    if (vkResult == VK_SUCCESS)
                    {
                        if (pool->GetTotalObjectCount() >= m_poolDescriptor.m_maxSets)
                        {
                            m_poolState.MarkFull(pool);
                        }
                        return AZStd::move(result.second);
                    }

                    m_poolState.MarkFull(pool);
                    if (vkResult != VK_ERROR_FRAGMENTED_POOL && vkResult != VK_ERROR_OUT_OF_POOL_MEMORY)
                    {
                        AZ_Assert(false, "Failed to Allocate descriptor set");
                    }
                    return {};
                };

                if (DescriptorPool* currentPool = m_poolState.GetCurrent())
                {
                    DescriptorPool::DescriptorSetList descriptorSets =
                        allocateFromPool(currentPool);
                    if (!descriptorSets.empty())
                    {
                        return descriptorSets;
                    }
                }

                while (DescriptorPool* availablePool = m_poolState.TakeAvailable())
                {
                    DescriptorPool::DescriptorSetList descriptorSets =
                        allocateFromPool(availablePool);
                    if (!descriptorSets.empty())
                    {
                        return descriptorSets;
                    }
                }

                DescriptorPool* newPool =
                    m_descriptorPoolAllocator->Allocate(m_poolDescriptor);
                if (!newPool)
                {
                    return {};
                }

                ++m_poolAcquireCount;
                m_poolState.SetCurrent(newPool);
                DescriptorPool::DescriptorSetList descriptorSets =
                    allocateFromPool(newPool);
                AZ_Assert(
                    descriptorSets.size() == count,
                    "Failed to allocate descriptor sets from a new descriptor pool.");
                return descriptorSets;
            }

            void DescriptorSetSubAllocator::DeAllocate(RHI::Ptr<ObjectType> descriptorSet)
            {
                DescriptorPool* descriptorPool = const_cast<DescriptorPool*>(descriptorSet->GetDescriptor().m_descriptorPool);
                descriptorPool->DeAllocate(descriptorSet);
                m_poolState.MarkPendingCollect(descriptorPool);
            }

            void DescriptorSetSubAllocator::Reset()
            {
                for (DescriptorPool* pool : m_poolState.GetAllPools())
                {
                    m_descriptorPoolAllocator->DeAllocate(pool);
                }

                m_poolState.Reset();
                m_descriptorPoolAllocator = nullptr;
            }

            void DescriptorSetSubAllocator::Collect()
            {
                AZStd::vector<DescriptorPool*> pendingCollectPools(
                    m_poolState.GetPendingCollectPools().begin(),
                    m_poolState.GetPendingCollectPools().end());

                for (DescriptorPool* pool : pendingCollectPools)
                {
                    pool->Collect();
                    if (pool->GetPendingObjectCount() == 0)
                    {
                        m_poolState.ClearPendingCollect(pool);
                    }

                    if (pool->GetTotalObjectCount() == 0)
                    {
                        m_poolState.Remove(pool);
                        m_descriptorPoolAllocator->DeAllocate(pool);
                    }
                    else if (pool->GetTotalObjectCount() < m_poolDescriptor.m_maxSets)
                    {
                        m_poolState.MarkAvailable(pool);
                    }
                    else
                    {
                        m_poolState.MarkFull(pool);
                    }
                }
            }

            DescriptorSetAllocatorStatistics DescriptorSetSubAllocator::GetStatistics() const
            {
                const DescriptorPoolState::Counts counts = m_poolState.GetCounts();
                DescriptorSetAllocatorStatistics statistics;
                statistics.m_allocationCount = m_allocationCount;
                statistics.m_poolAttemptCount = m_poolAttemptCount;
                statistics.m_poolAcquireCount = m_poolAcquireCount;
                statistics.m_totalPoolCount = counts.m_total;
                statistics.m_availablePoolCount = counts.m_available;
                statistics.m_fullPoolCount = counts.m_full;
                statistics.m_pendingCollectPoolCount = counts.m_pendingCollect;
                statistics.m_hasCurrentPool = counts.m_hasCurrent;
                return statistics;
            }
        }

        RHI::ResultCode DescriptorSetAllocator::Init(const Descriptor& descriptor)
        {
            AZ_Assert(m_isInitialized == false, "DescriptorSetAllocator already initialized!");
            m_descriptor = descriptor;
            Base::Init(*m_descriptor.m_device);

            DescriptorPool::Descriptor poolDescriptor;
            poolDescriptor.m_device = m_descriptor.m_device;
            poolDescriptor.m_maxSets = m_descriptor.m_poolSize;
            poolDescriptor.m_constantDataAllocator = m_descriptor.m_constantDataAllocator;
            poolDescriptor.m_collectLatency = descriptor.m_frameCountMax;
            AZStd::unordered_map<VkDescriptorType, VkDescriptorPoolSize> sizesByType;
            for (const auto& layoutBinding : descriptor.m_layout->GetNativeLayoutBindings())
            {
                sizesByType[layoutBinding.descriptorType].descriptorCount += layoutBinding.descriptorCount * m_descriptor.m_poolSize;
            }
            poolDescriptor.m_descriptorPoolSizes.reserve(sizesByType.size());
            AZStd::transform(sizesByType.begin(), sizesByType.end(), AZStd::back_inserter(poolDescriptor.m_descriptorPoolSizes), [](auto &it) 
            {
                it.second.type = it.first;
                return it.second; 
            });

            constexpr uint32_t MaxAllocationLaneCount = 8;
            const uint32_t hardwareThreadCount =
                AZStd::max(1u, AZStd::thread::hardware_concurrency());
            const uint32_t allocationLaneCount =
                m_descriptor.m_enableConcurrentAllocation
                ? AZStd::min(hardwareThreadCount, MaxAllocationLaneCount)
                : 1;
            m_allocationLanes.reserve(allocationLaneCount);

            for (uint32_t laneIndex = 0; laneIndex < allocationLaneCount; ++laneIndex)
            {
                auto lane = AZStd::make_unique<AllocationLane>();

                Internal::DescriptorPoolAllocator::Descriptor poolAllocatorDescriptor;
                poolAllocatorDescriptor.m_device = m_descriptor.m_device;
                poolAllocatorDescriptor.m_collectLatency = descriptor.m_frameCountMax;
                lane->m_poolAllocator.Init(poolAllocatorDescriptor);

                poolDescriptor.m_allocatorLaneIndex = laneIndex;
                lane->m_subAllocator.Init(
                    lane->m_poolAllocator,
                    *m_descriptor.m_device,
                    poolDescriptor);
                m_allocationLanes.push_back(AZStd::move(lane));
            }
            
            m_isInitialized = true;
            return RHI::ResultCode::Success;
        }

        DescriptorSetAllocator::AllocationLane&
        DescriptorSetAllocator::SelectAllocationLane()
        {
            AZ_Assert(
                !m_allocationLanes.empty(),
                "DescriptorSetAllocator has no allocation lanes.");
            const uint32_t laneIndex =
                m_nextAllocationLane.fetch_add(1, AZStd::memory_order_relaxed) %
                aznumeric_cast<uint32_t>(m_allocationLanes.size());
            return *m_allocationLanes[laneIndex];
        }

        RHI::Ptr<DescriptorSetAllocator::ObjectType> DescriptorSetAllocator::Allocate(DescriptorSetLayout& layout)
        {
            DescriptorSetList descriptorSets = AllocateBatch(layout, 1);
            return descriptorSets.empty() ? nullptr : AZStd::move(descriptorSets.front());
        }

        DescriptorSetAllocator::DescriptorSetList
        DescriptorSetAllocator::AllocateBatch(
            DescriptorSetLayout& layout,
            uint32_t count)
        {
            AZ_Assert(
                count <= RHI::Limits::Device::FrameCountMax,
                "Descriptor-set batch exceeds the maximum supported frame count.");
            ConstantDataAllocator::AllocationList constantDataAllocations;
            if (m_descriptor.m_constantDataAllocator &&
                layout.GetConstantDataSize())
            {
                constantDataAllocations =
                    m_descriptor.m_constantDataAllocator->AllocateBatch(count);
                if (constantDataAllocations.size() != count)
                {
                    return {};
                }
            }

            AllocationLane& lane = SelectAllocationLane();
            AZStd::lock_guard<AZStd::mutex> lock(lane.m_mutex);
            DescriptorSetList descriptorSets =
                lane.m_subAllocator.AllocateBatch(
                    layout,
                    count);

            if (descriptorSets.size() != count)
            {
                return {};
            }

            for (uint32_t descriptorSetIndex = 0;
                 descriptorSetIndex < constantDataAllocations.size();
                 ++descriptorSetIndex)
            {
                if (!descriptorSets[descriptorSetIndex]->InitConstantData(
                        AZStd::move(
                            constantDataAllocations[descriptorSetIndex])))
                {
                    for (RHI::Ptr<ObjectType>& allocatedDescriptorSet :
                         descriptorSets)
                    {
                        lane.m_subAllocator.DeAllocate(
                            AZStd::move(allocatedDescriptorSet));
                    }
                    descriptorSets.clear();
                    break;
                }
            }
            return descriptorSets;
        }

        void DescriptorSetAllocator::DeAllocate(RHI::Ptr<ObjectType> descriptorSet)
        {
            const DescriptorPool* descriptorPool =
                descriptorSet->GetDescriptor().m_descriptorPool;
            AZ_Assert(descriptorPool, "Descriptor set has no owning descriptor pool.");
            const uint32_t laneIndex =
                descriptorPool->GetDescriptor().m_allocatorLaneIndex;
            AZ_Assert(
                laneIndex < m_allocationLanes.size(),
                "Descriptor set has an invalid allocator lane.");

            AllocationLane& lane = *m_allocationLanes[laneIndex];
            AZStd::lock_guard<AZStd::mutex> lock(lane.m_mutex);
            lane.m_subAllocator.DeAllocate(AZStd::move(descriptorSet));
        }

        void DescriptorSetAllocator::Collect()
        {
            for (const AZStd::unique_ptr<AllocationLane>& lane : m_allocationLanes)
            {
                AZStd::lock_guard<AZStd::mutex> lock(lane->m_mutex);
                lane->m_subAllocator.Collect();
                lane->m_poolAllocator.Collect();
            }
        }

        DescriptorSetAllocator::Statistics DescriptorSetAllocator::GetStatistics() const
        {
            Statistics combinedStatistics;
            for (const AZStd::unique_ptr<AllocationLane>& lane : m_allocationLanes)
            {
                AZStd::lock_guard<AZStd::mutex> lock(lane->m_mutex);
                const Statistics laneStatistics =
                    lane->m_subAllocator.GetStatistics();
                combinedStatistics.m_allocationCount +=
                    laneStatistics.m_allocationCount;
                combinedStatistics.m_poolAttemptCount +=
                    laneStatistics.m_poolAttemptCount;
                combinedStatistics.m_poolAcquireCount +=
                    laneStatistics.m_poolAcquireCount;
                combinedStatistics.m_totalPoolCount +=
                    laneStatistics.m_totalPoolCount;
                combinedStatistics.m_availablePoolCount +=
                    laneStatistics.m_availablePoolCount;
                combinedStatistics.m_fullPoolCount +=
                    laneStatistics.m_fullPoolCount;
                combinedStatistics.m_pendingCollectPoolCount +=
                    laneStatistics.m_pendingCollectPoolCount;
                combinedStatistics.m_hasCurrentPool |=
                    laneStatistics.m_hasCurrentPool;
            }
            return combinedStatistics;
        }

        void DescriptorSetAllocator::Shutdown()
        {
            if (m_isInitialized)
            {
                for (const AZStd::unique_ptr<AllocationLane>& lane :
                     m_allocationLanes)
                {
                    AZStd::lock_guard<AZStd::mutex> lock(lane->m_mutex);
                    lane->m_subAllocator.Reset();
                    lane->m_poolAllocator.Shutdown();
                }
                m_allocationLanes.clear();
                m_isInitialized = false;
            }
        }    
    }
}
