/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RPI.Public/Shader/ShaderVariant.h>
#include <Atom/RPI.Public/Shader/ShaderReloadNotificationBus.h>
#include <Atom/RPI.Public/Shader/ShaderReloadDebugTracker.h>

#include <Atom/RHI/DrawListTagRegistry.h>
#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RHI.Reflect/ShaderStageFunction.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/parallel/shared_mutex.h>

namespace AZ
{
    namespace RPI
    {
        static_assert(
            ShaderVariantKeyBitCount <= 4 * sizeof(uint32_t) * 8,
            "RHI::SpecializationData::Key must have enough inline words for ShaderVariantKey.");

        struct ShaderVariant::SpecializationCache
        {
            struct OptionMetadata
            {
                ShaderOptionIndex m_optionIndex;
                Name m_name;
                uint32_t m_id = 0;
                RHI::SpecializationType m_type = RHI::SpecializationType::Invalid;
            };

            using MetadataList = AZStd::vector<OptionMetadata>;
            using MetadataListPtr = AZStd::shared_ptr<const MetadataList>;

            struct KeyHasher
            {
                size_t operator()(const RHI::SpecializationData::Key& key) const
                {
                    return AZStd::hash_range(key.begin(), key.end());
                }
            };

            static RHI::SpecializationConstant MakeConstant(
                const OptionMetadata& metadata,
                const ShaderOptionGroup& specialization)
            {
                RHI::SpecializationConstant constant;
                constant.m_name = metadata.m_name;
                constant.m_id = metadata.m_id;
                constant.m_value = RHI::SpecializationValue(
                    specialization.GetValue(metadata.m_optionIndex).GetIndex());
                constant.m_type = metadata.m_type;
                return constant;
            }

            ShaderVariantKey m_specializationMask;
            HashValue64 m_domainHash = HashValue64{ 0 };
            MetadataListPtr m_metadata;
            mutable AZStd::shared_mutex m_mutex;
            AZStd::unordered_map<
                RHI::SpecializationData::Key,
                RHI::SpecializationDataPtr,
                KeyHasher> m_payloads;
        };

        bool ShaderVariant::Init(
            const Data::Asset<ShaderAsset>& shaderAsset,
            const Data::Asset<ShaderVariantAsset>& shaderVariantAsset,
            SupervariantIndex supervariantIndex)
        {
            m_shaderAsset = shaderAsset;
            m_shaderVariantAsset = shaderVariantAsset;
            m_supervariantIndex = supervariantIndex;
            m_pipelineStateType = shaderAsset->GetPipelineStateType();
            m_pipelineLayoutDescriptor = shaderAsset->GetPipelineLayoutDescriptor(supervariantIndex);
            m_renderStates = &shaderAsset->GetRenderStates(supervariantIndex);
            m_useSpecializationConstants = shaderAsset->UseSpecializationConstants(supervariantIndex);

            m_specializationCache.reset();
            if (m_useSpecializationConstants)
            {
                m_specializationCache = AZStd::make_shared<SpecializationCache>();
                AZStd::shared_ptr<SpecializationCache::MetadataList> metadata =
                    AZStd::make_shared<SpecializationCache::MetadataList>();

                const ConstPtr<ShaderOptionGroupLayout>& optionLayout =
                    shaderAsset->GetShaderOptionGroupLayout();
                const AZStd::vector<ShaderOptionDescriptor>& shaderOptions =
                    optionLayout->GetShaderOptions();
                metadata->reserve(shaderOptions.size());

                HashValue64 domainHash = TypeHash64(optionLayout->GetHash());
                for (size_t optionIndex = 0; optionIndex < shaderOptions.size(); ++optionIndex)
                {
                    const ShaderOptionDescriptor& option = shaderOptions[optionIndex];
                    if (option.GetSpecializationId() < 0)
                    {
                        continue;
                    }

                    SpecializationCache::OptionMetadata optionMetadata;
                    optionMetadata.m_optionIndex = ShaderOptionIndex(static_cast<uint32_t>(optionIndex));
                    optionMetadata.m_name = option.GetName();
                    optionMetadata.m_id = static_cast<uint32_t>(option.GetSpecializationId());
                    switch (option.GetType())
                    {
                    case ShaderOptionType::Boolean:
                        optionMetadata.m_type = RHI::SpecializationType::Bool;
                        break;
                    case ShaderOptionType::Enumeration:
                    case ShaderOptionType::IntegerRange:
                        optionMetadata.m_type = RHI::SpecializationType::Integer;
                        break;
                    default:
                        AZ_Assert(false, "Unsupported specialization-constant shader option type.");
                        break;
                    }

                    domainHash = TypeHash64(optionMetadata.m_name.GetHash(), domainHash);
                    domainHash = TypeHash64(optionMetadata.m_id, domainHash);
                    domainHash = TypeHash64(optionMetadata.m_type, domainHash);
                    m_specializationCache->m_specializationMask |= option.GetBitMask();
                    metadata->emplace_back(AZStd::move(optionMetadata));
                }

                m_specializationCache->m_domainHash = domainHash;
                m_specializationCache->m_metadata = AZStd::move(metadata);
            }
            return true;
        }

