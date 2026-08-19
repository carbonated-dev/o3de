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
#include <SceneAPIExt/Groups/IMotionGroup.h>

#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/IO/Path/Path.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Utils/Utils.h>
#include <AzCore/std/sort.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzToolsFramework/API/EditorAssetSystemAPI.h>
#include <SceneAPI/SceneCore/Containers/SceneManifest.h>
#include <SceneAPI/SceneCore/Events/AssetImportRequest.h>
#include <SceneAPI/SceneCore/Utilities/FileUtilities.h>
#endif

namespace EMotionFX
{
    namespace EMotionFXBuilder
    {
#if defined(CARBONATED)
        namespace
        {
            bool GetMotionSourcePathFromDatabase(const AZStd::string& motionPath, AZStd::string& sourcePath)
            {
                bool resolvedSourcePath = false;
                AzToolsFramework::AssetSystemRequestBus::BroadcastResult(
                    resolvedSourcePath,
                    &AzToolsFramework::AssetSystem::AssetSystemRequest::GetFullSourcePathFromRelativeProductPath,
                    motionPath,
                    sourcePath);
                return resolvedSourcePath;
            }

            bool ManifestProducesMotion(
                const AZStd::string& manifestPath, const AZ::IO::PathView& expectedMotionFileName)
            {
                AZ::SceneAPI::Containers::SceneManifest manifest;
                if (!manifest.LoadFromFile(manifestPath))
                {
                    AZ_Warning(
                        AssetBuilderSDK::WarningWindow,
                        false,
                        "Unable to read Scene manifest \"%s\" while resolving motion dependencies.\n",
                        manifestPath.c_str());
                    return false;
                }

                for (const auto& manifestObject : manifest.GetValueStorage())
                {
                    const auto* motionGroup = azrtti_cast<const Pipeline::Group::IMotionGroup*>(manifestObject.get());
                    if (!motionGroup)
                    {
                        continue;
                    }

                    const AZ::IO::Path generatedMotionPath(
                        AZ::SceneAPI::Utilities::FileUtilities::CreateOutputFileName(
                            motionGroup->GetName(), {}, "motion", {}));
                    const AZStd::string generatedMotionFileName(generatedMotionPath.Filename().Native());
                    const AZStd::string expectedMotionFileNameString(expectedMotionFileName.Native());
                    if (AzFramework::StringFunc::Equal(
                            generatedMotionFileName.c_str(), expectedMotionFileNameString.c_str(), false))
                    {
                        return true;
                    }
                }

                return false;
            }

            bool FindMotionSourceFromManifests(const AZStd::string& motionPath, AZStd::string& sourcePath)
            {
                const AZ::IO::Path motionProductPath(motionPath);
                const AZ::IO::Path relativeDirectory = motionProductPath.ParentPath();

                AZStd::vector<AZStd::string> scanFolders;
                bool foundScanFolders = false;
                AzToolsFramework::AssetSystemRequestBus::BroadcastResult(
                    foundScanFolders,
                    &AzToolsFramework::AssetSystem::AssetSystemRequest::GetScanFolders,
                    scanFolders);
                if (!foundScanFolders)
                {
                    return false;
                }

                AZStd::vector<AZStd::string> candidateSources;
                for (const AZStd::string& scanFolder : scanFolders)
                {
                    const AZ::IO::Path sourceDirectory = AZ::IO::Path(scanFolder) / relativeDirectory;
                    AZ::IO::SystemFile::FindFiles(
                        (sourceDirectory / "*.assetinfo").c_str(),
                        [&candidateSources, &sourceDirectory, &motionProductPath](const char* fileName, bool isFile)
                        {
                            if (!isFile)
                            {
                                return true;
                            }

                            const AZ::IO::Path manifestPath = sourceDirectory / fileName;
                            AZ::IO::Path candidateSource = manifestPath;
                            candidateSource.ReplaceExtension({});
                            if (AZ::IO::SystemFile::Exists(candidateSource.c_str()) &&
                                ManifestProducesMotion(manifestPath.Native(), motionProductPath.Filename()))
                            {
                                candidateSources.push_back(candidateSource.Native());
                            }
                            return true;
                        });
                }

                AZStd::sort(candidateSources.begin(), candidateSources.end());
                candidateSources.erase(AZStd::unique(candidateSources.begin(), candidateSources.end()), candidateSources.end());
                if (candidateSources.size() == 1)
                {
                    sourcePath = AZStd::move(candidateSources.front());
                    return true;
                }

                if (candidateSources.size() > 1)
                {
                    AZ_Error(
                        AssetBuilderSDK::ErrorWindow,
                        false,
                        "Motion product \"%s\" is declared by more than one Scene manifest. The producer is ambiguous.\n",
                        motionPath.c_str());
                }
                return false;
            }

