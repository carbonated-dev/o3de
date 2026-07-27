/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include <Atom/RHI.Reflect/ShaderResourceGroupLayoutDescriptor.h>
#include <Atom/RHI.Reflect/ShaderResourceGroupLayout.h>
#include <AzCore/Utils/TypeHash.h>
#include <AzCore/std/parallel/lock.h>
#include <Atom/RHI.Reflect/Vulkan/Conversion.h>
#include <RHI/DescriptorPool.h>
#include <RHI/DescriptorSet.h>
#include <RHI/DescriptorSetLayout.h>
#include <RHI/Device.h>
#include <Atom/RHI.Reflect/VkAllocator.h>

#if defined(CARBONATED)
#include <Atom/RHI.Reflect/Bits.h>
#include <Atom/RHI/BufferPool.h>
#include <RHI/Buffer.h>
#include <RHI/BufferPool.h>
#endif

namespace AZ
{
    namespace Vulkan
    {
        RHI::Ptr<DescriptorPool> DescriptorPool::Create()
        {
            return aznew DescriptorPool();
        }

        DescriptorPool::~DescriptorPool()
        {
        }

        RHI::ResultCode DescriptorPool::Init(const Descriptor& descriptor)
        {
            m_descriptor = descriptor;
            AZ_Assert(descriptor.m_device, "Device is null.");
            Base::Init(*descriptor.m_device);

            RHI::ResultCode result = BuildNativeDescriptorPool();
            RETURN_RESULT_IF_UNSUCCESSFUL(result);

            ReleaseQueue::Descriptor collectorDescriptor;
            collectorDescriptor.m_collectLatency = m_descriptor.m_collectLatency;
            collectorDescriptor.m_collectFunction = nullptr;
            m_collector.Init(collectorDescriptor);

            AZ::Name name = GetName();
            if (name.IsEmpty())
            {
                name = AZ::Name("DescriptorPool");
            }
            SetName(name);
            return result;
        }

        void DescriptorPool::SetNameInternal(const AZStd::string_view& name)
        {
            if (IsInitialized() && !name.empty())
            {
                Debug::SetNameToObject(reinterpret_cast<uint64_t>(m_nativeDescriptorPool), name.data(), VK_OBJECT_TYPE_DESCRIPTOR_POOL, static_cast<Device&>(GetDevice()));
            }
        }

        void DescriptorPool::Shutdown()
        {
            m_collector.Collect(true);
            if (m_nativeDescriptorPool != VK_NULL_HANDLE)
            {
                auto& device = static_cast<Device&>(GetDevice());
                device.GetContext().DestroyDescriptorPool(device.GetNativeDevice(), m_nativeDescriptorPool, VkSystemAllocator::Get());
                m_nativeDescriptorPool = VK_NULL_HANDLE;
            }
            Base::Shutdown();
        }

        void DescriptorPool::Reset()
        {
            if (m_nativeDescriptorPool != VK_NULL_HANDLE)
            {
                auto& device = static_cast<Device&>(GetDevice());
                device.GetContext().ResetDescriptorPool(device.GetNativeDevice(), m_nativeDescriptorPool, 0);
            }
        }

        RHI::ResultCode DescriptorPool::BuildNativeDescriptorPool()
        {
            AZ_Assert(m_descriptor.m_maxSets > 0, "Maximum number of descriptor sets is zero.");
            VkDescriptorPoolCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            createInfo.pNext = nullptr;
            createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            createInfo.maxSets = m_descriptor.m_maxSets;
            createInfo.poolSizeCount = static_cast<uint32_t>(m_descriptor.m_descriptorPoolSizes.size());
            createInfo.pPoolSizes = m_descriptor.m_descriptorPoolSizes.empty() ? nullptr : m_descriptor.m_descriptorPoolSizes.data();

            if (m_descriptor.m_updateAfterBind)
            {
                createInfo.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
            }

            auto& device = static_cast<Device&>(GetDevice());
            const VkResult result = device.GetContext().CreateDescriptorPool(
                device.GetNativeDevice(), &createInfo, VkSystemAllocator::Get(), &m_nativeDescriptorPool);
            AssertSuccess(result);

            return ConvertResult(result);
        }

        void DescriptorPool::Collect()
        {
            m_collector.Collect();
        }

        size_t DescriptorPool::GetTotalObjectCount() const
        {
            return m_objects.size() + m_collector.GetObjectCount();
        }

        VkDescriptorPool DescriptorPool::GetNativeDescriptorPool() const
        {
            return m_nativeDescriptorPool;
        }

#if defined(CARBONATED)
        AZStd::mutex& DescriptorPool::GetMutex() const
        {
            return m_mutex;
        }

