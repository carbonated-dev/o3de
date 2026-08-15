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
#include <RHI/Conversions.h>
#include <RHI/Device.h>
#include <Atom/RHI/Factory.h>
#include <Atom/RHI/BufferPool.h>
#include <Atom/RHI/RayTracingBufferPools.h>

namespace AZ
{
    namespace DX12
    {
        RHI::Ptr<RayTracingTlas> RayTracingTlas::Create()
        {
            return aznew RayTracingTlas;
        }

        RHI::ResultCode RayTracingTlas::CreateBuffersInternal([[maybe_unused]] RHI::Device& deviceBase, [[maybe_unused]] const RHI::RayTracingTlasDescriptor* descriptor, [[maybe_unused]] const RHI::RayTracingBufferPools& bufferPools)
        {
#ifdef AZ_DX12_DXR_SUPPORT
#if defined(CARBONATED)
            Device& device = static_cast<Device&>(deviceBase);
            ID3D12DeviceX* dx12Device = device.GetDevice();

            // advance to the next buffer
            TlasBuffers& buffers = m_buffers.AdvanceCurrentElement();

            const RHI::RayTracingTlasInstanceVector& instances = descriptor->GetInstances();
            const bool usesExternalInstances = descriptor->GetInstancesBuffer() != nullptr;
            const uint32_t numInstances = usesExternalInstances
                ? descriptor->GetNumInstancesInBuffer()
                : aznumeric_caster(instances.size());
            if (numInstances == 0)
            {
                // no instances in the scene, clear the TLAS buffers
                buffers.m_tlasBuffer = nullptr;
                buffers.m_tlasInstancesBuffer = nullptr;
                buffers.m_scratchBuffer = nullptr;
                buffers.m_uploadedInstanceVersions.clear();
                buffers.m_instanceCapacity = 0;
                buffers.m_instanceCount = 0;
                buffers.m_buildMode = RHI::RayTracingTlasBuildMode::None;
                buffers.m_hasBeenBuilt = false;
                return RHI::ResultCode::Success;
            }

            const uint32_t requestedCapacity = AZStd::max(
                numInstances,
                usesExternalInstances ? numInstances : descriptor->GetInstanceCapacity());
            const bool capacityChanged =
                buffers.m_tlasBuffer == nullptr || buffers.m_instanceCapacity != requestedCapacity;

            D3D12_GPU_VIRTUAL_ADDRESS tlasInstancesGpuAddress = 0;
            if (!usesExternalInstances)
            {
                const uint64_t instanceDescsSizeInBytes = RHI::AlignUp(
                    aznumeric_cast<UINT64>(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * requestedCapacity),
                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

                if (capacityChanged || buffers.m_tlasInstancesBuffer == nullptr)
                {
                    buffers.m_tlasInstancesBuffer = RHI::Factory::Get().CreateBuffer();
                    AZ::RHI::BufferDescriptor bufferDescriptor;
                    bufferDescriptor.m_bindFlags = RHI::BufferBindFlags::ShaderReadWrite;
                    bufferDescriptor.m_byteCount = instanceDescsSizeInBytes;
                    bufferDescriptor.m_alignment = D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT;

                    AZ::RHI::BufferInitRequest request;
                    request.m_buffer = buffers.m_tlasInstancesBuffer.get();
                    request.m_descriptor = bufferDescriptor;
                    const RHI::ResultCode resultCode = bufferPools.GetTlasInstancesBufferPool()->InitBuffer(request);
                    AZ_Assert(resultCode == RHI::ResultCode::Success, "failed to create TLAS instances buffer");
                    buffers.m_uploadedInstanceVersions.assign(requestedCapacity, 0);
                }

                MemoryView& tlasInstancesMemoryView = static_cast<Buffer*>(buffers.m_tlasInstancesBuffer.get())->GetMemoryView();
                tlasInstancesMemoryView.SetName(L"TLAS Instance");

                AZStd::vector<uint32_t> staleInstances;
                staleInstances.reserve(numInstances);
                uint32_t staleRangeCount = 0;
                for (uint32_t instanceIndex = 0; instanceIndex < numInstances; ++instanceIndex)
                {
                    if (capacityChanged || buffers.m_uploadedInstanceVersions[instanceIndex] != instances[instanceIndex].m_version)
                    {
                        if (staleInstances.empty() || instanceIndex != staleInstances.back() + 1)
                        {
                            ++staleRangeCount;
                        }
                        staleInstances.push_back(instanceIndex);
                    }
                }

                if (staleRangeCount > MaxTlasInstanceUploadRanges)
                {
                    // Too many small staging uploads are more expensive than regenerating the active descriptor buffer.
                    staleInstances.resize(numInstances);
                    for (uint32_t instanceIndex = 0; instanceIndex < numInstances; ++instanceIndex)
                    {
                        staleInstances[instanceIndex] = instanceIndex;
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
                        const uint64_t byteOffset = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * firstInstance;
                        const uint64_t byteCount = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instanceCount;
                        RHI::BufferMapResponse mapResponse;
                        const RHI::ResultCode resultCode = bufferPools.GetTlasInstancesBufferPool()->MapBuffer(
                            RHI::BufferMapRequest(*buffers.m_tlasInstancesBuffer, byteOffset, byteCount), mapResponse);
                        AZ_Assert(resultCode == RHI::ResultCode::Success, "failed to map TLAS instances buffer");
                        auto* mappedData = reinterpret_cast<D3D12_RAYTRACING_INSTANCE_DESC*>(mapResponse.m_data);

                        for (size_t staleIndex = staleOffset; staleIndex < rangeEnd; ++staleIndex)
                        {
                            const uint32_t instanceIndex = staleInstances[staleIndex];
                            const RHI::RayTracingTlasInstance& instance = instances[instanceIndex];
                            RayTracingBlas* blas = static_cast<RayTracingBlas*>(instance.m_blas.get());
                            D3D12_RAYTRACING_INSTANCE_DESC& nativeInstance = mappedData[instanceIndex - firstInstance];

                            nativeInstance.InstanceID = instance.m_instanceID;
                            nativeInstance.InstanceContributionToHitGroupIndex = instance.m_hitGroupIndex;
                            AZ::Matrix3x4 matrix3x4 = AZ::Matrix3x4::CreateFromTransform(instance.m_transform);
                            matrix3x4.MultiplyByScale(instance.m_nonUniformScale);
                            matrix3x4.StoreToRowMajorFloat12(&nativeInstance.Transform[0][0]);
                            nativeInstance.AccelerationStructure =
                                static_cast<DX12::Buffer*>(blas->GetBuffers().m_blasBuffer.get())->GetMemoryView().GetGpuAddress();
                            nativeInstance.InstanceMask = instance.m_instanceMask;
                            nativeInstance.Flags = instance.m_transparent
                                ? D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_NON_OPAQUE
                                : D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
                            buffers.m_uploadedInstanceVersions[instanceIndex] = instance.m_version;
                        }

                        bufferPools.GetTlasInstancesBufferPool()->UnmapBuffer(*buffers.m_tlasInstancesBuffer);
                        staleOffset = rangeEnd;
                    }
                }
                tlasInstancesGpuAddress = tlasInstancesMemoryView.GetGpuAddress();
            }
            else
            {
                tlasInstancesGpuAddress = static_cast<DX12::Buffer*>(descriptor->GetInstancesBuffer().get())->GetMemoryView().GetGpuAddress();
                buffers.m_tlasInstancesBuffer = descriptor->GetInstancesBuffer();
            }

            ZeroMemory(&m_inputs, sizeof(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS));
            m_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
            m_inputs.InstanceDescs = tlasInstancesGpuAddress;
            m_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            m_inputs.NumDescs = static_cast<UINT>(numInstances);
            m_inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE |
                D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;

            if (capacityChanged)
            {
                D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS capacityInputs = m_inputs;
                capacityInputs.NumDescs = requestedCapacity;
                D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
                dx12Device->GetRaytracingAccelerationStructurePrebuildInfo(&capacityInputs, &prebuildInfo);

                const uint64_t scratchSize = RHI::AlignUp(
                    AZStd::max(prebuildInfo.ScratchDataSizeInBytes, prebuildInfo.UpdateScratchDataSizeInBytes),
                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
                const uint64_t tlasSize = RHI::AlignUp(
                    prebuildInfo.ResultDataMaxSizeInBytes,
                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

                buffers.m_scratchBuffer = RHI::Factory::Get().CreateBuffer();
                AZ::RHI::BufferDescriptor scratchDescriptor;
                scratchDescriptor.m_bindFlags = RHI::BufferBindFlags::ShaderReadWrite | RHI::BufferBindFlags::RayTracingScratchBuffer;
                scratchDescriptor.m_byteCount = scratchSize;
                AZ::RHI::BufferInitRequest scratchRequest;
                scratchRequest.m_buffer = buffers.m_scratchBuffer.get();
                scratchRequest.m_descriptor = scratchDescriptor;
                RHI::ResultCode resultCode = bufferPools.GetScratchBufferPool()->InitBuffer(scratchRequest);
                AZ_Assert(resultCode == RHI::ResultCode::Success, "failed to create TLAS scratch buffer");
                static_cast<Buffer*>(buffers.m_scratchBuffer.get())->GetMemoryView().SetName(L"TLAS Scratch");

                buffers.m_tlasBuffer = RHI::Factory::Get().CreateBuffer();
                AZ::RHI::BufferDescriptor tlasDescriptor;
                tlasDescriptor.m_bindFlags = RHI::BufferBindFlags::RayTracingAccelerationStructure;
                tlasDescriptor.m_byteCount = tlasSize;
                AZ::RHI::BufferInitRequest tlasRequest;
                tlasRequest.m_buffer = buffers.m_tlasBuffer.get();
                tlasRequest.m_descriptor = tlasDescriptor;
                resultCode = bufferPools.GetTlasBufferPool()->InitBuffer(tlasRequest);
                AZ_Assert(resultCode == RHI::ResultCode::Success, "failed to create TLAS buffer");
                static_cast<Buffer*>(buffers.m_tlasBuffer.get())->GetMemoryView().SetName(L"TLAS");
                buffers.m_hasBeenBuilt = false;
            }

            const bool topologyChanged = buffers.m_topologyRevision != descriptor->GetTopologyRevision() ||
                buffers.m_instanceCount != numInstances;
            buffers.m_buildMode = (!buffers.m_hasBeenBuilt || capacityChanged || topologyChanged)
                ? RHI::RayTracingTlasBuildMode::Build
                : RHI::RayTracingTlasBuildMode::Update;
            buffers.m_instanceCapacity = requestedCapacity;
            buffers.m_instanceCount = numInstances;
            buffers.m_topologyRevision = descriptor->GetTopologyRevision();
            buffers.m_hasBeenBuilt = true;
#else
            Device& device = static_cast<Device&>(deviceBase);
            ID3D12DeviceX* dx12Device = device.GetDevice();

            // advance to the next buffer
            TlasBuffers& buffers = m_buffers.AdvanceCurrentElement();

            const RHI::RayTracingTlasInstanceVector& instances = descriptor->GetInstances();
            if (instances.empty())
            {
                // no instances in the scene, clear the TLAS buffers
                buffers.m_tlasBuffer = nullptr;
                buffers.m_tlasInstancesBuffer = nullptr;
                buffers.m_scratchBuffer = nullptr;
                return RHI::ResultCode::Success;
            }
            
            D3D12_GPU_VIRTUAL_ADDRESS tlasInstancesGpuAddress = 0;
            uint32_t numInstances = 0;
            if (descriptor->GetInstancesBuffer() == nullptr)
            {
                numInstances = aznumeric_caster(instances.size());
                uint64_t instanceDescsSizeInBytes = RHI::AlignUp(aznumeric_cast<UINT64>(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instances.size()), D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
            
                // create instances buffer
                buffers.m_tlasInstancesBuffer = RHI::Factory::Get().CreateBuffer();
                AZ::RHI::BufferDescriptor tlasInstancesBufferDescriptor;
                tlasInstancesBufferDescriptor.m_bindFlags = RHI::BufferBindFlags::ShaderReadWrite;
                tlasInstancesBufferDescriptor.m_byteCount = instanceDescsSizeInBytes;
                tlasInstancesBufferDescriptor.m_alignment = D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT;
            
                AZ::RHI::BufferInitRequest tlasInstancesBufferRequest;
                tlasInstancesBufferRequest.m_buffer = buffers.m_tlasInstancesBuffer.get();
                tlasInstancesBufferRequest.m_descriptor = tlasInstancesBufferDescriptor;
                [[maybe_unused]] RHI::ResultCode resultCode = bufferPools.GetTlasInstancesBufferPool()->InitBuffer(tlasInstancesBufferRequest);
                AZ_Assert(resultCode == RHI::ResultCode::Success, "failed to create TLAS instances buffer");
            
                MemoryView& tlasInstancesMemoryView = static_cast<Buffer*>(buffers.m_tlasInstancesBuffer.get())->GetMemoryView();
                tlasInstancesMemoryView.SetName(L"TLAS Instance");
            
                RHI::BufferMapResponse mapResponse;
                resultCode = bufferPools.GetTlasInstancesBufferPool()->MapBuffer(RHI::BufferMapRequest(*buffers.m_tlasInstancesBuffer, 0, instanceDescsSizeInBytes), mapResponse);
                AZ_Assert(resultCode == RHI::ResultCode::Success, "failed to map TLAS instances buffer");
                D3D12_RAYTRACING_INSTANCE_DESC* mappedData = reinterpret_cast<D3D12_RAYTRACING_INSTANCE_DESC*>(mapResponse.m_data);
            
                ZeroMemory(mappedData, instanceDescsSizeInBytes);
            
                // create each D3D12_RAYTRACING_INSTANCE_DESC structure
                for (uint32_t i = 0; i < instances.size(); ++i)
                {
                    const RHI::RayTracingTlasInstance& instance = instances[i];
                    RayTracingBlas* blas = static_cast<RayTracingBlas*>(instance.m_blas.get());
            
                    mappedData[i].InstanceID = instance.m_instanceID;
                    mappedData[i].InstanceContributionToHitGroupIndex = instance.m_hitGroupIndex;
                    // convert transform to row-major 3x4
                    AZ::Matrix3x4 matrix3x4 = AZ::Matrix3x4::CreateFromTransform(instance.m_transform);
                    matrix3x4.MultiplyByScale(instance.m_nonUniformScale);
                    matrix3x4.StoreToRowMajorFloat12(&mappedData[i].Transform[0][0]);
                    mappedData[i].AccelerationStructure = static_cast<DX12::Buffer*>(blas->GetBuffers().m_blasBuffer.get())->GetMemoryView().GetGpuAddress();
                    mappedData[i].InstanceMask = instance.m_instanceMask;
                    mappedData[i].Flags = instance.m_transparent ? D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_NON_OPAQUE : D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
                }
            
                bufferPools.GetTlasInstancesBufferPool()->UnmapBuffer(*buffers.m_tlasInstancesBuffer);
                tlasInstancesGpuAddress = tlasInstancesMemoryView.GetGpuAddress();
            }
            else
            {
                AZ_Assert(descriptor->GetNumInstancesInBuffer(), "TLAS InstancesBuffer set but instances count is zero");
                tlasInstancesGpuAddress = static_cast<DX12::Buffer*>(descriptor->GetInstancesBuffer().get())->GetMemoryView().GetGpuAddress();
                buffers.m_tlasInstancesBuffer = descriptor->GetInstancesBuffer();
                numInstances = descriptor->GetNumInstancesInBuffer();
            }
            
            // retrieve the required sizes for the scratch and TLAS buffers
            ZeroMemory(&m_inputs, sizeof(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS));
            m_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
            m_inputs.InstanceDescs = tlasInstancesGpuAddress;
            m_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            m_inputs.NumDescs = static_cast<UINT>(numInstances);
            m_inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
            
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
            dx12Device->GetRaytracingAccelerationStructurePrebuildInfo(&m_inputs, &prebuildInfo);
            
            prebuildInfo.ScratchDataSizeInBytes = RHI::AlignUp(prebuildInfo.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
            prebuildInfo.ResultDataMaxSizeInBytes = RHI::AlignUp(prebuildInfo.ResultDataMaxSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
            
            // create scratch buffer
            buffers.m_scratchBuffer = RHI::Factory::Get().CreateBuffer();
            AZ::RHI::BufferDescriptor scratchBufferDescriptor;
            scratchBufferDescriptor.m_bindFlags = RHI::BufferBindFlags::ShaderReadWrite | RHI::BufferBindFlags::RayTracingScratchBuffer;
            scratchBufferDescriptor.m_byteCount = prebuildInfo.ScratchDataSizeInBytes;
            
            AZ::RHI::BufferInitRequest scratchBufferRequest;
            scratchBufferRequest.m_buffer = buffers.m_scratchBuffer.get();
            scratchBufferRequest.m_descriptor = scratchBufferDescriptor;
            [[maybe_unused]] RHI::ResultCode resultCode = bufferPools.GetScratchBufferPool()->InitBuffer(scratchBufferRequest);
            AZ_Assert(resultCode == RHI::ResultCode::Success, "failed to create TLAS scratch buffer");
            
            MemoryView& scratchMemoryView = static_cast<Buffer*>(buffers.m_scratchBuffer.get())->GetMemoryView();
            scratchMemoryView.SetName(L"TLAS Scratch");
            
            // create TLAS buffer
            buffers.m_tlasBuffer = RHI::Factory::Get().CreateBuffer();
            AZ::RHI::BufferDescriptor tlasBufferDescriptor;
            tlasBufferDescriptor.m_bindFlags = RHI::BufferBindFlags::RayTracingAccelerationStructure;
            tlasBufferDescriptor.m_byteCount = prebuildInfo.ResultDataMaxSizeInBytes;
            
            AZ::RHI::BufferInitRequest tlasBufferRequest;
            tlasBufferRequest.m_buffer = buffers.m_tlasBuffer.get();
            tlasBufferRequest.m_descriptor = tlasBufferDescriptor;
            resultCode = bufferPools.GetTlasBufferPool()->InitBuffer(tlasBufferRequest);
            AZ_Assert(resultCode == RHI::ResultCode::Success, "failed to create TLAS buffer");
            
            MemoryView& tlasMemoryView = static_cast<Buffer*>(buffers.m_tlasBuffer.get())->GetMemoryView();
            tlasMemoryView.SetName(L"TLAS");
#endif // defined(CARBONATED)
#endif // AZ_DX12_DXR_SUPPORT
            return RHI::ResultCode::Success;
        }
    }
}
