/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RHI/PipelineStateDescriptor.h>
#include <Atom/RHI.Reflect/PipelineLayoutDescriptor.h>
#include <Atom/RHI.Reflect/ShaderStageFunction.h>
#include <Atom/RHI.Reflect/InputStreamLayout.h>

namespace AZ::RHI
{
    PipelineStateDescriptor::PipelineStateDescriptor(PipelineStateType pipelineStateType)
        : m_type{pipelineStateType}
    {}

    PipelineStateType PipelineStateDescriptor::GetType() const
    {
        return m_type;
    }

    HashValue64 PipelineStateDescriptor::GetHash() const
    {
        AZ_Assert(m_pipelineLayoutDescriptor, "Pipeline layout descriptor is null.");
        AZ::HashValue64 seed = AZ::HashValue64{ 0 };
        seed = TypeHash64(m_pipelineLayoutDescriptor->GetHash(), seed);
        if (HasSpecializationData())
        {
            seed = TypeHash64(GetSpecializationDataHash(), seed);
        }
        seed = TypeHash64(GetHashInternal(), seed);
        return seed;
    }

    bool PipelineStateDescriptor::operator==(const PipelineStateDescriptor& rhs) const
    {
        return m_type == rhs.m_type && SpecializationDataEquals(rhs);
    }

    void PipelineStateDescriptor::SetSpecializationData(SpecializationDataPtr specializationData)
    {
        m_specializationData.clear();
        m_specializationDataSource = AZStd::move(specializationData);
    }

    void PipelineStateDescriptor::ClearSpecializationData()
    {
        m_specializationData.clear();
        m_specializationDataSource.reset();
    }

    const AZStd::vector<SpecializationConstant>& PipelineStateDescriptor::GetSpecializationConstants() const
    {
        return m_specializationDataSource
            ? m_specializationDataSource->GetConstants()
            : m_specializationData;
    }

    bool PipelineStateDescriptor::SpecializationDataEquals(const PipelineStateDescriptor& rhs) const
    {
        if (m_specializationDataSource && rhs.m_specializationDataSource)
        {
            return m_specializationDataSource->IsEquivalent(*rhs.m_specializationDataSource);
        }

        if (!m_specializationDataSource && !rhs.m_specializationDataSource)
        {
            return m_specializationData == rhs.m_specializationData;
        }

        return GetSpecializationConstants() == rhs.GetSpecializationConstants();
    }

    bool PipelineStateDescriptor::HasSpecializationData() const
    {
        return m_specializationDataSource || !m_specializationData.empty();
    }

    HashValue64 PipelineStateDescriptor::GetSpecializationDataHash() const
    {
        if (m_specializationDataSource)
        {
            return m_specializationDataSource->GetConstantsHash();
        }

        HashValue64 seed = HashValue64{ 0 };
        for (const SpecializationConstant& constant : m_specializationData)
        {
            seed = TypeHash64(constant.GetHash(), seed);
        }
        return seed;
    }

    PipelineStateDescriptorForDraw::PipelineStateDescriptorForDraw()
        : PipelineStateDescriptor(PipelineStateType::Draw)
    {}

    PipelineStateDescriptorForDispatch::PipelineStateDescriptorForDispatch()
        : PipelineStateDescriptor(PipelineStateType::Dispatch)
    {}

    PipelineStateDescriptorForRayTracing::PipelineStateDescriptorForRayTracing()
        : PipelineStateDescriptor(PipelineStateType::RayTracing)
    {}

    AZ::HashValue64 PipelineStateDescriptorForDispatch::GetHashInternal() const
    {
        AZ_Assert(m_computeFunction, "Compute function is null.");

        AZ::HashValue64 seed = AZ::HashValue64{ 0 };
        seed = TypeHash64(m_computeFunction->GetHash(), seed);
        return seed;
    }

    AZ::HashValue64 PipelineStateDescriptorForDraw::GetHashInternal() const
    {
        AZ::HashValue64 seed = AZ::HashValue64{ 0 };

        if (m_vertexFunction)
        {
            seed = TypeHash64(m_vertexFunction->GetHash(), seed);
        }
        if (m_geometryFunction)
        {
            seed = TypeHash64(m_geometryFunction->GetHash(), seed);
        }
        if (m_fragmentFunction)
        {
            seed = TypeHash64(m_fragmentFunction->GetHash(), seed);
        }

        seed = TypeHash64(m_inputStreamLayout.GetHash(), seed);
        seed = TypeHash64(m_renderAttachmentConfiguration.GetHash(), seed);

        seed = m_renderStates.GetHash(seed);

        return seed;
    }

    AZ::HashValue64 PipelineStateDescriptorForRayTracing::GetHashInternal() const
    {
        AZ::HashValue64 seed = AZ::HashValue64{ 0 };
        seed = TypeHash64(m_rayTracingFunction->GetHash(), seed);
        return seed;
    }

    bool PipelineStateDescriptorForDraw::operator == (const PipelineStateDescriptorForDraw& rhs) const
    {
        return m_fragmentFunction == rhs.m_fragmentFunction && m_pipelineLayoutDescriptor == rhs.m_pipelineLayoutDescriptor &&
            m_renderStates == rhs.m_renderStates && m_vertexFunction == rhs.m_vertexFunction &&
            m_geometryFunction == rhs.m_geometryFunction && m_inputStreamLayout == rhs.m_inputStreamLayout && 
            m_renderAttachmentConfiguration == rhs.m_renderAttachmentConfiguration &&
            SpecializationDataEquals(rhs);
    }

    bool PipelineStateDescriptorForDispatch::operator == (const PipelineStateDescriptorForDispatch& rhs) const
    {
        return m_computeFunction == rhs.m_computeFunction &&
            m_pipelineLayoutDescriptor == rhs.m_pipelineLayoutDescriptor &&
            SpecializationDataEquals(rhs);
    }

    bool PipelineStateDescriptorForRayTracing::operator == (const PipelineStateDescriptorForRayTracing& rhs) const
    {
        return m_pipelineLayoutDescriptor == rhs.m_pipelineLayoutDescriptor &&
            m_rayTracingFunction == rhs.m_rayTracingFunction &&
            SpecializationDataEquals(rhs);
    }
}
