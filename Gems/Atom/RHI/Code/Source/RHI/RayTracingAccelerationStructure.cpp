/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RHI/RayTracingAccelerationStructure.h>
#include <Atom/RHI/Buffer.h>
#include <Atom/RHI/Factory.h>

namespace AZ::RHI
{
    RayTracingBlasDescriptor* RayTracingBlasDescriptor::Build()
    {
        return this;
    }

    RayTracingBlasDescriptor* RayTracingBlasDescriptor::Geometry()
    {
        m_geometries.emplace_back();
        m_buildContext = &m_geometries.back();
        return this;
    }

    RayTracingBlasDescriptor* RayTracingBlasDescriptor::AABB(const Aabb& aabb)
    {
        m_aabb = aabb;
        return this;
    }

    RayTracingBlasDescriptor* RayTracingBlasDescriptor::VertexBuffer(const RHI::StreamBufferView& vertexBuffer)
    {
        AZ_Assert(m_buildContext, "VertexBuffer property can only be added to a Geometry entry");
        m_buildContext->m_vertexBuffer = vertexBuffer;
        return this;
    }

    RayTracingBlasDescriptor* RayTracingBlasDescriptor::VertexFormat(RHI::Format vertexFormat)
    {
        AZ_Assert(m_buildContext, "VertexFormat property can only be added to a Geometry entry");
        m_buildContext->m_vertexFormat = vertexFormat;
        return this;
    }

    RayTracingBlasDescriptor* RayTracingBlasDescriptor::IndexBuffer(const RHI::IndexBufferView& indexBuffer)
    {
        AZ_Assert(m_buildContext, "IndexBuffer property can only be added to a Geometry entry");
        m_buildContext->m_indexBuffer = indexBuffer;
        return this;
    }

    RayTracingBlasDescriptor* RayTracingBlasDescriptor::BuildFlags(const RHI::RayTracingAccelerationStructureBuildFlags &buildFlags)
    {
        AZ_Assert(m_buildContext, "BuildFlags property can only be added to a Geometry entry");
        m_buildFlags = buildFlags;
        return this;
    }

    RayTracingTlasDescriptor* RayTracingTlasDescriptor::Build()
    {
        return this;
    }

    RayTracingTlasDescriptor* RayTracingTlasDescriptor::Instance()
    {
        AZ_Assert(m_instancesBuffer == nullptr, "Instance cannot be combined with an instances buffer");
        m_instances.emplace_back();
#if defined(CARBONATED)
        AssignNewVersion(m_instances.back());
#endif
        m_buildContext = &m_instances.back();
#if defined(CARBONATED)
        ++m_topologyRevision;
#endif
        return this;
    }

#if defined(CARBONATED)
    RayTracingTlasDescriptor* RayTracingTlasDescriptor::Instance(uint32_t instanceIndex)
    {
        AZ_Assert(instanceIndex < m_instances.size(), "TLAS instance index is out of range");
        m_buildContext = &m_instances[instanceIndex];
        AssignNewVersion(*m_buildContext);
        return this;
    }

    void RayTracingTlasDescriptor::AssignNewVersion(RayTracingTlasInstance& instance)
    {
        instance.m_version = m_nextInstanceVersion++;
    }

    RayTracingTlasDescriptor* RayTracingTlasDescriptor::InsertInstance(uint32_t instanceIndex)
    {
        AZ_Assert(m_instancesBuffer == nullptr, "Instance cannot be combined with an instances buffer");
        AZ_Assert(instanceIndex <= m_instances.size(), "TLAS instance insertion index is out of range");
        m_buildContext = &*m_instances.insert(m_instances.begin() + instanceIndex, RayTracingTlasInstance{});
        AssignNewVersion(*m_buildContext);
        ++m_topologyRevision;
        return this;
    }

    void RayTracingTlasDescriptor::RemoveInstance(uint32_t instanceIndex)
    {
        AZ_Assert(!m_instances.empty(), "Cannot remove an instance from an empty TLAS descriptor");
        RemoveInstance(instanceIndex, aznumeric_caster(m_instances.size() - 1));
    }

    void RayTracingTlasDescriptor::RemoveInstance(uint32_t instanceIndex, uint32_t replacementIndex)
    {
        AZ_Assert(instanceIndex < m_instances.size(), "TLAS instance index is out of range");
        AZ_Assert(replacementIndex < m_instances.size(), "TLAS replacement index is out of range");
        AZ_Assert(instanceIndex <= replacementIndex, "Replacement must not precede the removed instance");
        if (instanceIndex != replacementIndex)
        {
            m_instances[instanceIndex] = AZStd::move(m_instances[replacementIndex]);
            AssignNewVersion(m_instances[instanceIndex]);
        }
        m_instances.erase(m_instances.begin() + replacementIndex);
        m_buildContext = nullptr;
        ++m_topologyRevision;
    }

