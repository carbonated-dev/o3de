/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RHI/PipelineStateCache.h>
#include <Atom/RHI/PipelineLibraryNotificationBus.h>
#include <Atom/RHI/Factory.h>

#include <AzCore/Debug/Profiler.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/parallel/exponential_backoff.h>

#if defined(CARBONATED)
#include <Atom/RHI/RHISystemInterface.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/Memory/MemoryMarker.h>
#include <AzCore/Serialization/Utils.h>
#endif

#if defined(CARBONATED)
AZ_CVAR(
    AZ::u32,
    r_pipelineLibraryStrategy,
    static_cast<AZ::u32>(AZ::RHI::PipelineLibraryStrategy::Global),
    nullptr,
    AZ::ConsoleFunctorFlags::NeedsReload,
    "Pipeline library strategy. 0: Per Shader; 1: Global. The value is captured when PipelineStateCache is created and changing it requires a restart.");

#endif
namespace AZ::RHI
{
#if defined(CARBONATED)
    namespace
    {
        constexpr int GlobalPipelineCacheVersion = 0;

        void SaveGlobalPipelineLibraryCommand([[maybe_unused]] const ConsoleCommandContainer& arguments)
        {
            RHISystemInterface* rhiSystem = RHISystemInterface::Get();
            if (!rhiSystem || !rhiSystem->GetPipelineStateCache())
            {
                AZ_Warning("PipelineStateCache", false, "The RHI pipeline state cache is not initialized.");
                return;
            }

            if (rhiSystem->GetPipelineStateCache()->SaveGlobalPipelineLibrary())
            {
                AZ_Printf("PipelineStateCache", "Saved the global pipeline library.\n");
            }
        }

        AZ_CONSOLEFREEFUNC(
            "SaveGlobalPipelineLibrary",
            SaveGlobalPipelineLibraryCommand,
            ConsoleFunctorFlags::DontReplicate,
            "Save the global RHI pipeline library to disk.");

        AZStd::string GetGlobalPipelineLibraryPath(const Device& device)
        {
            IO::FileIOBase* fileIO = IO::FileIOBase::GetInstance();
            if (!fileIO)
            {
                return {};
            }

            const PhysicalDeviceDescriptor physicalDeviceDescriptor = device.GetPhysicalDevice().GetDescriptor();
            const char* configName = BuildOptions::IsDebugBuild
                ? "Debug"
                : (BuildOptions::IsProfileBuild ? "Profile" : "Release");

            char unresolvedPath[AZ_MAX_PATH_LEN];
            azsnprintf(
                unresolvedPath,
                AZ_MAX_PATH_LEN,
                "@user@/Atom/PipelineStateCache_%s_%u_%u_%s_Ver_%i/%s/Global.bin",
                ToString(physicalDeviceDescriptor.m_vendorId).data(),
                physicalDeviceDescriptor.m_deviceId,
                physicalDeviceDescriptor.m_driverVersion,
                configName,
                GlobalPipelineCacheVersion,
                Factory::Get().GetName().GetCStr());

            char resolvedPath[AZ_MAX_PATH_LEN];
            fileIO->ResolvePath(unresolvedPath, resolvedPath, AZ_MAX_PATH_LEN);
            return resolvedPath;
        }

        PipelineLibraryStrategy GetPipelineLibraryStrategyFromCVar(const Device& device)
        {
            const bool isValidStrategy =
                r_pipelineLibraryStrategy <= static_cast<AZ::u32>(PipelineLibraryStrategy::Global);
            AZ_Error(
                "PipelineStateCache",
                isValidStrategy,
                "Invalid r_pipelineLibraryStrategy value %u.",
                static_cast<AZ::u32>(r_pipelineLibraryStrategy));
            if (!isValidStrategy)
            {
                return PipelineLibraryStrategy::Shader;
            }

            const PipelineLibraryStrategy strategy =
                static_cast<PipelineLibraryStrategy>(static_cast<AZ::u32>(r_pipelineLibraryStrategy));
            if (strategy == PipelineLibraryStrategy::Global &&
                !device.GetFeatures().m_supportsGlobalPipelineLibrary)
            {
                AZ_Warning(
                    "PipelineStateCache",
                    false,
                    "The device does not support a global pipeline library. Falling back to Thread.");
                return PipelineLibraryStrategy::Shader;
            }

            return strategy;
        }
    }

#endif
    Ptr<PipelineStateCache> PipelineStateCache::Create(Device& device)
    {
        return aznew PipelineStateCache(device);
    }