            bool FindVerifiedSameStemSource(const AZStd::string& motionPath, AZStd::string& sourcePath)
            {
                AZStd::unordered_set<AZStd::string> sceneExtensions;
                AZ::SceneAPI::Events::AssetImportRequestBus::Broadcast(
                    &AZ::SceneAPI::Events::AssetImportRequestBus::Events::GetSupportedFileExtensions,
                    sceneExtensions);

                AZStd::vector<AZStd::string> candidateSources;
                for (const AZStd::string& extension : sceneExtensions)
                {
                    AZ::IO::Path candidateRelativePath(motionPath);
                    candidateRelativePath.ReplaceExtension(AZ::IO::PathView(extension));

                    AZ::Data::AssetInfo sourceInfo;
                    AZStd::string watchFolder;
                    bool foundSource = false;
                    AzToolsFramework::AssetSystemRequestBus::BroadcastResult(
                        foundSource,
                        &AzToolsFramework::AssetSystem::AssetSystemRequest::GetSourceInfoBySourcePath,
                        candidateRelativePath.c_str(),
                        sourceInfo,
                        watchFolder);
                    if (!foundSource)
                    {
                        continue;
                    }

                    const AZ::IO::Path candidateFullPath = AZ::IO::Path(watchFolder) / sourceInfo.m_relativePath;
                    AZ::IO::Path manifestPath = candidateFullPath;
                    manifestPath.Native() += ".assetinfo";

                    // A source manifest explicitly controls its outputs. If it exists, the manifest resolver above must
                    // identify the matching MotionGroup; do not override it with a filename convention.
                    if (!AZ::IO::SystemFile::Exists(manifestPath.c_str()))
                    {
                        candidateSources.push_back(candidateFullPath.Native());
                    }
                }

                AZStd::sort(candidateSources.begin(), candidateSources.end());
                candidateSources.erase(AZStd::unique(candidateSources.begin(), candidateSources.end()), candidateSources.end());
                if (candidateSources.size() == 1)
                {
                    sourcePath = AZStd::move(candidateSources.front());
                    return true;
                }

                if (candidateSources.size() > 1)
                {
                    AZ_Error(
                        AssetBuilderSDK::ErrorWindow,
                        false,
                        "Motion product \"%s\" has more than one same-stem Scene source. The producer is ambiguous.\n",
                        motionPath.c_str());
                }
                return false;
            }

            bool GetMotionSourcePath(const AZStd::string& motionPath, AZStd::string& sourcePath)
            {
                if (GetMotionSourcePathFromDatabase(motionPath, sourcePath) ||
                    FindMotionSourceFromManifests(motionPath, sourcePath) ||
                    FindVerifiedSameStemSource(motionPath, sourcePath))
                {
                    return true;
                }

                return false;
            }

            bool ResolveMotionAssetId(const AZStd::string& motionPath, AZ::Data::AssetId& motionAssetId)
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

                // The builder catalog can lag behind the Asset Processor database. Query the source and its products through
                // the tools asset-system API so a newly generated motion product can still be resolved during this job.
                AZStd::string sourcePath;
                if (!GetMotionSourcePath(motionPath, sourcePath))
                {
                    return false;
                }
                AZ::Data::AssetInfo sourceInfo;
                AZStd::string watchFolder;
                bool foundSource = false;
                AzToolsFramework::AssetSystemRequestBus::BroadcastResult(
                    foundSource,
                    &AzToolsFramework::AssetSystem::AssetSystemRequest::GetSourceInfoBySourcePath,
                    sourcePath.c_str(),
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
            // Version 4 resolves Scene sources through product records or MotionGroup manifests and rejects partially
            // resolved motion preload lists.
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

            AssetBuilderSDK::ProductPathDependencySet motionPathDependencies;
            if (!ParseProductDependencies(fullPath, request.m_sourceFile, motionPathDependencies))
            {
                response.m_result = AssetBuilderSDK::CreateJobsResultCode::Failed;
                return;
            }

            AZStd::vector<AZStd::string> motionSourcePaths;
            motionSourcePaths.reserve(motionPathDependencies.size());
            for (const AssetBuilderSDK::ProductPathDependency& dependency : motionPathDependencies)
            {
                AZStd::string sourcePath;
                if (!GetMotionSourcePath(dependency.m_dependencyPath, sourcePath))
                {
                    AZ_Error(
                        AssetBuilderSDK::ErrorWindow,
                        false,
                        "Unable to identify the Scene source that produces motion \"%s\" referenced by \"%s\".\n",
                        dependency.m_dependencyPath.c_str(),
                        request.m_sourceFile.c_str());
                    response.m_result = AssetBuilderSDK::CreateJobsResultCode::Failed;
                    return;
                }
                motionSourcePaths.push_back(AZStd::move(sourcePath));
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
                if (!ResolveMotionAssetId(motionPath, motionAssetId))
                {
                    AZ_Error(
                        AssetBuilderSDK::ErrorWindow,
                        false,
                        "Motion product \"%s\" referenced by motion set %s could not be resolved.\n",
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

            if (motionSetAssetData.m_motionAssets.size() != motionPaths.size())
            {
                AZ_Error(
                    AssetBuilderSDK::ErrorWindow,
                    false,
                    "Motion set %s references %zu motion products, but only %zu could be resolved. "
                    "Refusing to emit a cooked motion set with an incomplete preload list.\n",
                    fileName.c_str(),
                    motionPaths.size(),
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