        DescriptorPool::BatchAllocResult DescriptorPool::AllocateBatch(
            const DescriptorSetLayout& descriptorSetLayout,
            uint32_t count)
        {
            AZ_Assert(
                count > 0 && count <= RHI::Limits::Device::FrameCountMax,
                "Invalid descriptor-set batch size.");
            if (count == 0 || count > RHI::Limits::Device::FrameCountMax)
            {
                return { VK_ERROR_INITIALIZATION_FAILED, {} };
            }

            auto& device = static_cast<Device&>(GetDevice());

            RHI::Ptr<Buffer> constantDataBuffer;
            const size_t constantDataSize = descriptorSetLayout.GetConstantDataSize();
            size_t constantDataStride = 0;
            if (m_descriptor.m_constantDataPool && constantDataSize)
            {
                constantDataStride =
                    RHI::AlignUp(constantDataSize, aznumeric_cast<size_t>(device.GetLimits().m_minConstantBufferViewOffset));
                constantDataBuffer = Buffer::Create();
                const RHI::BufferDescriptor bufferDescriptor(
                    RHI::BufferBindFlags::Constant,
                    constantDataStride * count);
                RHI::BufferInitRequest request(*constantDataBuffer, bufferDescriptor);
                if (m_descriptor.m_constantDataPool->InitBuffer(request) != RHI::ResultCode::Success)
                {
                    return { VK_ERROR_OUT_OF_HOST_MEMORY, {} };
                }
            }

            AZStd::fixed_vector<VkDescriptorSet, RHI::Limits::Device::FrameCountMax> nativeDescriptorSets;
            nativeDescriptorSets.resize(count, VK_NULL_HANDLE);

            // Unbounded descriptor sets still have to defer native allocation until their variable
            // descriptor count is known. Their wrappers and constant data are nevertheless batched.
            if (!descriptorSetLayout.GetHasUnboundedArray())
            {
                AZStd::fixed_vector<VkDescriptorSetLayout, RHI::Limits::Device::FrameCountMax> nativeLayouts;
                nativeLayouts.resize(count, descriptorSetLayout.GetNativeDescriptorSetLayout());

                VkDescriptorSetAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                allocInfo.descriptorPool = m_nativeDescriptorPool;
                allocInfo.descriptorSetCount = count;
                allocInfo.pSetLayouts = nativeLayouts.data();

                VkResult result = VK_SUCCESS;
                {
                    AZStd::lock_guard<AZStd::mutex> poolLock(GetMutex());
                    result = device.GetContext().AllocateDescriptorSets(
                        device.GetNativeDevice(),
                        &allocInfo,
                        nativeDescriptorSets.data());
                }
                if (result == VK_ERROR_FRAGMENTED_POOL)
                {
                    AZ_Warning(
                        "Vulkan RHI",
                        false,
                        "Fragmented pool while allocating descriptor-set batch; the allocator will retry with another pool.");
                }
                else
                {
                    AssertSuccess(result);
                }

                if (result != VK_SUCCESS)
                {
                    return { result, {} };
                }
            }

            DescriptorSetList descriptorSets;
            for (uint32_t descriptorSetIndex = 0; descriptorSetIndex < count; ++descriptorSetIndex)
            {
                RHI::Ptr<DescriptorSet> descriptorSet = DescriptorSet::Create();
                DescriptorSet::Descriptor descriptor;
                descriptor.m_device = &device;
                descriptor.m_descriptorPool = this;
                descriptor.m_descriptorSetLayout = &descriptorSetLayout;
                const VkResult result = descriptorSet->Init(
                    descriptor,
                    nativeDescriptorSets[descriptorSetIndex],
                    constantDataBuffer,
                    constantDataStride * descriptorSetIndex);
                AZ_Assert(result == VK_SUCCESS, "Failed to initialize a descriptor set from a successfully allocated batch.");
                if (result != VK_SUCCESS)
                {
                    for (RHI::Ptr<DescriptorSet>& initializedDescriptorSet : descriptorSets)
                    {
                        initializedDescriptorSet->Shutdown();
                    }

                    const uint32_t remainingSetCount = count - descriptorSetIndex;
                    if (nativeDescriptorSets[descriptorSetIndex] != VK_NULL_HANDLE)
                    {
                        AZStd::lock_guard<AZStd::mutex> poolLock(GetMutex());
                        AssertSuccess(device.GetContext().FreeDescriptorSets(
                            device.GetNativeDevice(),
                            m_nativeDescriptorPool,
                            remainingSetCount,
                            nativeDescriptorSets.data() + descriptorSetIndex));
                    }
                    return { result, {} };
                }

                descriptorSets.emplace_back(AZStd::move(descriptorSet));
            }

            for (const RHI::Ptr<DescriptorSet>& descriptorSet : descriptorSets)
            {
                m_objects.insert(descriptorSet);
            }
            return { VK_SUCCESS, AZStd::move(descriptorSets) };
        }

#else
        DescriptorPool::AllocResult DescriptorPool::Allocate(const DescriptorSetLayout& descriptorSetLayout)
        {
            auto descriptorSets = DescriptorSet::Create();
            DescriptorSet::Descriptor descSetDesc;
            descSetDesc.m_device = static_cast<Device*>(&GetDevice());
            descSetDesc.m_descriptorPool = this;
            descSetDesc.m_descriptorSetLayout = &descriptorSetLayout;
            VkResult vkResult = descriptorSets->Init(descSetDesc);
            if (vkResult != VK_SUCCESS)
            {
                return AZStd::make_pair(vkResult, nullptr);
            }

            m_objects.insert(descriptorSets);
            return AZStd::make_pair(vkResult, descriptorSets);
        }

#endif

        void DescriptorPool::DeAllocate(RHI::Ptr<ObjectType> object)
        {
            m_collector.QueueForCollect(object);
            m_objects.erase(object);
        }

        const DescriptorPool::Descriptor& DescriptorPool::GetDescriptor() const
        {
            return m_descriptor;
        }
    }
}