    PipelineStateCache::PipelineStateCache(Device& device)
        : m_device{&device}
#if defined(CARBONATED)
        , m_pipelineLibraryStrategy{GetPipelineLibraryStrategyFromCVar(device)}
#endif
    {
#if defined(CARBONATED)
        if (m_pipelineLibraryStrategy == PipelineLibraryStrategy::Global)
        {
            m_globalLibrarySet.emplace_back();
            m_globalLibraryActiveBits[0] = true;

            PipelineLibraryDescriptor& descriptor = m_globalLibrarySet[0].m_pipelineLibraryDescriptor;
            descriptor.m_filePath = GetGlobalPipelineLibraryPath(device);
            if (r_enablePsoCaching)
            {
                if (!descriptor.m_filePath.empty() && device.GetFeatures().m_isPsoCacheFileOperationsNeeded)
                {
                    descriptor.m_serializedData =
                        Utils::LoadObjectFromFile<PipelineLibraryData>(descriptor.m_filePath);
                }
            }

            Ptr<PipelineLibrary> pipelineLibrary = Factory::Get().CreatePipelineLibrary();
            const ResultCode resultCode = pipelineLibrary->Init(*m_device, descriptor);
            if (resultCode != ResultCode::Success)
            {
                AZ_Warning(
                    "PipelineStateCache",
                    false,
                    "Failed to initialize the global pipeline library. PipelineLibrary usage is disabled.");
            }

            // Keep the object even if initialization failed so initialization is only attempted once.
            m_globalPipelineLibrary = AZStd::move(pipelineLibrary);
        }
#endif
    }

#if defined(CARBONATED)
    PipelineStateCache::~PipelineStateCache()
    {
        if (m_pipelineLibraryStrategy == PipelineLibraryStrategy::Global && r_enablePsoCaching)
        {
            SaveGlobalPipelineLibrary();
        }
    }

    bool PipelineStateCache::SaveGlobalPipelineLibrary() const
    {
        AZStd::unique_lock<AZStd::shared_mutex> lock(m_mutex);
        if (m_pipelineLibraryStrategy != PipelineLibraryStrategy::Global ||
            !m_globalPipelineLibrary ||
            !m_globalPipelineLibrary->IsInitialized())
        {
            AZ_Warning("PipelineStateCache", false, "A global pipeline library is not available.");
            return false;
        }

        const AZStd::string& filePath = m_globalLibrarySet[0].m_pipelineLibraryDescriptor.m_filePath;
        if (filePath.empty())
        {
            AZ_Warning("PipelineStateCache", false, "The global pipeline library path is unavailable.");
            return false;
        }

        bool result = false;
        if (m_device->GetFeatures().m_isPsoCacheFileOperationsNeeded)
        {
            ConstPtr<PipelineLibraryData> serializedData = m_globalPipelineLibrary->GetSerializedData();
            if (serializedData)
            {
                result = Utils::SaveObjectToFile<PipelineLibraryData>(
                    filePath, DataStream::ST_BINARY, serializedData.get());
            }
        }
        else
        {
            result = m_globalPipelineLibrary->SaveSerializedData(filePath);
        }

        AZ_Error("PipelineStateCache", result, "Global pipeline library %s was not saved.", filePath.c_str());
        return result;
    }

    PipelineLibraryStrategy PipelineStateCache::GetPipelineLibraryStrategy() const
    {
        return m_pipelineLibraryStrategy;
    }    
#endif

    void PipelineStateCache::ValidateCacheIntegrity() const
    {
#if defined(AZ_ENABLE_TRACING)
        for (size_t i = 0; i < m_globalLibrarySet.size(); ++i)
        {
            const GlobalLibraryEntry& globalLibraryEntry = m_globalLibrarySet[i];
            const PipelineStateSet& readOnlyCache = globalLibraryEntry.m_readOnlyCache;
            AZ_Assert(globalLibraryEntry.m_pendingCompileCount == 0, "Compiles are pending for pipeline library");
            AZ_Assert(globalLibraryEntry.m_pendingCache.empty(), "Pending cache is not empty.");

            if (!m_globalLibraryActiveBits[i])
            {
                AZ_Assert(readOnlyCache.empty(), "Inactive library has pipeline states in its global entry.");
            }

#if defined(AZ_DEBUG_BUILD)
            // the PipelineStateSet is expensive to duplicate, only do this in debug.
            PipelineStateSet readOnlyCacheCopy = readOnlyCache;
            AZ_Assert(AZStd::unique(readOnlyCacheCopy.begin(), readOnlyCacheCopy.end()) == readOnlyCacheCopy.end(),
                "'%d' Duplicates existed in the read-only cache!", readOnlyCache.size() - readOnlyCacheCopy.size());
#endif
        }

        m_threadLibrarySet.ForEach([this](const ThreadLibrarySet& threadLibrarySet)
        {
            const size_t libraryCount = m_globalLibrarySet.size();

            for (size_t i = 0; i < libraryCount; ++i)
            {
                const ThreadLibraryEntry& threadLibraryEntry = threadLibrarySet[i];

                if (!m_globalLibraryActiveBits[i])
                {
                    AZ_Assert(!threadLibraryEntry.m_library, "Inactive library has a valid RHI::PipelineLibrary instance.");
                }

                AZ_Assert(threadLibraryEntry.m_threadLocalCache.empty(), "Thread library should not have any items in its local cache.");
            }
        });
#endif
    }

