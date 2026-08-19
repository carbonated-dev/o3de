/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include "MotionSetBuilderWorker.h"

#include <EMotionFX/Source/Importer/Importer.h>
#include <EMotionFX/Source/MotionSet.h>
#include <EMotionFX/Source/EMotionFXManager.h>
#include <Integration/Assets/MotionSetAsset.h>
#if defined(CARBONATED)
#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Utils/Utils.h>
#include <AzCore/std/sort.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzToolsFramework/API/EditorAssetSystemAPI.h>
#endif

namespace EMotionFX
{
    namespace EMotionFXBuilder
    {
#if defined(CARBONATED)
        namespace
        {
            struct AuthoredMotionDependency
            {
                AZStd::string m_productPath;
                AZStd::string m_sourcePath;
            };

            bool LoadAuthoredMotionDependencies(
                const AZStd::string& motionSetPath, AZStd::vector<AuthoredMotionDependency>& dependencies)
            {
                AZ::ObjectStream::FilterDescriptor loadFilter(
                    &AZ::Data::AssetFilterNoAssetLoading,
                    AZ::ObjectStream::FILTERFLAG_IGNORE_UNKNOWN_CLASSES);
                AZStd::unique_ptr<MotionSet> motionSet(GetImporter().LoadMotionSet(motionSetPath, nullptr, loadFilter));
                if (!motionSet)
                {
                    return false;
                }

                dependencies.reserve(motionSet->GetNumMotionEntries());
                for (const auto& [motionId, motionEntry] : motionSet->GetMotionEntries())
                {
                    if (motionEntry->GetSourceFilename().empty())
                    {
                        AZ_Error(
                            AssetBuilderSDK::ErrorWindow,
                            false,
                            "Motion \"%s\" (entry \"%s\") in \"%s\" does not contain its Scene source path. "
                            "Open and save the motion set in Animation Editor to upgrade it.\n",
                            motionEntry->GetFilename(),
                            motionId.c_str(),
                            motionSetPath.c_str());
                        return false;
                    }
                    dependencies.push_back({ motionEntry->GetFilenameString(), motionEntry->GetSourceFilename() });
                }

                AZStd::sort(
                    dependencies.begin(),
                    dependencies.end(),
                    [](const AuthoredMotionDependency& left, const AuthoredMotionDependency& right)
                    {
                        if (left.m_productPath == right.m_productPath)
                        {
                            return left.m_sourcePath < right.m_sourcePath;
                        }
                        return left.m_productPath < right.m_productPath;
                    });

                for (size_t dependencyIndex = 1; dependencyIndex < dependencies.size(); ++dependencyIndex)
                {
                    if (dependencies[dependencyIndex - 1].m_productPath == dependencies[dependencyIndex].m_productPath &&
                        dependencies[dependencyIndex - 1].m_sourcePath != dependencies[dependencyIndex].m_sourcePath)
                    {
                        AZ_Error(
                            AssetBuilderSDK::ErrorWindow,
                            false,
                            "Motion product \"%s\" in \"%s\" declares conflicting Scene sources.\n",
                            dependencies[dependencyIndex].m_productPath.c_str(),
                            motionSetPath.c_str());
                        return false;
                    }
                }

                dependencies.erase(
                    AZStd::unique(
                        dependencies.begin(),
                        dependencies.end(),
                        [](const AuthoredMotionDependency& left, const AuthoredMotionDependency& right)
                        {
                            return left.m_productPath == right.m_productPath;
                        }),
                    dependencies.end());
                return true;
            }

            bool ResolveMotionAssetId(
                const AZStd::string& motionPath,
                const AZStd::string& motionSourcePath,
                AZ::Data::AssetId& motionAssetId)
            {
                AZ::Data::AssetCatalogRequestBus::BroadcastResult(
                    motionAssetId,
                    &AZ::Data::AssetCatalogRequestBus::Events::GetAssetIdByPath,
                    motionPath.c_str(),
                    AZ::Data::s_invalidAssetType,
                    false);
                if (motionAssetId.IsValid())
                {
                    return true;
                }

                // The builder catalog can lag behind the Asset Processor database. The authored source path lets us query
                // its products directly without reverse-resolving the product during a clean-cache build.
                AZ::Data::AssetInfo sourceInfo;
                AZStd::string watchFolder;
                bool foundSource = false;
                AzToolsFramework::AssetSystemRequestBus::BroadcastResult(
                    foundSource,
                    &AzToolsFramework::AssetSystem::AssetSystemRequest::GetSourceInfoBySourcePath,
                    motionSourcePath.c_str(),
                    sourceInfo,
                    watchFolder);
                if (!foundSource || !sourceInfo.m_assetId.IsValid())
                {
                    return false;
                }

                AZStd::vector<AZ::Data::AssetInfo> productInfos;
                bool foundProducts = false;
                AzToolsFramework::AssetSystemRequestBus::BroadcastResult(
                    foundProducts,
                    &AzToolsFramework::AssetSystem::AssetSystemRequest::GetAssetsProducedBySourceUUID,
                    sourceInfo.m_assetId.m_guid,
                    productInfos);
                if (!foundProducts)
                {
                    return false;
                }

                for (const AZ::Data::AssetInfo& productInfo : productInfos)
                {
                    if (productInfo.m_assetType == azrtti_typeid<Integration::MotionAsset>() &&
                        AzFramework::StringFunc::Equal(productInfo.m_relativePath.c_str(), motionPath.c_str(), false))
                    {
                        motionAssetId = productInfo.m_assetId;
                        return motionAssetId.IsValid();
                    }
                }

                return false;
            }
        } // namespace
#endif