        ShaderVariant::~ShaderVariant()
        {

        }

        void ShaderVariant::ConfigurePipelineState(
            RHI::PipelineStateDescriptor& descriptor,
            const ShaderVariantId& specialization) const
        {
            ConfigurePipelineState(descriptor, ShaderOptionGroup(m_shaderAsset->GetShaderOptionGroupLayout(), specialization));
        }

        void ShaderVariant::ConfigurePipelineState(
            RHI::PipelineStateDescriptor& descriptor,
            const ShaderOptionGroup& specialization) const
        {
            if (!m_useSpecializationConstants)
            {
                ConfigurePipelineStateBase(descriptor);
                return;
            }

            ShaderOptionGroup options = specialization;
            options.SetUnspecifiedToDefaultValues();
            ConfigurePipelineStateWithFullySpecifiedOptions(descriptor, options);
        }

        void ShaderVariant::ConfigurePipelineStateWithFullySpecifiedOptions(
            RHI::PipelineStateDescriptor& descriptor,
            const ShaderOptionGroup& specialization) const
        {
            AZ_Assert(
                specialization.GetShaderOptionLayout() == m_shaderAsset->GetShaderOptionGroupLayout(),
                "OptionGroup for specialization is different to the one in the ShaderAsset");
            AZ_Assert(
                !m_useSpecializationConstants || specialization.IsFullySpecified(),
                "The fast specialization path requires a fully specified ShaderOptionGroup.");

            ConfigurePipelineStateBase(descriptor);
            if (m_useSpecializationConstants)
            {
                descriptor.SetSpecializationData(GetOrCreateSpecializationData(specialization));
            }
        }

        void ShaderVariant::ConfigurePipelineStateBase(RHI::PipelineStateDescriptor& descriptor) const
        {
            descriptor.ClearSpecializationData();
            descriptor.m_pipelineLayoutDescriptor = m_pipelineLayoutDescriptor;

            switch (descriptor.GetType())
            {
            case RHI::PipelineStateType::Draw:
            {
                AZ_Assert(m_pipelineStateType == RHI::PipelineStateType::Draw, "ShaderVariant is not intended for the raster pipeline.");
                AZ_Assert(m_renderStates, "Invalid RenderStates");
                RHI::PipelineStateDescriptorForDraw& descriptorForDraw = static_cast<RHI::PipelineStateDescriptorForDraw&>(descriptor);
                descriptorForDraw.m_vertexFunction = m_shaderVariantAsset->GetShaderStageFunction(RHI::ShaderStage::Vertex);
                descriptorForDraw.m_geometryFunction = m_shaderVariantAsset->GetShaderStageFunction(RHI::ShaderStage::Geometry);
                descriptorForDraw.m_fragmentFunction = m_shaderVariantAsset->GetShaderStageFunction(RHI::ShaderStage::Fragment);
                descriptorForDraw.m_renderStates = *m_renderStates;
                break;
            }

            case RHI::PipelineStateType::Dispatch:
            {
                AZ_Assert(m_pipelineStateType == RHI::PipelineStateType::Dispatch, "ShaderVariant is not intended for the compute pipeline.");
                RHI::PipelineStateDescriptorForDispatch& descriptorForDispatch = static_cast<RHI::PipelineStateDescriptorForDispatch&>(descriptor);
                descriptorForDispatch.m_computeFunction = m_shaderVariantAsset->GetShaderStageFunction(RHI::ShaderStage::Compute);
                break;
            }

            case RHI::PipelineStateType::RayTracing:
            {
                AZ_Assert(m_pipelineStateType == RHI::PipelineStateType::RayTracing, "ShaderVariant is not intended for the ray tracing pipeline.");
                RHI::PipelineStateDescriptorForRayTracing& descriptorForRayTracing = static_cast<RHI::PipelineStateDescriptorForRayTracing&>(descriptor);
                descriptorForRayTracing.m_rayTracingFunction = m_shaderVariantAsset->GetShaderStageFunction(RHI::ShaderStage::RayTracing);
                break;
            }

            default:
                AZ_Assert(false, "Unexpected PipelineStateType");
                break;
            }
        }