    void PipelineStateCache::Reset()
    {
        AZStd::unique_lock<AZStd::shared_mutex> lock(m_mutex);

        for (size_t i = 0; i < m_globalLibrarySet.size(); ++i)
        {
            if (m_globalLibraryActiveBits[i])
            {
                ResetLibraryImpl(PipelineLibraryHandle(i));
            }
        }
    }

    PipelineLibraryHandle PipelineStateCache::CreateLibrary(const PipelineLibraryData* serializedData, const AZStd::string& filePath)
    {
#if defined(CARBONATED)
        if (m_pipelineLibraryStrategy == PipelineLibraryStrategy::Global)
        {
            return PipelineLibraryHandle{0};
        }

#endif
        AZStd::unique_lock<AZStd::shared_mutex> lock(m_mutex);

        PipelineLibraryHandle handle;
        if (!m_libraryFreeList.empty())
        {
            handle = m_libraryFreeList.back();
            m_libraryFreeList.pop_back();
        }
        else
        {
            if (m_globalLibrarySet.size() == LibraryCountMax)
            {
                AZ_Error(
                    "PipelineStateCache", false,
                    "Exceeded maximum number of allowed pipeline libraries in "
                    "cache. You must update LibraryCountMax to add more.");
                return {};
            }

            handle = PipelineLibraryHandle(m_globalLibrarySet.size());
            m_globalLibrarySet.emplace_back();
        }

        AZ_Assert(m_globalLibraryActiveBits[handle.GetIndex()] == false, "Attempted to allocate active library entry!");
        m_globalLibraryActiveBits[handle.GetIndex()] = true;

        GlobalLibraryEntry& libraryEntry = m_globalLibrarySet[handle.GetIndex()];
        libraryEntry.m_pipelineLibraryDescriptor.m_serializedData = serializedData;
        libraryEntry.m_pipelineLibraryDescriptor.m_filePath = filePath;
        AZ_Assert(libraryEntry.m_readOnlyCache.empty() && libraryEntry.m_pendingCache.empty(), "Library entry has entries in its caches!");

        return handle;
    }

    void PipelineStateCache::ReleaseLibrary(PipelineLibraryHandle handle)
    {
#if defined(CARBONATED)
        if (m_pipelineLibraryStrategy == PipelineLibraryStrategy::Global)
        {
            return;
        }

#endif
        if (handle.IsValid())
        {
            AZStd::unique_lock<AZStd::shared_mutex> lock(m_mutex);
            AZ_Assert(m_globalLibraryActiveBits[handle.GetIndex()], "Releasing a library that is no longer valid.");

            PipelineLibraryNotificationBus::Broadcast(
                &PipelineLibraryNotificationBus::Events::OnPipelineLibraryRelease, this, handle);

            ResetLibraryImpl(handle);

            GlobalLibraryEntry& libraryEntry = m_globalLibrarySet[handle.GetIndex()];
            libraryEntry.m_readOnlyCache.clear();
            libraryEntry.m_pipelineLibraryDescriptor.m_serializedData = nullptr;
            libraryEntry.m_pipelineLibraryDescriptor.m_filePath = "";
                
            m_globalLibraryActiveBits[handle.GetIndex()] = false;
            m_libraryFreeList.push_back(handle);
        }
    }

    void PipelineStateCache::ResetLibrary(PipelineLibraryHandle handle)
    {
        if (handle.IsValid())
        {
            AZStd::unique_lock<AZStd::shared_mutex> lock(m_mutex);
            ResetLibraryImpl(handle);
        }
    }

    void PipelineStateCache::ResetLibraryImpl(PipelineLibraryHandle handle)
    {
        m_threadLibrarySet.ForEach([handle](ThreadLibrarySet& librarySet)
        {
            ThreadLibraryEntry& libraryEntry = librarySet[handle.GetIndex()];
            libraryEntry.m_library = nullptr;
            libraryEntry.m_threadLocalCache.clear();
        });

        GlobalLibraryEntry& libraryEntry = m_globalLibrarySet[handle.GetIndex()];

        AZ_Assert(libraryEntry.m_pendingCompileCount == 0, "Reseting library while compiles are still pending!");
        libraryEntry.m_readOnlyCache.clear();
        libraryEntry.m_pendingCacheMutex.lock();
        libraryEntry.m_pendingCache.clear();
        libraryEntry.m_pendingCacheMutex.unlock();
    }