        void MotionSetBuilderWorker::RegisterBuilderWorker()
        {
            AssetBuilderSDK::AssetBuilderDesc motionSetBuilderDescriptor;
            motionSetBuilderDescriptor.m_name = "MotionSetBuilderWorker";
            motionSetBuilderDescriptor.m_patterns.emplace_back(AssetBuilderSDK::AssetBuilderPattern("*.motionset", AssetBuilderSDK::AssetBuilderPattern::PatternType::Wildcard));
            motionSetBuilderDescriptor.m_busId = azrtti_typeid<MotionSetBuilderWorker>();
#if defined(CARBONATED)
            // Version 4 reads the producing Scene source directly from each authored motion-set entry.
            motionSetBuilderDescriptor.m_version = 4;
#else
            motionSetBuilderDescriptor.m_version = 3;
#endif
            motionSetBuilderDescriptor.m_createJobFunction =
                AZStd::bind(&MotionSetBuilderWorker::CreateJobs, this, AZStd::placeholders::_1, AZStd::placeholders::_2);
            motionSetBuilderDescriptor.m_processJobFunction =
                AZStd::bind(&MotionSetBuilderWorker::ProcessJob, this, AZStd::placeholders::_1, AZStd::placeholders::_2);

            BusConnect(motionSetBuilderDescriptor.m_busId);

            AssetBuilderSDK::AssetBuilderBus::Broadcast(&AssetBuilderSDK::AssetBuilderBusTraits::RegisterBuilderInformation, motionSetBuilderDescriptor);
        }

        void MotionSetBuilderWorker::ShutDown()
        {
            m_isShuttingDown = true;
        }

        void MotionSetBuilderWorker::CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response)
        {
            if (m_isShuttingDown)
            {
                response.m_result = AssetBuilderSDK::CreateJobsResultCode::ShuttingDown;
                return;
            }

#if defined(CARBONATED)
            AZStd::string fullPath;
            AzFramework::StringFunc::Path::ConstructFull(
                request.m_watchFolder.c_str(), request.m_sourceFile.c_str(), fullPath, true);

            AZStd::vector<AuthoredMotionDependency> motionDependencies;
            if (!LoadAuthoredMotionDependencies(fullPath, motionDependencies))
            {
                response.m_result = AssetBuilderSDK::CreateJobsResultCode::Failed;
                return;
            }

            AZStd::vector<AZStd::string> motionSourcePaths;
            motionSourcePaths.reserve(motionDependencies.size());
            for (const AuthoredMotionDependency& dependency : motionDependencies)
            {
                motionSourcePaths.push_back(dependency.m_sourcePath);
            }
            AZStd::sort(motionSourcePaths.begin(), motionSourcePaths.end());
            motionSourcePaths.erase(AZStd::unique(motionSourcePaths.begin(), motionSourcePaths.end()), motionSourcePaths.end());
#endif

            for (const AssetBuilderSDK::PlatformInfo& info : request.m_enabledPlatforms)
            {
                AssetBuilderSDK::JobDescriptor descriptor;
                descriptor.m_jobKey = "motionset";
                descriptor.m_critical = true;
                descriptor.SetPlatformIdentifier(info.m_identifier.c_str());

#if defined(CARBONATED)
                // The motion set product embeds the AssetIds of its motion products. Make sure Scene compilation has
                // registered those products before this job attempts to resolve them.
                for (const AZStd::string& sourcePath : motionSourcePaths)
                {
                    AssetBuilderSDK::SourceFileDependency sourceDependency;
                    sourceDependency.m_sourceFileDependencyPath = sourcePath;
                    descriptor.m_jobDependencyList.emplace_back(
                        "Scene compilation",
                        info.m_identifier.c_str(),
                        AssetBuilderSDK::JobDependencyType::OrderOnly,
                        AZStd::move(sourceDependency));
                }
#endif

                response.m_createJobOutputs.push_back(descriptor);
            }

            response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
        }