        RHI::SpecializationDataPtr ShaderVariant::GetOrCreateSpecializationData(
            const ShaderOptionGroup& specialization) const
        {
            AZ_Assert(m_specializationCache, "Specialization cache was not initialized.");

            ShaderVariantKey specializationKey = specialization.GetShaderVariantKey();
            specializationKey &= m_specializationCache->m_specializationMask;

            RHI::SpecializationData::Key key;
            key.reserve(specializationKey.num_words());
            for (size_t wordIndex = 0; wordIndex < specializationKey.num_words(); ++wordIndex)
            {
                key.emplace_back(specializationKey.data()[wordIndex]);
            }

            {
                AZStd::shared_lock<AZStd::shared_mutex> lock(m_specializationCache->m_mutex);
                auto payload = m_specializationCache->m_payloads.find(key);
                if (payload != m_specializationCache->m_payloads.end())
                {
                    return payload->second;
                }
            }

            AZStd::unique_lock<AZStd::shared_mutex> lock(m_specializationCache->m_mutex);
            auto payload = m_specializationCache->m_payloads.find(key);
            if (payload != m_specializationCache->m_payloads.end())
            {
                return payload->second;
            }

            HashValue64 constantsHash = HashValue64{ 0 };
            for (const SpecializationCache::OptionMetadata& metadata : *m_specializationCache->m_metadata)
            {
                const RHI::SpecializationConstant constant =
                    SpecializationCache::MakeConstant(metadata, specialization);
                constantsHash = TypeHash64(constant.GetHash(), constantsHash);
            }

            const SpecializationCache::MetadataListPtr metadata = m_specializationCache->m_metadata;
            RHI::SpecializationDataPtr specializationData = AZStd::make_shared<RHI::SpecializationData>(
                key,
                m_specializationCache->m_domainHash,
                constantsHash,
                [metadata, specialization](AZStd::vector<RHI::SpecializationConstant>& constants)
                {
                    constants.clear();
                    constants.reserve(metadata->size());
                    for (const SpecializationCache::OptionMetadata& optionMetadata : *metadata)
                    {
                        constants.emplace_back(SpecializationCache::MakeConstant(optionMetadata, specialization));
                    }
                });

            m_specializationCache->m_payloads.emplace(AZStd::move(key), specializationData);
            return specializationData;
        }

        void ShaderVariant::ConfigurePipelineState(RHI::PipelineStateDescriptor& descriptor) const
        {
            auto layout = m_shaderAsset->GetShaderOptionGroupLayout();
            for ([[maybe_unused]] auto& option : layout->GetShaderOptions())
            {
                if (m_useSpecializationConstants && option.GetSpecializationId() >= 0)
                {
                    AZ_Error(
                        "ConfigurePipelineState",
                        !m_useSpecializationConstants || option.GetSpecializationId() < 0,
                        "Configuring PipelineStateDescriptor without specializing option %s.\
                         Call ConfigurePipelineState with specialization data. Default value will be used.",
                        option.GetName().GetCStr());
                }
            }
            ConfigurePipelineState(descriptor, ShaderOptionGroup(layout));
        }

        bool ShaderVariant::IsFullySpecialized() const
        {
            return m_shaderAsset->IsFullySpecialized(m_supervariantIndex);
        }

        bool ShaderVariant::UseSpecializationConstants() const
        {
            return m_shaderAsset->UseSpecializationConstants(m_supervariantIndex);
        }

        bool ShaderVariant::UseKeyFallback() const
        {
            return !(IsFullyBaked() || IsFullySpecialized());
        }

    } // namespace RPI
} // namespace AZ