    Ptr<PipelineLibrary> PipelineStateCache::GetMergedLibrary(PipelineLibraryHandle handle) const
    {
        if (handle.IsNull())
        {
            return nullptr;
        }

#if defined(CARBONATED)
        if (m_pipelineLibraryStrategy == PipelineLibraryStrategy::Global)
        {
            return nullptr;
        }

#endif
        AZStd::unique_lock<AZStd::shared_mutex> lock(m_mutex);
        const GlobalLibraryEntry& entry = m_globalLibrarySet[handle.GetIndex()];

        //! Each thread has its own PipelineLibrary instance. To produce the final serialized data, we
        //! coalesce data from each individual library by merging the thread-local ones into a single
        //! global (temporary) library. The data is then extracted from this global library and returned.
        //! This operation is designed to happen once at application shutdown; certainly not every frame.
        AZStd::vector<const PipelineLibrary*> threadLibraries;
        m_threadLibrarySet.ForEach([handle, &threadLibraries](const ThreadLibrarySet& threadLibrarySet)
        {
            const ThreadLibraryEntry& threadLibraryEntry = threadLibrarySet[handle.GetIndex()];

            // Skip libraries that failed to initialize.
            if (threadLibraryEntry.m_library && threadLibraryEntry.m_library->IsInitialized())
            {
                threadLibraries.push_back(threadLibraryEntry.m_library.get());
            }
        });

        bool doesPSODataExist = entry.m_pipelineLibraryDescriptor.m_serializedData.get();
        for (const RHI::PipelineLibrary* libraryBase : threadLibraries)
        {
            const PipelineLibrary* library = static_cast<const PipelineLibrary*>(libraryBase);
            doesPSODataExist |= library->IsMergeRequired();
        }

        if (doesPSODataExist)
        {
            Ptr<PipelineLibrary> pipelineLibrary = Factory::Get().CreatePipelineLibrary();
            ResultCode resultCode = pipelineLibrary->Init(*m_device, entry.m_pipelineLibraryDescriptor);

            if (resultCode == ResultCode::Success)
            {
                resultCode = pipelineLibrary->MergeInto(threadLibraries);

                if (resultCode == ResultCode::Success)
                {
                    return pipelineLibrary;
                }
            }
        }

        return nullptr;
    }

#if defined(CARBONATED)
    void PipelineStateCache::Compact()
    {
        AZ_PROFILE_SCOPE(RHI, "PipelineStateCache: Compact");
        AZStd::unique_lock<AZStd::shared_mutex> lock(m_mutex);
        CompactInternal();
    }
#else
    void PipelineStateCache::Compact()
    {
        AZ_PROFILE_SCOPE(RHI, "PipelineStateCache: Compact");
        AZStd::unique_lock<AZStd::shared_mutex> lock(m_mutex);
        // Merge the pending cache into the read-only cache.
        bool hasCompiledPipelineStates = false;
        for (size_t i = 0; i < m_globalLibrarySet.size(); ++i)
        {
            GlobalLibraryEntry& globalLibraryEntry = m_globalLibrarySet[i];

            // Skip inactive libraries and ones that didn't compile anything this cycle.
            if (m_globalLibraryActiveBits[i] && !globalLibraryEntry.m_pendingCache.empty())
            {
                hasCompiledPipelineStates = true;

                // Allocate a temporary staging set, perform the merge, and then move it back into the read-only cache.
                PipelineStateSet mergeResult;
                mergeResult.reserve(globalLibraryEntry.m_readOnlyCache.size() + globalLibraryEntry.m_pendingCache.size());

                AZStd::merge(
                    globalLibraryEntry.m_readOnlyCache.begin(),
                    globalLibraryEntry.m_readOnlyCache.end(),
                    globalLibraryEntry.m_pendingCache.begin(),
                    globalLibraryEntry.m_pendingCache.end(),
                    AZStd::inserter(mergeResult, mergeResult.begin()));

                globalLibraryEntry.m_readOnlyCache.swap(mergeResult);
                globalLibraryEntry.m_pendingCache.clear();
            }
        }

        // If we had compilation events, then the thread-local caches are not empty and need to be cleared.
        if (hasCompiledPipelineStates)
        {
            const size_t libraryCount = m_globalLibrarySet.size();

            m_threadLibrarySet.ForEach(
                [this, libraryCount](ThreadLibrarySet& threadLibrarySet)
                {
                    for (size_t i = 0; i < libraryCount; ++i)
                    {
                        if (m_globalLibraryActiveBits[i])
                        {
                            threadLibrarySet[i].m_threadLocalCache.clear();
                        }
                    }
                });
        }

        ValidateCacheIntegrity();
    }
#endif

#if defined(CARBONATED)
    bool PipelineStateCache::TryCompact()
    {
        AZ_PROFILE_SCOPE(RHI, "PipelineStateCache: TryCompact");
        AZStd::unique_lock<AZStd::shared_mutex> lock(m_mutex, AZStd::try_to_lock);
        if (!lock.owns_lock())
        {
            return false;
        }

        CompactInternal();
        return true;
    }