    bool RayTracingTlasDescriptor::CompactInstancesIfNeeded()
    {
        const uint32_t activeCount = aznumeric_caster(m_instances.size());
        const uint32_t capacity = GetInstanceCapacity();
        if (capacity == 0 || activeCount > capacity / 4)
        {
            m_underutilizedFrameCount = 0;
            return false;
        }

        if (++m_underutilizedFrameCount < CompactionDelayFrames)
        {
            return false;
        }

        m_instances.shrink_to_fit();
        m_buildContext = nullptr;
        m_underutilizedFrameCount = 0;
        if (GetInstanceCapacity() < capacity)
        {
            ++m_topologyRevision;
            return true;
        }
        return false;
    }
#endif

    RayTracingTlasDescriptor* RayTracingTlasDescriptor::InstanceID(uint32_t instanceID)
    {
        AZ_Assert(m_buildContext, "InstanceID property can only be added to an Instance entry");
        m_buildContext->m_instanceID = instanceID;
        return this;
    }

    RayTracingTlasDescriptor* RayTracingTlasDescriptor::InstanceMask(uint32_t instanceMask)
    {
        AZ_Assert(m_buildContext, "InstanceMask property can only be added to an Instance entry");
        m_buildContext->m_instanceMask = instanceMask;
        return this;
    }

    RayTracingTlasDescriptor* RayTracingTlasDescriptor::HitGroupIndex(uint32_t hitGroupIndex)
    {
        AZ_Assert(m_buildContext, "HitGroupIndex property can only be added to an Instance entry");
        m_buildContext->m_hitGroupIndex = hitGroupIndex;
        return this;
    }

    RayTracingTlasDescriptor* RayTracingTlasDescriptor::Transform(const AZ::Transform& transform)
    {
        AZ_Assert(m_buildContext, "Transform property can only be added to an Instance entry");
        m_buildContext->m_transform = transform;
        return this;
    }

    RayTracingTlasDescriptor* RayTracingTlasDescriptor::NonUniformScale(const AZ::Vector3& nonUniformScale)
    {
        AZ_Assert(m_buildContext, "NonUniformSCale property can only be added to an Instance entry");
        m_buildContext->m_nonUniformScale = nonUniformScale;
        return this;
    }

    RayTracingTlasDescriptor* RayTracingTlasDescriptor::Transparent(bool transparent)
    {
        AZ_Assert(m_buildContext, "Transparent property can only be added to a Geometry entry");
        m_buildContext->m_transparent = transparent;
        return this;
    }

    RayTracingTlasDescriptor* RayTracingTlasDescriptor::Blas(const RHI::Ptr<RHI::RayTracingBlas>& blas)
    {
        AZ_Assert(m_buildContext, "Blas property can only be added to an Instance entry");
        m_buildContext->m_blas = blas;
        return this;
    }

    RayTracingTlasDescriptor* RayTracingTlasDescriptor::InstancesBuffer(const RHI::Ptr<RHI::Buffer>& instancesBuffer)
    {
        AZ_Assert(!m_buildContext, "InstancesBuffer property can only be added to the top level");
        AZ_Assert(m_instances.size() == 0, "InstancesBuffer cannot exist with instance entries");
        m_instancesBuffer = instancesBuffer;
        return this;
    }

    RayTracingTlasDescriptor* RayTracingTlasDescriptor::NumInstances(uint32_t numInstancesInBuffer)
    {
        AZ_Assert(m_instancesBuffer.get(), "NumInstances property can only be added to the InstancesBuffer entry");
        m_numInstancesInBuffer = numInstancesInBuffer;
        return this;
    }

    RHI::Ptr<RHI::RayTracingBlas> RayTracingBlas::CreateRHIRayTracingBlas()
    {
        RHI::Ptr<RHI::RayTracingBlas> rayTracingBlas = RHI::Factory::Get().CreateRayTracingBlas();
        AZ_Error("RayTracingBlas", rayTracingBlas.get(), "Failed to create RHI::RayTracingBlas");
        return rayTracingBlas;
    }

    ResultCode RayTracingBlas::CreateBuffers(Device& device, const RayTracingBlasDescriptor* descriptor, const RayTracingBufferPools& rayTracingBufferPools)
    {
        ResultCode resultCode = CreateBuffersInternal(device, descriptor, rayTracingBufferPools);
        if (resultCode == ResultCode::Success)
        {
            DeviceObject::Init(device);
            m_geometries = descriptor->GetGeometries();
        }
        return resultCode;
    }

    RHI::Ptr<RHI::RayTracingTlas> RayTracingTlas::CreateRHIRayTracingTlas()
    {
        RHI::Ptr<RHI::RayTracingTlas> rayTracingTlas = RHI::Factory::Get().CreateRayTracingTlas();
        AZ_Error("RayTracingTlas", rayTracingTlas.get(), "Failed to create RHI::RayTracingTlas");
        return rayTracingTlas;
    }

    ResultCode RayTracingTlas::CreateBuffers(Device& device, const RayTracingTlasDescriptor* descriptor, const RayTracingBufferPools& rayTracingBufferPools)
    {
        ResultCode resultCode = CreateBuffersInternal(device, descriptor, rayTracingBufferPools);
        if (resultCode == ResultCode::Success)
        {
            DeviceObject::Init(device);
        }
        return resultCode;
    }
}
