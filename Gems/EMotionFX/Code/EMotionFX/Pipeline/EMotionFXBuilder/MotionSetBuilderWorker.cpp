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

#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Utils/Utils.h>
#include <AzCore/std/sort.h>
#include <AzFramework/StringFunc/StringFunc.h>

namespace EMotionFX
{
    namespace EMotionFXBuilder
    {
        void MotionSetBuilderWorker::RegisterBuilderWorker()
        {
            AssetBuilderSDK::AssetBuilderDesc motionSetBuilderDescriptor;
            motionSetBuilderDescriptor.m_name = "MotionSetBuilderWorker";
            motionSetBuilderDescriptor.m_patterns.emplace_back(AssetBuilderSDK::AssetBuilderPattern("*.motionset", AssetBuilderSDK::AssetBuilderPattern::PatternType::Wildcard));
            motionSetBuilderDescriptor.m_busId = azrtti_typeid<MotionSetBuilderWorker>();
            // Version 5 introduced the cooked MotionSetAsset::SerializedData product format. Version 6 preserves
            // the legacy behavior of tolerating motion-set entries whose products do not exist.
            motionSetBuilderDescriptor.m_version = 6;
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

            for (const AssetBuilderSDK::PlatformInfo& info : request.m_enabledPlatforms)
            {
                AssetBuilderSDK::JobDescriptor descriptor;
                descriptor.m_jobKey = "motionset";
                descriptor.m_critical = true;
                descriptor.SetPlatformIdentifier(info.m_identifier.c_str());
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

            AssetBuilderSDK::ProductPathDependencySet motionPathDependencies;
            if (!ParseProductDependencies(request.m_fullPath, request.m_sourceFile, motionPathDependencies))
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

            AZStd::vector<AZStd::string> motionPaths;
            motionPaths.reserve(motionPathDependencies.size());
            for (const AssetBuilderSDK::ProductPathDependency& dependency : motionPathDependencies)
            {
                motionPaths.push_back(dependency.m_dependencyPath);
            }
            AZStd::sort(motionPaths.begin(), motionPaths.end());

            AssetBuilderSDK::JobProduct jobProduct(destPath, azrtti_typeid<Integration::MotionSetAsset>(), 0);
            for (const AZStd::string& motionPath : motionPaths)
            {
                AZ::Data::AssetId motionAssetId;
                AZ::Data::AssetCatalogRequestBus::BroadcastResult(
                    motionAssetId,
                    &AZ::Data::AssetCatalogRequestBus::Events::GetAssetIdByPath,
                    motionPath.c_str(),
                    AZ::Data::s_invalidAssetType,
                    false);

                if (!motionAssetId.IsValid())
                {
                    AZ_Warning(
                        AssetBuilderSDK::WarningWindow,
                        false,
                        "Motion product \"%s\" referenced by motion set %s could not be resolved and will not be preloaded.\n",
                        motionPath.c_str(),
                        fileName.c_str());
                    continue;
                }

                AZ::Data::Asset<Integration::MotionAsset> motionAsset(
                    motionAssetId,
                    azrtti_typeid<Integration::MotionAsset>(),
                    motionPath);
                motionAsset.SetAutoLoadBehavior(AZ::Data::AssetLoadBehavior::PreLoad);
                motionSetAssetData.m_motionAssets.push_back(AZStd::move(motionAsset));

                jobProduct.m_dependencies.emplace_back(
                    motionAssetId,
                    AZ::Data::ProductDependencyInfo::CreateFlags(AZ::Data::AssetLoadBehavior::PreLoad));
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