    void PipelineStateCache::CompactInternal()
    {
        // Merge the pending cache into the read-only cache.
        bool hasCompiledPipelineStates = false;
        for (size_t i = 0; i < m_globalLibrarySet.size(); ++i)
        {
            GlobalLibraryEntry& globalLibraryEntry = m_globalLibrarySet[i];

            // Skip inactive libraries and ones that didn't compile anything this cycle.
            if (m_globalLibraryActiveBits[i] && !globalLibraryEntry.m_pendingCache.empty())
            {
                hasCompiledPipelineStates = true;

                // Allocate a temporary staging set, perform the merge, and then move it back into the read-only cache.
                PipelineStateSet mergeResult;
                mergeResult.reserve(globalLibraryEntry.m_readOnlyCache.size() + globalLibraryEntry.m_pendingCache.size());

                AZStd::merge(
                    globalLibraryEntry.m_readOnlyCache.begin(), globalLibraryEntry.m_readOnlyCache.end(),
                    globalLibraryEntry.m_pendingCache.begin(), globalLibraryEntry.m_pendingCache.end(),
                    AZStd::inserter(mergeResult, mergeResult.begin()));

                globalLibraryEntry.m_readOnlyCache.swap(mergeResult);
                globalLibraryEntry.m_pendingCache.clear();
            }
        }

        // If we had compilation events, then the thread-local caches are not empty and need to be cleared.
        if (hasCompiledPipelineStates)
        {
            const size_t libraryCount = m_globalLibrarySet.size();

            m_threadLibrarySet.ForEach([this, libraryCount](ThreadLibrarySet& threadLibrarySet)
            {
                for (size_t i = 0; i < libraryCount; ++i)
                {
                    if (m_globalLibraryActiveBits[i])
                    {
                        threadLibrarySet[i].m_threadLocalCache.clear();
                    }
                }
            });
        }

        ValidateCacheIntegrity();
    }
#endif

    const PipelineState* PipelineStateCache::FindPipelineState(const PipelineStateSet& pipelineStateSet, const PipelineStateDescriptor& descriptor)
    {
        auto pipelineStateIt = pipelineStateSet.find(PipelineStateEntry(descriptor.GetHash(), nullptr, descriptor));
        if (pipelineStateIt != pipelineStateSet.end())
        {
            return pipelineStateIt->m_pipelineState.get();
        }
        return nullptr;
    }

    bool PipelineStateCache::InsertPipelineState(PipelineStateSet& pipelineStateSet, PipelineStateEntry pipelineStateEntry)
    {
        auto ret = pipelineStateSet.insert(pipelineStateEntry);
        return ret.second;
    }

#if defined(CARBONATED)
    const PipelineState* PipelineStateCache::AcquirePipelineState(
        PipelineLibraryHandle handle,
        const PipelineStateDescriptor& descriptor,
        const AZ::Name& name /*= AZ::Name()*/)
    {
        return AcquirePipelineStateInternal(handle, descriptor, name, PipelineStateAcquireFlags::None).get();
    }

    ConstPtr<PipelineState> PipelineStateCache::AcquirePipelineState(
        PipelineLibraryHandle handle,
        const PipelineStateDescriptor& descriptor,
        PipelineStateAcquireFlags acquireFlags,
        const AZ::Name& name /*= AZ::Name()*/)
    {
        return AcquirePipelineStateInternal(handle, descriptor, name, acquireFlags);
    }

    ConstPtr<PipelineState> PipelineStateCache::AcquirePipelineStateInternal(
        PipelineLibraryHandle handle,
        const PipelineStateDescriptor& descriptor,
        const AZ::Name& name,
        PipelineStateAcquireFlags acquireFlags)
    {
        if (handle.IsNull())
        {
            return nullptr;
        }
        MEMORY_TAG(Shader);

        AZStd::shared_lock<AZStd::shared_mutex> lock(m_mutex);

        if (!m_globalLibraryActiveBits[handle.GetIndex()])
        {
            AZ_Error("PipelineStateCache", false, "PipelineLibrary %d is not in used (maybe released)", handle.GetIndex());
            return nullptr;
        }
        GlobalLibraryEntry& globalLibraryEntry = m_globalLibrarySet[handle.GetIndex()];
        PipelineStateHash pipelineStateHash = descriptor.GetHash();
        const bool noCompile = CheckBitsAny(acquireFlags, PipelineStateAcquireFlags::NoCompile);
        const bool noShare = CheckBitsAny(acquireFlags, PipelineStateAcquireFlags::NoShare);
        const bool useThreadLocalCache = !noShare || CheckBitsAny(acquireFlags, PipelineStateAcquireFlags::ThreadLocalCache);

        // Search the read-only cache first.
        if (const PipelineState* pipelineState = FindPipelineState(globalLibraryEntry.m_readOnlyCache, descriptor))
        {
            return pipelineState;
        }

        // NoCompile deliberately ignores thread-local and pending entries. They are not globally visible until Compact.
        if (noCompile)
        {
            return nullptr;
        }

        // Search the thread-local cache next.
        {
            ThreadLibrarySet& threadLibrarySet = m_threadLibrarySet.GetStorage();
            ThreadLibraryEntry& threadLibraryEntry = threadLibrarySet[handle.GetIndex()];
            PipelineStateSet& threadLocalCache = threadLibraryEntry.m_threadLocalCache;

            if (useThreadLocalCache)
            {
                if (const PipelineState* pipelineState = FindPipelineState(threadLocalCache, descriptor))
                {
                    return pipelineState;
                }
            }

            // No entry in the thread-local set. Request a pipeline state from the pending cache and add
            // it to the thread-local cache to reduce contention on the pending cache.
            {
                // Lazy-init the per-thread library on first access.
                if (!threadLibraryEntry.m_library)
                {
                    if (m_pipelineLibraryStrategy == PipelineLibraryStrategy::Global)
                    {
                        threadLibraryEntry.m_library = m_globalPipelineLibrary;
                    }
                    else
                    {
                        Ptr<PipelineLibrary> newPipelineLibrary = Factory::Get().CreatePipelineLibrary();
                        RHI::ResultCode resultCode =
                            newPipelineLibrary->Init(*m_device, globalLibraryEntry.m_pipelineLibraryDescriptor);
                        if (resultCode != RHI::ResultCode::Success)
                        {
                            AZ_Warning(
                                "PipelineStateCache",
                                false,
                                "Failed to initialize pipeline library. PipelineLibrary usage is disabled.");
                        }

                        // Store a valid pointer even if initialization failed to avoid retrying every access.
                        threadLibraryEntry.m_library = AZStd::move(newPipelineLibrary);
                    }
                }

                ConstPtr<PipelineState> pipelineState = AcquirePendingPipelineState(
                    globalLibraryEntry,
                    threadLibraryEntry,
                    descriptor,
                    pipelineStateHash,
                    name,
                    acquireFlags);

                [[maybe_unused]] bool success = InsertPipelineState(
                    threadLocalCache,
                    PipelineStateEntry(
                        pipelineStateHash,
                        pipelineState,
                        descriptor));
                AZ_Assert(success || noShare, "PipelineStateEntry already exists in the thread cache.");

                return pipelineState;
            }
        }
    }