        void MotionSetBuilderWorker::ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response)
        {
            AZ_TracePrintf(AssetBuilderSDK::InfoWindow, "MotionSetBuilderWorker Starting Job.\n");

            if (m_isShuttingDown)
            {
                AZ_TracePrintf(AssetBuilderSDK::WarningWindow, "Cancelled job %s because shutdown was requested.\n", request.m_fullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
                return;
            }

            AZStd::string fileName;
            AzFramework::StringFunc::Path::GetFullFileName(request.m_fullPath.c_str(), fileName);

            AZStd::string destPath;
            // Do all work inside the tempDirPath.
            AzFramework::StringFunc::Path::ConstructFull(request.m_tempDirPath.c_str(), fileName.c_str(), destPath, true);

#if defined(CARBONATED)
            AZStd::vector<AuthoredMotionDependency> motionDependencies;
            if (!LoadAuthoredMotionDependencies(request.m_fullPath, motionDependencies))
            {
                AZ_Error(AssetBuilderSDK::ErrorWindow, false, "Error while parsing motion dependencies for asset %s.\n", fileName.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }

            auto nativeDataOutcome = AZ::Utils::ReadFile<AZStd::vector<AZ::u8>>(request.m_fullPath);
            if (!nativeDataOutcome.IsSuccess())
            {
                AZ_Error(
                    AssetBuilderSDK::ErrorWindow,
                    false,
                    "Failed to read motion set asset %s: %s\n",
                    fileName.c_str(),
                    nativeDataOutcome.GetError().c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }

            Integration::MotionSetAsset::SerializedData motionSetAssetData;
            motionSetAssetData.m_emfxNativeData = nativeDataOutcome.TakeValue();

            AssetBuilderSDK::JobProduct jobProduct(destPath, azrtti_typeid<Integration::MotionSetAsset>(), 0);
            for (const AuthoredMotionDependency& dependency : motionDependencies)
            {
                AZ::Data::AssetId motionAssetId;
                if (!ResolveMotionAssetId(dependency.m_productPath, dependency.m_sourcePath, motionAssetId))
                {
                    AZ_Error(
                        AssetBuilderSDK::ErrorWindow,
                        false,
                        "Motion product \"%s\" referenced by motion set %s could not be resolved.\n",
                        dependency.m_productPath.c_str(),
                        fileName.c_str());
                    continue;
                }

                AZ::Data::Asset<Integration::MotionAsset> motionAsset(
                    motionAssetId,
                    azrtti_typeid<Integration::MotionAsset>(),
                    dependency.m_productPath);
                motionAsset.SetAutoLoadBehavior(AZ::Data::AssetLoadBehavior::PreLoad);
                motionSetAssetData.m_motionAssets.push_back(AZStd::move(motionAsset));

                jobProduct.m_dependencies.emplace_back(
                    motionAssetId,
                    AZ::Data::ProductDependencyInfo::CreateFlags(AZ::Data::AssetLoadBehavior::PreLoad));
            }

            if (motionSetAssetData.m_motionAssets.size() != motionDependencies.size())
            {
                AZ_Error(
                    AssetBuilderSDK::ErrorWindow,
                    false,
                    "Motion set %s references %zu motion products, but only %zu could be resolved. "
                    "Refusing to emit a cooked motion set with an incomplete preload list.\n",
                    fileName.c_str(),
                    motionDependencies.size(),
                    motionSetAssetData.m_motionAssets.size());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }

            if (!AZ::Utils::SaveObjectToFile(destPath, AZ::DataStream::ST_BINARY, &motionSetAssetData))
            {
                AZ_Error(AssetBuilderSDK::ErrorWindow, false, "Failed to save cooked motion set asset %s.\n", destPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
                return;
            }

            AZ_TracePrintf(
                AssetBuilderSDK::InfoWindow,
                "Saved cooked motion set %s with %zu preload motion dependencies.\n",
                fileName.c_str(),
                motionSetAssetData.m_motionAssets.size());

            jobProduct.m_dependenciesHandled = true;
            response.m_outputProducts.push_back(AZStd::move(jobProduct));
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
#else
            AssetBuilderSDK::JobProduct jobProduct(request.m_fullPath, azrtti_typeid<Integration::MotionSetAsset>(), 0);

            if (!ParseProductDependencies(request.m_fullPath, request.m_sourceFile, jobProduct.m_pathDependencies))
            {
                AZ_Error(AssetBuilderSDK::ErrorWindow, false, "Error during outputing product dependencies for asset %s.\n", fileName.c_str());
            }

            jobProduct.m_dependenciesHandled = true; // We've output the dependencies immediately above so it's OK to tell the AP we've handled dependencies
            response.m_outputProducts.push_back(jobProduct);
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
#endif
        }

        bool MotionSetBuilderWorker::ParseProductDependencies(const AZStd::string& fullPath, [[maybe_unused]] const AZStd::string& sourceFile, AssetBuilderSDK::ProductPathDependencySet& pathDependencies)
        {
            AZ::ObjectStream::FilterDescriptor loadFilter = AZ::ObjectStream::FilterDescriptor(&AZ::Data::AssetFilterNoAssetLoading, AZ::ObjectStream::FILTERFLAG_IGNORE_UNKNOWN_CLASSES);
            AZStd::unique_ptr<MotionSet> motionSet(GetImporter().LoadMotionSet(fullPath, nullptr, loadFilter));

            if (!motionSet)
            {
                return false;
            }

            for (const AZStd::pair<AZStd::string, MotionSet::MotionEntry*>& it : motionSet->GetMotionEntries())
            {
                pathDependencies.emplace(it.second->GetFilename(), AssetBuilderSDK::ProductPathDependencyType::ProductFile);
            }

            return true;
        }
    }
}
