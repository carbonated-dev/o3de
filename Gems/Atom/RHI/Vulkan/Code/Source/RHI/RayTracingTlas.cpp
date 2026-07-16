/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Math/Matrix3x4.h>
#include <RHI/RayTracingTlas.h>
#include <RHI/RayTracingBlas.h>
#include <RHI/Buffer.h>
#include <RHI/BufferView.h>
#include <RHI/Device.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/BufferPool.h>
#include <Atom/RHI/RayTracingBufferPools.h>
#include <Atom/RHI.Reflect/VkAllocator.h>

namespace AZ
{
    namespace Vulkan
    {
        RHI::Ptr<RayTracingTlas> RayTracingTlas::Create()
        {
            return aznew RayTracingTlas;
        }

        RHI::ResultCode RayTracingTlas::CreateBuffersInternal(RHI::Device& deviceBase, const RHI::RayTracingTlasDescriptor* descriptor, const RHI::RayTracingBufferPools& bufferPools)
        {
#if defined(CARBONATED)
            auto& device = static_cast<Device&>(deviceBase);
            auto& physicalDevice = static_cast<const PhysicalDevice&>(device.GetPhysicalDevice());
            const auto& accelerationStructureProperties = physicalDevice.GetPhysicalDeviceAccelerationStructureProperties();
            TlasBuffers& buffers = m_buffers.AdvanceCurrentElement();

            const auto& instances = descriptor->GetInstances();
            const bool usesExternalInstances = descriptor->GetInstancesBuffer() != nullptr;
            const uint32_t numInstances = usesExternalInstances
                ? descriptor->GetNumInstancesInBuffer()
                : aznumeric_caster(instances.size());

            if (numInstances == 0)
            {
                if (buffers.m_accelerationStructure)
                {
                    device.GetContext().DestroyAccelerationStructureKHR(
                        device.GetNativeDevice(), buffers.m_accelerationStructure, VkSystemAllocator::Get());
                }
                buffers = TlasBuffers{};
                return RHI::ResultCode::Success;
            }

            const uint32_t requestedCapacity = AZStd::max(
                numInstances,
                usesExternalInstances ? numInstances : descriptor->GetInstanceCapacity());
            const bool capacityChanged =
                buffers.m_tlasBuffer == nullptr || buffers.m_instanceCapacity != requestedCapacity;

            VkDeviceAddress instancesGpuAddress = 0;
            if (!usesExternalInstances)
            {
                const uint64_t byteCount = sizeof(VkAccelerationStructureInstanceKHR) * requestedCapacity;
                if (capacityChanged || buffers.m_tlasInstancesBuffer == nullptr)
                {
                    buffers.m_tlasInstancesBuffer = RHI::Factory::Get().CreateBuffer();
                    RHI::BufferDescriptor bufferDescriptor;
                    bufferDescriptor.m_bindFlags =
                        RHI::BufferBindFlags::ShaderReadWrite | RHI::BufferBindFlags::RayTracingAccelerationStructure;
                    bufferDescriptor.m_byteCount = byteCount;
                    RHI::BufferInitRequest request;
                    request.m_buffer = buffers.m_tlasInstancesBuffer.get();
                    request.m_descriptor = bufferDescriptor;
                    const RHI::ResultCode resultCode = bufferPools.GetTlasInstancesBufferPool()->InitBuffer(request);
                    AZ_Assert(resultCode == RHI::ResultCode::Success, "failed to create TLAS instances buffer");
                    buffers.m_uploadedInstanceVersions.assign(requestedCapacity, 0);
                }

                BufferMemoryView* memoryView = static_cast<Buffer*>(buffers.m_tlasInstancesBuffer.get())->GetBufferMemoryView();
                memoryView->SetName("TLAS Instance");

                AZStd::vector<uint32_t> staleInstances;
                staleInstances.reserve(numInstances);
                uint32_t staleRangeCount = 0;
                for (uint32_t index = 0; index < numInstances; ++index)
                {
                    if (capacityChanged || buffers.m_uploadedInstanceVersions[index] != instances[index].m_version)
                    {
                        if (staleInstances.empty() || index != staleInstances.back() + 1)
                        {
                            ++staleRangeCount;
                        }
                        staleInstances.push_back(index);
                    }
                }

                if (staleRangeCount > MaxTlasInstanceUploadRanges)
                {
                    // Too many small staging uploads are more expensive than regenerating the active descriptor buffer.
                    staleInstances.resize(numInstances);
                    for (uint32_t index = 0; index < numInstances; ++index)
                    {
                        staleInstances[index] = index;
                    }
                }

                if (!staleInstances.empty())
                {
                    AZ_PROFILE_SCOPE(RHI, "RayTracingTlas: update %zu of %u instances", staleInstances.size(), numInstances);
                    // Device-heap maps use a fresh staging allocation. Map only dirty ranges so bytes for
                    // unchanged descriptors are not replaced by uninitialized staging memory.
                    size_t staleOffset = 0;
                    while (staleOffset < staleInstances.size())
                    {
                        const uint32_t firstInstance = staleInstances[staleOffset];
                        size_t rangeEnd = staleOffset + 1;
                        while (rangeEnd < staleInstances.size() && staleInstances[rangeEnd] == staleInstances[rangeEnd - 1] + 1)
                        {
                            ++rangeEnd;
                        }

                        const uint32_t instanceCount = aznumeric_caster(rangeEnd - staleOffset);
                        const uint64_t rangeByteOffset = sizeof(VkAccelerationStructureInstanceKHR) * firstInstance;
                        const uint64_t rangeByteCount = sizeof(VkAccelerationStructureInstanceKHR) * instanceCount;
                        RHI::BufferMapResponse mapResponse;
                        const RHI::ResultCode resultCode = bufferPools.GetTlasInstancesBufferPool()->MapBuffer(
                            RHI::BufferMapRequest(*buffers.m_tlasInstancesBuffer, rangeByteOffset, rangeByteCount), mapResponse);
                        AZ_Assert(resultCode == RHI::ResultCode::Success, "failed to map TLAS instances buffer");
                        auto* mappedData = reinterpret_cast<VkAccelerationStructureInstanceKHR*>(mapResponse.m_data);

                        for (size_t staleIndex = staleOffset; staleIndex < rangeEnd; ++staleIndex)
                        {
                            const uint32_t index = staleInstances[staleIndex];
                            const RHI::RayTracingTlasInstance& instance = instances[index];
                            VkAccelerationStructureInstanceKHR& nativeInstance = mappedData[index - firstInstance];
                            nativeInstance.instanceCustomIndex = instance.m_instanceID;
                            nativeInstance.instanceShaderBindingTableRecordOffset = instance.m_hitGroupIndex;

                            AZ::Matrix3x4 matrix3x4 = AZ::Matrix3x4::CreateFromTransform(instance.m_transform);
                            matrix3x4.MultiplyByScale(instance.m_nonUniformScale);
                            matrix3x4.StoreToRowMajorFloat12(&nativeInstance.transform.matrix[0][0]);

                            RayTracingBlas* blas = static_cast<RayTracingBlas*>(instance.m_blas.get());
                            VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
                            addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
                            addressInfo.accelerationStructure = blas->GetBuffers().m_accelerationStructure;
                            nativeInstance.accelerationStructureReference =
                                device.GetContext().GetAccelerationStructureDeviceAddressKHR(device.GetNativeDevice(), &addressInfo);
                            nativeInstance.mask = instance.m_instanceMask;
                            nativeInstance.flags = instance.m_transparent ? VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR : 0;
                            buffers.m_uploadedInstanceVersions[index] = instance.m_version;
                        }
                        bufferPools.GetTlasInstancesBufferPool()->UnmapBuffer(*buffers.m_tlasInstancesBuffer);
                        staleOffset = rangeEnd;
                    }
                }

                VkBufferDeviceAddressInfo addressInfo = {};
                addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
                addressInfo.buffer = memoryView->GetNativeBuffer();
                instancesGpuAddress = device.GetContext().GetBufferDeviceAddress(device.GetNativeDevice(), &addressInfo);
            }
            else
            {
                buffers.m_tlasInstancesBuffer = descriptor->GetInstancesBuffer();
                VkBufferDeviceAddressInfo addressInfo = {};
                addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
                addressInfo.buffer = static_cast<Buffer*>(descriptor->GetInstancesBuffer().get())->GetBufferMemoryView()->GetNativeBuffer();
                instancesGpuAddress = device.GetContext().GetBufferDeviceAddress(device.GetNativeDevice(), &addressInfo);
            }

            buffers.m_geometry = {};
            buffers.m_geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            buffers.m_geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
            buffers.m_geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
            buffers.m_geometry.geometry.instances.arrayOfPointers = VK_FALSE;
            buffers.m_geometry.geometry.instances.data.deviceAddress = instancesGpuAddress;

            buffers.m_buildInfo = {};
            buffers.m_buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            buffers.m_buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
            buffers.m_buildInfo.geometryCount = 1;
            buffers.m_buildInfo.pGeometries = &buffers.m_geometry;
            buffers.m_buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            buffers.m_buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

            if (capacityChanged)
            {
                VkAccelerationStructureBuildSizesInfoKHR buildSizes = {};
                buildSizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
                device.GetContext().GetAccelerationStructureBuildSizesKHR(
                    device.GetNativeDevice(),
                    VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                    &buffers.m_buildInfo,
                    &requestedCapacity,
                    &buildSizes);

                buildSizes.accelerationStructureSize = RHI::AlignUp(buildSizes.accelerationStructureSize, 256);
                const uint64_t scratchSize = RHI::AlignUp(
                    AZStd::max(buildSizes.buildScratchSize, buildSizes.updateScratchSize),
                    accelerationStructureProperties.minAccelerationStructureScratchOffsetAlignment);

                buffers.m_scratchBuffer = RHI::Factory::Get().CreateBuffer();
                RHI::BufferDescriptor scratchDescriptor;
                scratchDescriptor.m_bindFlags = RHI::BufferBindFlags::ShaderReadWrite | RHI::BufferBindFlags::RayTracingScratchBuffer;
                scratchDescriptor.m_byteCount = scratchSize;
                scratchDescriptor.m_alignment = accelerationStructureProperties.minAccelerationStructureScratchOffsetAlignment;
                RHI::BufferInitRequest scratchRequest;
                scratchRequest.m_buffer = buffers.m_scratchBuffer.get();
                scratchRequest.m_descriptor = scratchDescriptor;
                RHI::ResultCode resultCode = bufferPools.GetScratchBufferPool()->InitBuffer(scratchRequest);
                AZ_Assert(resultCode == RHI::ResultCode::Success, "failed to create TLAS scratch buffer");
                auto* scratchMemoryView = static_cast<Buffer*>(buffers.m_scratchBuffer.get())->GetBufferMemoryView();
                scratchMemoryView->SetName("TLAS Scratch");

                if (buffers.m_accelerationStructure)
                {
                    device.GetContext().DestroyAccelerationStructureKHR(
                        device.GetNativeDevice(), buffers.m_accelerationStructure, VkSystemAllocator::Get());
                    buffers.m_accelerationStructure = VK_NULL_HANDLE;
                }

                buffers.m_tlasBuffer = RHI::Factory::Get().CreateBuffer();
                RHI::BufferDescriptor tlasDescriptor;
                tlasDescriptor.m_bindFlags = RHI::BufferBindFlags::RayTracingAccelerationStructure;
                tlasDescriptor.m_byteCount = buildSizes.accelerationStructureSize;
                RHI::BufferInitRequest tlasRequest;
                tlasRequest.m_buffer = buffers.m_tlasBuffer.get();
                tlasRequest.m_descriptor = tlasDescriptor;
                resultCode = bufferPools.GetTlasBufferPool()->InitBuffer(tlasRequest);
                AZ_Assert(resultCode == RHI::ResultCode::Success, "failed to create TLAS buffer");
                auto* tlasMemoryView = static_cast<Buffer*>(buffers.m_tlasBuffer.get())->GetBufferMemoryView();
                tlasMemoryView->SetName("TLAS");

                VkAccelerationStructureCreateInfoKHR createInfo = {};
                createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
                createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
                createInfo.size = buildSizes.accelerationStructureSize;
                createInfo.buffer = tlasMemoryView->GetNativeBuffer();
                const VkResult vkResult = device.GetContext().CreateAccelerationStructureKHR(
                    device.GetNativeDevice(), &createInfo, VkSystemAllocator::Get(), &buffers.m_accelerationStructure);
                AssertSuccess(vkResult);
                buffers.m_hasBeenBuilt = false;
            }

            buffers.m_buildInfo.dstAccelerationStructure = buffers.m_accelerationStructure;
            VkBufferDeviceAddressInfo scratchAddressInfo = {};
            scratchAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            scratchAddressInfo.buffer = static_cast<Buffer*>(buffers.m_scratchBuffer.get())->GetBufferMemoryView()->GetNativeBuffer();
            buffers.m_buildInfo.scratchData.deviceAddress =
                device.GetContext().GetBufferDeviceAddress(device.GetNativeDevice(), &scratchAddressInfo);

            const bool topologyChanged = buffers.m_topologyRevision != descriptor->GetTopologyRevision() ||
                buffers.m_instanceCount != numInstances;
            buffers.m_buildMode = (!buffers.m_hasBeenBuilt || capacityChanged || topologyChanged)
                ? RHI::RayTracingTlasBuildMode::Build
                : RHI::RayTracingTlasBuildMode::Update;
            buffers.m_instanceCapacity = requestedCapacity;
            buffers.m_instanceCount = numInstances;
            buffers.m_topologyRevision = descriptor->GetTopologyRevision();
            buffers.m_hasBeenBuilt = true;

            buffers.m_offsetInfo = {};
            buffers.m_offsetInfo.primitiveCount = numInstances;
            static_cast<Buffer*>(buffers.m_tlasBuffer.get())->SetNativeAccelerationStructure(buffers.m_accelerationStructure);
            return RHI::ResultCode::Success;
#else
            auto& device = static_cast<Device&>(deviceBase);
            auto& physicalDevice = static_cast<const PhysicalDevice&>(device.GetPhysicalDevice());
            const VkPhysicalDeviceAccelerationStructurePropertiesKHR& accelerationStructureProperties = physicalDevice.GetPhysicalDeviceAccelerationStructureProperties();
                        
            // advance to the next buffer
            TlasBuffers& buffers = m_buffers.AdvanceCurrentElement();

            if (buffers.m_accelerationStructure)
            {
                device.GetContext().DestroyAccelerationStructureKHR(
                    device.GetNativeDevice(), buffers.m_accelerationStructure, VkSystemAllocator::Get());
                buffers.m_accelerationStructure = nullptr;
            }

            const RHI::RayTracingTlasInstanceVector& instances = descriptor->GetInstances();
            if (instances.empty())
            {
                // no instances in the scene, clear the TLAS buffers
                buffers.m_tlasBuffer = nullptr;
                buffers.m_tlasInstancesBuffer = nullptr;
                buffers.m_scratchBuffer = nullptr;
                return RHI::ResultCode::Success;
            }
            
            VkDeviceAddress tlasInstancesGpuAddress = 0;
            if (descriptor->GetInstancesBuffer() == nullptr)
            {
                buffers.m_instanceCount = aznumeric_caster(instances.size());
                uint64_t instanceDescsSizeInBytes = aznumeric_cast<uint32_t>(sizeof(VkAccelerationStructureInstanceKHR) * instances.size());
            
                // create instances buffer
                buffers.m_tlasInstancesBuffer = RHI::Factory::Get().CreateBuffer();
                AZ::RHI::BufferDescriptor tlasInstancesBufferDescriptor;
                tlasInstancesBufferDescriptor.m_bindFlags = RHI::BufferBindFlags::ShaderReadWrite | RHI::BufferBindFlags::RayTracingAccelerationStructure;
                tlasInstancesBufferDescriptor.m_byteCount = instanceDescsSizeInBytes;
                
                AZ::RHI::BufferInitRequest tlasInstancesBufferRequest;
                tlasInstancesBufferRequest.m_buffer = buffers.m_tlasInstancesBuffer.get();
                tlasInstancesBufferRequest.m_descriptor = tlasInstancesBufferDescriptor;
                [[maybe_unused]] RHI::ResultCode resultCode = bufferPools.GetTlasInstancesBufferPool()->InitBuffer(tlasInstancesBufferRequest);
                AZ_Assert(resultCode == RHI::ResultCode::Success, "failed to create TLAS instances buffer");
                
                BufferMemoryView* tlasInstancesMemoryView = static_cast<Buffer*>(buffers.m_tlasInstancesBuffer.get())->GetBufferMemoryView();
                tlasInstancesMemoryView->SetName("TLAS Instance");
                
                RHI::BufferMapResponse mapResponse;
                resultCode = bufferPools.GetTlasInstancesBufferPool()->MapBuffer(RHI::BufferMapRequest(*buffers.m_tlasInstancesBuffer, 0, instanceDescsSizeInBytes), mapResponse);
                AZ_Assert(resultCode == RHI::ResultCode::Success, "failed to map TLAS instances buffer");
                VkAccelerationStructureInstanceKHR* mappedData = reinterpret_cast<VkAccelerationStructureInstanceKHR*>(mapResponse.m_data);

                memset(mappedData, 0, instanceDescsSizeInBytes);
            
                // create each VkAccelerationStructureInstanceKHR structure
                for (uint32_t i = 0; i < instances.size(); ++i)
                {
                    const RHI::RayTracingTlasInstance& instance = instances[i];
            
                    mappedData[i].instanceCustomIndex = instance.m_instanceID;
                    mappedData[i].instanceShaderBindingTableRecordOffset = instance.m_hitGroupIndex;
                    AZ::Matrix3x4 matrix3x4 = AZ::Matrix3x4::CreateFromTransform(instance.m_transform);
                    matrix3x4.MultiplyByScale(instance.m_nonUniformScale);
                    matrix3x4.StoreToRowMajorFloat12(&mappedData[i].transform.matrix[0][0]);
            
                    RayTracingBlas* blas = static_cast<RayTracingBlas*>(instance.m_blas.get());
                    VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
                    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
                    addressInfo.pNext = nullptr;
                    addressInfo.accelerationStructure = blas->GetBuffers().m_accelerationStructure;
                    mappedData[i].accelerationStructureReference =
                        device.GetContext().GetAccelerationStructureDeviceAddressKHR(device.GetNativeDevice(), &addressInfo);

                    mappedData[i].mask = instance.m_instanceMask;
                    mappedData[i].flags = instance.m_transparent ? VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR : 0;
                }
            
                bufferPools.GetTlasInstancesBufferPool()->UnmapBuffer(*buffers.m_tlasInstancesBuffer);
            
                VkBufferDeviceAddressInfo addressInfo = {};
                addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
                addressInfo.pNext = nullptr;
                addressInfo.buffer = tlasInstancesMemoryView->GetNativeBuffer();
                tlasInstancesGpuAddress = device.GetContext().GetBufferDeviceAddress(device.GetNativeDevice(), &addressInfo);
            }
            else
            {
                AZ_Assert(descriptor->GetNumInstancesInBuffer(), "TLAS InstancesBuffer set but instances count is zero");

                VkBufferDeviceAddressInfo addressInfo = {};
                addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
                addressInfo.pNext = nullptr;
                addressInfo.buffer = static_cast<Buffer*>(descriptor->GetInstancesBuffer().get())->GetBufferMemoryView()->GetNativeBuffer();
                tlasInstancesGpuAddress = device.GetContext().GetBufferDeviceAddress(device.GetNativeDevice(), &addressInfo);
                buffers.m_instanceCount = descriptor->GetNumInstancesInBuffer();
            }
            
            buffers.m_geometry = {};
            buffers.m_geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            buffers.m_geometry.pNext = nullptr;
            buffers.m_geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
            buffers.m_geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
            buffers.m_geometry.geometry.instances.arrayOfPointers = VK_FALSE;
            buffers.m_geometry.geometry.instances.data.deviceAddress = tlasInstancesGpuAddress;
            
            buffers.m_buildInfo = {};
            buffers.m_buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            buffers.m_buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            buffers.m_buildInfo.geometryCount = 1;
            buffers.m_buildInfo.pGeometries = &buffers.m_geometry;
            buffers.m_buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            buffers.m_buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            buffers.m_buildInfo.srcAccelerationStructure = nullptr;
            
            VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo = {};
            buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

            device.GetContext().GetAccelerationStructureBuildSizesKHR(
                device.GetNativeDevice(),
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &buffers.m_buildInfo,
                &buffers.m_instanceCount,
                &buildSizesInfo);

            buildSizesInfo.accelerationStructureSize = RHI::AlignUp(buildSizesInfo.accelerationStructureSize, 256);
            buildSizesInfo.buildScratchSize = RHI::AlignUp(buildSizesInfo.buildScratchSize, accelerationStructureProperties.minAccelerationStructureScratchOffsetAlignment);

            // create scratch buffer
            buffers.m_scratchBuffer = RHI::Factory::Get().CreateBuffer();
            AZ::RHI::BufferDescriptor scratchBufferDescriptor;
            scratchBufferDescriptor.m_bindFlags = RHI::BufferBindFlags::ShaderReadWrite | RHI::BufferBindFlags::RayTracingScratchBuffer;
            scratchBufferDescriptor.m_byteCount = buildSizesInfo.buildScratchSize;
            scratchBufferDescriptor.m_alignment = accelerationStructureProperties.minAccelerationStructureScratchOffsetAlignment;
            
            AZ::RHI::BufferInitRequest scratchBufferRequest;
            scratchBufferRequest.m_buffer = buffers.m_scratchBuffer.get();
            scratchBufferRequest.m_descriptor = scratchBufferDescriptor;
            [[maybe_unused]] RHI::ResultCode resultCode = bufferPools.GetScratchBufferPool()->InitBuffer(scratchBufferRequest);
            AZ_Assert(resultCode == RHI::ResultCode::Success, "failed to create TLAS scratch buffer");
            
            BufferMemoryView* scratchMemoryView = static_cast<Buffer*>(buffers.m_scratchBuffer.get())->GetBufferMemoryView();
            scratchMemoryView->SetName("TLAS Scratch");

            // create TLAS buffer
            buffers.m_tlasBuffer = RHI::Factory::Get().CreateBuffer();
            AZ::RHI::BufferDescriptor tlasBufferDescriptor;
            tlasBufferDescriptor.m_bindFlags = RHI::BufferBindFlags::RayTracingAccelerationStructure;
            tlasBufferDescriptor.m_byteCount = buildSizesInfo.accelerationStructureSize;
            
            AZ::RHI::BufferInitRequest tlasBufferRequest;
            tlasBufferRequest.m_buffer = buffers.m_tlasBuffer.get();
            tlasBufferRequest.m_descriptor = tlasBufferDescriptor;
            resultCode = bufferPools.GetTlasBufferPool()->InitBuffer(tlasBufferRequest);
            AZ_Assert(resultCode == RHI::ResultCode::Success, "failed to create TLAS buffer");
            
            BufferMemoryView* tlasMemoryView = static_cast<Buffer*>(buffers.m_tlasBuffer.get())->GetBufferMemoryView();
            tlasMemoryView->SetName("TLAS");

            // create acceleration structure
            VkAccelerationStructureCreateInfoKHR createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            createInfo.pNext = nullptr;
            createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            createInfo.size = buildSizesInfo.accelerationStructureSize;
            createInfo.offset = 0;
            createInfo.buffer = tlasMemoryView->GetNativeBuffer();

            VkResult vkResult = device.GetContext().CreateAccelerationStructureKHR(
                device.GetNativeDevice(), &createInfo, VkSystemAllocator::Get(), &buffers.m_accelerationStructure);
            AssertSuccess(vkResult);
            
            buffers.m_buildInfo.dstAccelerationStructure = buffers.m_accelerationStructure;
            
            VkBufferDeviceAddressInfo addressInfo = {};
            addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            addressInfo.pNext = nullptr;
            addressInfo.buffer = scratchMemoryView->GetNativeBuffer();
            buffers.m_buildInfo.scratchData.deviceAddress =
                device.GetContext().GetBufferDeviceAddress(device.GetNativeDevice(), &addressInfo);

            buffers.m_offsetInfo = {};
            buffers.m_offsetInfo.primitiveCount = buffers.m_instanceCount;

            // store the VkAccelerationStructureKHR in the TLAS Buffer, this is necessary since we need it to
            // setup the TLAS in the DescriptorSet when the Srg is compiled
            static_cast<Buffer*>(buffers.m_tlasBuffer.get())->SetNativeAccelerationStructure(buffers.m_accelerationStructure);

            return RHI::ResultCode::Success;
#endif
        }
    }
}