    ConstPtr<PipelineState> PipelineStateCache::AcquirePendingPipelineState(
        GlobalLibraryEntry& globalLibraryEntry,
        ThreadLibraryEntry& threadLibraryEntry,
        const PipelineStateDescriptor& descriptor,
        PipelineStateHash pipelineStateHash,
        const AZ::Name& name,
        PipelineStateAcquireFlags acquireFlags)
    {
        ConstPtr<PipelineState> pipelineState;
        Ptr<PipelineState> pipelineStateToCompile;
        bool ownsSharedCompilation = false;
        const bool noShare = CheckBitsAny(acquireFlags, PipelineStateAcquireFlags::NoShare);

        if (!noShare)
        {
            AZStd::lock_guard<AZStd::mutex> lock(globalLibraryEntry.m_pendingCacheMutex);

            // Another thread may have started compiling this pipeline state. Check the pending cache.
            if (const PipelineState* cachedPipelineState = FindPipelineState(globalLibraryEntry.m_pendingCache, descriptor))
            {
                return cachedPipelineState;
            }
            else
            {
                pipelineStateToCompile = Factory::Get().CreatePipelineState();
                pipelineState = pipelineStateToCompile;
                // Shared callers publish the uninitialized object first, preserving the original cache contract.
                [[maybe_unused]] bool success = InsertPipelineState(
                    globalLibraryEntry.m_pendingCache,
                    PipelineStateEntry(pipelineStateHash, pipelineState, descriptor));
                AZ_Assert(success, "PipelineStateEntry already exists in the pending cache.");
                ownsSharedCompilation = true;
            }
        }
        else
        {
            // NoShare requests deliberately ignore pending and thread-local work and compile a private object.
            pipelineStateToCompile = Factory::Get().CreatePipelineState();
            pipelineState = pipelineStateToCompile;
        }

        AZ_Assert(pipelineStateToCompile, "Failed to create pipeline to compile");
        ResultCode resultCode = ResultCode::InvalidArgument;

        // Increment the pending compile count on the global entry, which tracks how many pipeline states
        // are currently being compiled across all threads.
        if (Validation::IsEnabled())
        {
            ++globalLibraryEntry.m_pendingCompileCount;
        }

        PipelineLibrary* pipelineLibrary = threadLibraryEntry.m_library.get();
        if (pipelineLibrary && !pipelineLibrary->IsInitialized())
        {
            pipelineLibrary = nullptr;
        }

        // We no longer have the pending-cache lock, but we own compilation of the pipeline state.
        switch (descriptor.GetType())
        {
        case PipelineStateType::Draw:
            resultCode = pipelineStateToCompile->Init(
                *m_device, static_cast<const PipelineStateDescriptorForDraw&>(descriptor), pipelineLibrary);
            break;

        case PipelineStateType::Dispatch:
            resultCode = pipelineStateToCompile->Init(
                *m_device, static_cast<const PipelineStateDescriptorForDispatch&>(descriptor), pipelineLibrary);
            break;

        case PipelineStateType::RayTracing:
            resultCode = pipelineStateToCompile->Init(
                *m_device, static_cast<const PipelineStateDescriptorForRayTracing&>(descriptor), pipelineLibrary);
            break;

        default:
            AZ_Assert(false, "Invalid pipeline state descriptor type specified.");
        }

        pipelineStateToCompile->SetName(name);

        if (Validation::IsEnabled())
        {
            --globalLibraryEntry.m_pendingCompileCount;
        }

        AZ_Error("PipelineStateCache", resultCode == ResultCode::Success, "Failed to compile pipeline state. It will remain uninitialized.");
        if (!ownsSharedCompilation)
        {
            if (resultCode != ResultCode::Success)
            {
                return nullptr;
            }

            // Publish an independently compiled pipeline only after it is ready. If another compilation is pending or
            // already succeeded, that entry remains canonical and this strong result stays local to the caller thread.
            AZStd::lock_guard<AZStd::mutex> lock(globalLibraryEntry.m_pendingCacheMutex);
            InsertPipelineState(
                globalLibraryEntry.m_pendingCache,
                PipelineStateEntry(pipelineStateHash, pipelineState, descriptor));
        }
        return pipelineState;
    }

#else
    const PipelineState* PipelineStateCache::AcquirePipelineState(
        PipelineLibraryHandle handle, const PipelineStateDescriptor& descriptor, const AZ::Name& name /*= AZ::Name()*/)
    {
        if (handle.IsNull())
        {
            return nullptr;
        }

        AZStd::shared_lock<AZStd::shared_mutex> lock(m_mutex);

        GlobalLibraryEntry& globalLibraryEntry = m_globalLibrarySet[handle.GetIndex()];
        PipelineStateHash pipelineStateHash = descriptor.GetHash();

        // Search the read-only cache first.
        if (const PipelineState* pipelineState = FindPipelineState(globalLibraryEntry.m_readOnlyCache, descriptor))
        {
            return pipelineState;
        }

        // Search the thread-local cache next.
        {
            ThreadLibrarySet& threadLibrarySet = m_threadLibrarySet.GetStorage();
            ThreadLibraryEntry& threadLibraryEntry = threadLibrarySet[handle.GetIndex()];
            PipelineStateSet& threadLocalCache = threadLibraryEntry.m_threadLocalCache;

            if (const PipelineState* pipelineState = FindPipelineState(threadLocalCache, descriptor))
            {
                return pipelineState;
            }

            // No entry in the thread-local set. Request a pipeline state from the pending cache and add
            // it to the thread-local cache to reduce contention on the pending cache.
            {
                // Lazy-init the library on first access.
                if (!threadLibraryEntry.m_library)
                {
                    Ptr<PipelineLibrary> pipelineLibrary = Factory::Get().CreatePipelineLibrary();
                    RHI::ResultCode resultCode = pipelineLibrary->Init(*m_device, globalLibraryEntry.m_pipelineLibraryDescriptor);
                    if (resultCode != RHI::ResultCode::Success)
                    {
                        AZ_Warning("PipelineStateCache", false, "Failed to initialize pipeline library. PipelineLibrary usage is disabled.");
                    }

                    // We store a valid pointer even if initialization failed, to avoid attempting
                    // to re-create it with every access.
                    threadLibraryEntry.m_library = AZStd::move(pipelineLibrary);
                }

                ConstPtr<PipelineState> pipelineState = CompilePipelineState(globalLibraryEntry, threadLibraryEntry, descriptor, pipelineStateHash, name);

                [[maybe_unused]] bool success = InsertPipelineState(threadLocalCache, PipelineStateEntry(pipelineStateHash, pipelineState, descriptor));
                AZ_Assert(success, "PipelineStateEntry already exists in the thread cache.");

                return pipelineState.get();
            }
        }
    }

