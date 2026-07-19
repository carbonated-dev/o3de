/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RPI.Public/Shader/ShaderResourceGroupPool.h>

#include <Atom/RHI/Factory.h>
#include <Atom/RHI/RHISystemInterface.h>

#include <AtomCore/Instance/InstanceDatabase.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <AzCore/Debug/Profiler.h>

namespace AZ
{
    namespace RPI
    {
        Data::Instance<ShaderResourceGroupPool> ShaderResourceGroupPool::FindOrCreate(
            const Data::Asset<ShaderAsset>& shaderAsset, const SupervariantIndex& supervariantIndex, const AZ::Name& srgName)
        {
            Data::InstanceId instanceId;
            {
                AZ_PROFILE_SCOPE(RPI, "RPI::ShaderResourceGroupPool::MakeInstanceId");
                instanceId = ShaderResourceGroup::MakeSrgPoolInstanceId(shaderAsset, supervariantIndex, srgName);
            }
            ShaderResourceGroup::SrgInitParams srgInitParams{ supervariantIndex, srgName };
            auto anyArgInitParams = AZStd::any(srgInitParams);
            AZ_PROFILE_SCOPE(
                RPI,
                "RPI::ShaderResourceGroupPool::InstanceDatabaseFindOrCreate Shader=%s Srg=%s",
                shaderAsset->GetName().GetCStr(),
                srgName.GetCStr());
            return Data::InstanceDatabase<ShaderResourceGroupPool>::Instance().FindOrCreate(instanceId,
                shaderAsset, &anyArgInitParams);
        }

        Data::Instance<ShaderResourceGroupPool> ShaderResourceGroupPool::CreateInternal(
            [[maybe_unused]] ShaderAsset& shaderAsset, const AZStd::any* anySrgInitParams)
        {
            AZ_Assert(anySrgInitParams, "Invalid SrgInitParams");
            auto srgInitParams = AZStd::any_cast<ShaderResourceGroup::SrgInitParams>(*anySrgInitParams);

            AZ_PROFILE_SCOPE(
                RPI,
                "RPI::ShaderResourceGroupPool::CreateInternal Shader=%s Srg=%s",
                shaderAsset.GetName().GetCStr(),
                srgInitParams.m_srgName.GetCStr());

            Data::Instance<ShaderResourceGroupPool> srgPool;
            {
                AZ_PROFILE_SCOPE(RPI, "RPI::ShaderResourceGroupPool::Allocate");
                srgPool = aznew ShaderResourceGroupPool();
            }

            RHI::ResultCode resultCode;
            {
                AZ_PROFILE_SCOPE(RPI, "RPI::ShaderResourceGroupPool::Init");
                resultCode = srgPool->Init(shaderAsset, srgInitParams.m_supervariantIndex, srgInitParams.m_srgName);
            }
            if (resultCode != RHI::ResultCode::Success)
            {
                return nullptr;
            }

            return srgPool;
        }

        RHI::ResultCode ShaderResourceGroupPool::Init(
            ShaderAsset& shaderAsset, const SupervariantIndex& supervariantIndex, const AZ::Name& srgName)
        {
            RHI::Ptr<RHI::Device> device = RHI::RHISystemInterface::Get()->GetDevice();

            {
                AZ_PROFILE_SCOPE(RPI, "RPI::ShaderResourceGroupPool::Init::CreateRhiPool");
                m_pool = RHI::Factory::Get().CreateShaderResourceGroupPool();
            }
            if (!m_pool)
            {
                AZ_Error("ShaderResourceGroupPool", false, "Failed to create RHI::ShaderResourceGroupPool");
                return RHI::ResultCode::Fail;
            }

            RHI::ShaderResourceGroupPoolDescriptor poolDescriptor;
            {
                AZ_PROFILE_SCOPE(RPI, "RPI::ShaderResourceGroupPool::Init::FindLayout");
                poolDescriptor.m_layout = shaderAsset.FindShaderResourceGroupLayout(srgName, supervariantIndex).get();
            }

            {
                AZ_PROFILE_SCOPE(RPI, "RPI::ShaderResourceGroupPool::Init::SetName");
                m_pool->SetName(AZ::Name(AZStd::string::format("%s_%s",shaderAsset.GetName().GetCStr(),srgName.GetCStr())));
            }
 
            AZ_PROFILE_SCOPE(
                RPI,
                "RPI::ShaderResourceGroupPool::Init::InitRhiPool Layout=%p",
                poolDescriptor.m_layout.get());
            const RHI::ResultCode resultCode = m_pool->Init(*device, poolDescriptor);
            return resultCode;
        }

        RHI::Ptr<RHI::ShaderResourceGroup> ShaderResourceGroupPool::CreateRHIShaderResourceGroup()
        {
            RHI::Ptr<RHI::ShaderResourceGroup> srg;
            {
                AZ_PROFILE_SCOPE(RPI, "RPI::ShaderResourceGroupPool::CreateRhiSrg::FactoryCreate");
                srg = RHI::Factory::Get().CreateShaderResourceGroup();
            }
            AZ_Error("ShaderResourceGroupPool", srg, "Failed to create RHI::ShaderResourceGroup");

            if (srg)
            {
                AZ_PROFILE_SCOPE(
                    RPI,
                    "RPI::ShaderResourceGroupPool::CreateRhiSrg::InitGroup Pool=%p",
                    m_pool.get());
                RHI::ResultCode result = m_pool->InitGroup(*srg);
                if (result != RHI::ResultCode::Success)
                {
                    srg.reset();
                    AZ_Error("ShaderResourceGroupPool", false, "Failed to initialize RHI::ShaderResourceGroup");
                }
            }

            return srg;
        }

        RHI::ShaderResourceGroupPool* ShaderResourceGroupPool::GetRHIPool()
        {
            return m_pool.get();
        }

        const RHI::ShaderResourceGroupPool* ShaderResourceGroupPool::GetRHIPool() const
        {
            return m_pool.get();
        }
    }
}
