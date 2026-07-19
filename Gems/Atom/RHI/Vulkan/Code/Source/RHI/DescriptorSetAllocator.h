/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/std/parallel/mutex.h>
#include <AzCore/std/parallel/atomic.h>
#include <Atom/RHI/Object.h>
#include <Atom/RHI/ObjectPool.h>
#include <Atom/RHI.Reflect/Limits.h>
#include <AzCore/std/containers/fixed_vector.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <RHI/DescriptorPool.h>
#include <RHI/DescriptorPoolState.h>
#include <RHI/DescriptorSet.h>

namespace AZ
{
    namespace Vulkan
    {
        class Device;
        class ConstantDataAllocator;

        struct DescriptorSetAllocatorStatistics
        {
            uint64_t m_allocationCount = 0;
            uint64_t m_poolAttemptCount = 0;
            uint64_t m_poolAcquireCount = 0;
            size_t m_totalPoolCount = 0;
            size_t m_availablePoolCount = 0;
            size_t m_fullPoolCount = 0;
            size_t m_pendingCollectPoolCount = 0;
            bool m_hasCurrentPool = false;
        };

        namespace Internal
        {
            class DescriptorPoolFactory final
                : public RHI::ObjectFactoryBase<DescriptorPool>
            {
                using Base = RHI::ObjectFactoryBase<DescriptorPool>;
            public:
                struct Descriptor
                {
                    Device* m_device = nullptr;
                };

                void Init(const Descriptor& descriptor);

                RHI::Ptr<DescriptorPool> CreateObject(const DescriptorPool::Descriptor& poolDescriptor);

                void ResetObject(DescriptorPool& descriptorPool, const DescriptorPool::Descriptor& poolDescriptor);
                void ShutdownObject(DescriptorPool& descriptorPool, bool isPoolShutdown);
                bool CollectObject(DescriptorPool& descriptorPool);

                const Descriptor& GetDescriptor() const;

            private:
                Descriptor m_descriptor;
            };

            struct DescriptorPoolAllocatorlTraits : public RHI::ObjectPoolTraits
            {
                using ObjectType = DescriptorPool;
                using ObjectFactoryType = DescriptorPoolFactory;
                using MutexType = RHI::NullMutex;
            };

            using DescriptorPoolAllocator = RHI::ObjectPool<DescriptorPoolAllocatorlTraits>;

            class DescriptorSetSubAllocator final
            {
                using ObjectType = DescriptorSet;
            public:
                DescriptorSetSubAllocator() = default;
                DescriptorSetSubAllocator(const DescriptorSetSubAllocator&) = delete;

                void Init(DescriptorPoolAllocator& descriptorPoolAllocator, Device& device, const DescriptorPool::Descriptor& poolDescriptor);

                RHI::Ptr<ObjectType> Allocate(DescriptorSetLayout& layout);
                DescriptorPool::DescriptorSetList AllocateBatch(
                    DescriptorSetLayout& layout,
                    uint32_t count);
                void DeAllocate(RHI::Ptr<ObjectType> descriptorSet);
                void Reset();
                void Collect();
                DescriptorSetAllocatorStatistics GetStatistics() const;

            private:
                Device* m_device;
                DescriptorPoolAllocator* m_descriptorPoolAllocator = nullptr;
                DescriptorPool::Descriptor m_poolDescriptor;
                DescriptorPoolState m_poolState;
                uint64_t m_allocationCount = 0;
                uint64_t m_poolAttemptCount = 0;
                uint64_t m_poolAcquireCount = 0;
            };
        }

        /**
        * Allocator for creating descriptor sets.
        * Each descriptor set is allocated from a descriptor set pool.
        * When the pool can't allocate more descriptor sets we create a new pool.
        * We use the return value from Vulkan to check if the pool ran out of memory and we need to create
        * a new one. A DescriptorSetAllocator is used to generate new descriptor set pools when needed.
        */
        class DescriptorSetAllocator final
            : public RHI::DeviceObject
        {
            using Base = RHI::DeviceObject;
            using ObjectType = DescriptorSet;

        public:
            AZ_CLASS_ALLOCATOR(DescriptorSetAllocator, AZ::SystemAllocator);

            struct Descriptor
            {
                Device* m_device = nullptr;
                uint32_t m_frameCountMax = RHI::Limits::Device::FrameCountMax;
                uint32_t m_poolSize = 0;
                const DescriptorSetLayout* m_layout = nullptr;
                AZStd::shared_ptr<ConstantDataAllocator> m_constantDataAllocator;
                bool m_enableConcurrentAllocation = false;
            };

            DescriptorSetAllocator() = default;
            ~DescriptorSetAllocator() = default;

            using Statistics = DescriptorSetAllocatorStatistics;
            using DescriptorSetList =
                AZStd::fixed_vector<RHI::Ptr<ObjectType>, RHI::Limits::Device::FrameCountMax>;

            RHI::ResultCode Init(const Descriptor& descriptor);
            RHI::Ptr<ObjectType> Allocate(DescriptorSetLayout& layout);
            DescriptorSetList AllocateBatch(DescriptorSetLayout& layout, uint32_t count);
            void DeAllocate(RHI::Ptr<ObjectType> descriptor);
            void Collect();
            Statistics GetStatistics() const;
            void Shutdown() override;

        private:
            struct AllocationLane
            {
                mutable AZStd::mutex m_mutex;
                Internal::DescriptorSetSubAllocator m_subAllocator;
                Internal::DescriptorPoolAllocator m_poolAllocator;
            };

            AllocationLane& SelectAllocationLane();

            AZStd::vector<AZStd::unique_ptr<AllocationLane>> m_allocationLanes;
            AZStd::atomic_uint32_t m_nextAllocationLane{ 0 };
            Descriptor m_descriptor;
            bool m_isInitialized = false;
        };
    }
}