    ConstPtr<PipelineState> PipelineStateCache::CompilePipelineState(
        GlobalLibraryEntry& globalLibraryEntry,
        ThreadLibraryEntry& threadLibraryEntry,
        const PipelineStateDescriptor& descriptor,
        PipelineStateHash pipelineStateHash,
        const AZ::Name& name)
    {
        Ptr<PipelineState> pipelineState;

        PipelineStateSet& pendingCache = globalLibraryEntry.m_pendingCache;

        {
            AZStd::lock_guard<AZStd::mutex> lock(globalLibraryEntry.m_pendingCacheMutex);

            // Another thread may have started compiling this pipeline state. Check the pending cache.
            if (const PipelineState* pipeline = FindPipelineState(pendingCache, descriptor))
            {
                return pipeline;
            }

            // We need to create and insert the pipeline state into the locked cache. Create the pipeline state
            // but don't initialize it yet. We can safely allocate the 'empty' instance and cache it.
            pipelineState = Factory::Get().CreatePipelineState();

            [[maybe_unused]] bool success = InsertPipelineState(pendingCache, PipelineStateEntry(pipelineStateHash, pipelineState, descriptor));
            AZ_Assert(success, "PipelineStateEntry already exists in the pending cache.");
        }

        [[maybe_unused]] ResultCode resultCode = ResultCode::InvalidArgument;

        // Increment the pending compile count on the global entry, which tracks how many pipeline states
        // are currently being compiled across all threads.
        if (Validation::IsEnabled())
        {
            ++globalLibraryEntry.m_pendingCompileCount;
        }

        // If the pipeline library failed to initialize, then we don't use it.
        PipelineLibrary* pipelineLibrary = threadLibraryEntry.m_library.get();
        if (!pipelineLibrary->IsInitialized())
        {
            pipelineLibrary = nullptr;
        }

        // We no longer have the lock, but we own compilation of the pipeline state. Use the
        // thread-local library to perform compilation without blocking other threads.
        switch (descriptor.GetType())
        {
        case PipelineStateType::Draw:
            resultCode = pipelineState->Init(*m_device, static_cast<const PipelineStateDescriptorForDraw&>(descriptor), pipelineLibrary);
            break;

        case PipelineStateType::Dispatch:
            resultCode = pipelineState->Init(*m_device, static_cast<const PipelineStateDescriptorForDispatch&>(descriptor), pipelineLibrary);
            break;

        case PipelineStateType::RayTracing:
            resultCode = pipelineState->Init(*m_device, static_cast<const PipelineStateDescriptorForRayTracing&>(descriptor), pipelineLibrary);
            break;

        default:
            AZ_Assert(false, "Invalid pipeline state descriptor type specified.");
        }

        pipelineState->SetName(name);

        if (Validation::IsEnabled())
        {
            --globalLibraryEntry.m_pendingCompileCount;
        }

        // NOTE: We can't return null on a failure, since other threads will return the entry without compiling
        // it. Instead, the pipeline state remains uninitialized.

        AZ_Error("PipelineStateCache", resultCode == ResultCode::Success, "Failed to compile pipeline state. It will remain in an initialized state.");
        return AZStd::move(pipelineState);
    }
#endif

    PipelineStateCache::PipelineStateEntry::PipelineStateEntry(PipelineStateHash hash, ConstPtr<PipelineState> pipelineState, const PipelineStateDescriptor& descriptor)
        : m_hash{ hash }
        , m_pipelineState{ AZStd::move(pipelineState) }
    {
        switch(descriptor.GetType())
        {
        case PipelineStateType::Dispatch:
            m_pipelineStateDescriptorVariant = static_cast<const AZ::RHI::PipelineStateDescriptorForDispatch&>(descriptor);
            break;

        case PipelineStateType::Draw:
            m_pipelineStateDescriptorVariant = static_cast<const AZ::RHI::PipelineStateDescriptorForDraw&>(descriptor);
            break;

        case PipelineStateType::RayTracing:
            m_pipelineStateDescriptorVariant = static_cast<const AZ::RHI::PipelineStateDescriptorForRayTracing&>(descriptor);
            break;
        }
    }

    bool PipelineStateCache::PipelineStateEntry::operator == (const PipelineStateCache::PipelineStateEntry& rhs) const
    {
        if(AZStd::get_if<AZ::RHI::PipelineStateDescriptorForDispatch>(&rhs.m_pipelineStateDescriptorVariant) &&
            AZStd::get_if<AZ::RHI::PipelineStateDescriptorForDispatch>(&m_pipelineStateDescriptorVariant))
        {
            const AZ::RHI::PipelineStateDescriptorForDispatch& lhsDesc = AZStd::get<PipelineStateDescriptorForDispatch>(m_pipelineStateDescriptorVariant);
            const AZ::RHI::PipelineStateDescriptorForDispatch& rhsDesc = AZStd::get<PipelineStateDescriptorForDispatch>(rhs.m_pipelineStateDescriptorVariant);

            return lhsDesc == rhsDesc;
        }
        else if(AZStd::get_if<AZ::RHI::PipelineStateDescriptorForDraw>(&rhs.m_pipelineStateDescriptorVariant) &&
            AZStd::get_if<AZ::RHI::PipelineStateDescriptorForDraw>(&m_pipelineStateDescriptorVariant))
        {
            const AZ::RHI::PipelineStateDescriptorForDraw& lhsDesc = AZStd::get<PipelineStateDescriptorForDraw>(m_pipelineStateDescriptorVariant);
            const AZ::RHI::PipelineStateDescriptorForDraw& rhsDesc = AZStd::get<PipelineStateDescriptorForDraw>(rhs.m_pipelineStateDescriptorVariant);

            return lhsDesc == rhsDesc;
        }
        else if(AZStd::get_if<AZ::RHI::PipelineStateDescriptorForRayTracing>(&rhs.m_pipelineStateDescriptorVariant) &&
            AZStd::get_if<AZ::RHI::PipelineStateDescriptorForRayTracing>(&m_pipelineStateDescriptorVariant))
        {
            const AZ::RHI::PipelineStateDescriptorForRayTracing& lhsDesc = AZStd::get<PipelineStateDescriptorForRayTracing>(m_pipelineStateDescriptorVariant);
            const AZ::RHI::PipelineStateDescriptorForRayTracing& rhsDesc = AZStd::get<PipelineStateDescriptorForRayTracing>(rhs.m_pipelineStateDescriptorVariant);

            return lhsDesc == rhsDesc;
        }

        return false;
    }
}
