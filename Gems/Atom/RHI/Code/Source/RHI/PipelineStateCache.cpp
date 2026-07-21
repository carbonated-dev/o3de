/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RHI/PipelineStateCache.h>
#include <Atom/RHI/Factory.h>

#include <AzCore/Console/IConsole.h>
#include <AzCore/Debug/Profiler.h>
#include <AzCore/std/sort.h>
#include <AzCore/std/parallel/exponential_backoff.h>
#include <AzCore/std/smart_ptr/make_shared.h>

#if defined(CARBONATED)
#include <AzCore/Memory/MemoryMarker.h>
#endif

AZ_CVAR(
    AZ::u32,
    r_pipelineLibraryStrategy,
    static_cast<AZ::u32>(AZ::RHI::PipelineLibraryStrategy::Global),
    nullptr,
    AZ::ConsoleFunctorFlags::NeedsReload,
    "Pipeline library strategy. 0: one library per thread and shader; 1: one global library per device. "
    "The value is captured when PipelineStateCache is created and changing it requires a restart.");

namespace AZ::RHI
{
    namespace
    {
        PipelineLibraryStrategy GetPipelineLibraryStrategyFromCVar()
        {
            switch (r_pipelineLibraryStrategy)
            {
            case static_cast<AZ::u32>(PipelineLibraryStrategy::PerThreadPerShader):
                return PipelineLibraryStrategy::PerThreadPerShader;
            case static_cast<AZ::u32>(PipelineLibraryStrategy::Global):
                return PipelineLibraryStrategy::Global;
            default:
                AZ_Warning(
                    "PipelineStateCache",
                    false,
                    "Invalid r_pipelineLibraryStrategy value %u. Falling back to PerThreadPerShader.",
                    static_cast<AZ::u32>(r_pipelineLibraryStrategy));
                return PipelineLibraryStrategy::PerThreadPerShader;
            }
        }
    }

    Ptr<PipelineStateCache> PipelineStateCache::Create(Device& device)
    {
        return aznew PipelineStateCache(device);
    }

    PipelineStateCache::PipelineStateCache(Device& device)
        : m_device{&device}
        , m_pipelineLibraryStrategy{GetPipelineLibraryStrategyFromCVar()}
    {}

    PipelineLibraryStrategy PipelineStateCache::GetPipelineLibraryStrategy() const
    {
        return m_pipelineLibraryStrategy;
    }

    bool PipelineStateCache::NeedsPipelineLibraryData() const
    {
        AZStd::shared_lock<AZStd::shared_mutex> lock(m_mutex);
        return m_pipelineLibraryStrategy == PipelineLibraryStrategy::PerThreadPerShader || !m_globalPipelineLibrary;
    }

    bool PipelineStateCache::ShouldSavePipelineLibrary(PipelineLibraryHandle handle) const
    {
        if (handle.IsNull())
        {
            return false;
        }

        AZStd::shared_lock<AZStd::shared_mutex> lock(m_mutex);
        if (!m_globalLibraryActiveBits[handle.GetIndex()])
        {
            return false;
        }

        return m_pipelineLibraryStrategy == PipelineLibraryStrategy::PerThreadPerShader ||
            m_globalLibraryActiveBits.count() == 1;
    }

    PipelineStateCache::PipelineStateCompileState::PipelineStateCompileState(bool isAsyncCompile)
        : m_isAsyncCompile(isAsyncCompile)
    {}

    void PipelineStateCache::PipelineStateCompileState::SetCompleted(bool succeeded)
    {
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
            m_succeeded = succeeded;
            m_isComplete = true;
        }
        m_condition.notify_all();
    }

    bool PipelineStateCache::PipelineStateCompileState::WaitForCompletion()
    {
        AZStd::unique_lock<AZStd::mutex> lock(m_mutex);
        m_condition.wait(lock, [this]()
        {
            return m_isComplete;
        });
        return m_succeeded;
    }

    bool PipelineStateCache::PipelineStateCompileState::IsSuccessful()
    {
        AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
        AZ_Assert(m_isComplete, "A pipeline state was compacted before compilation completed.");
        return m_succeeded;
    }

    bool PipelineStateCache::PipelineStateCompileState::IsCompleteAndSuccessful()
    {
        AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
        return m_isComplete && m_succeeded;
    }

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

        if (m_pipelineLibraryStrategy == PipelineLibraryStrategy::Global && !m_globalPipelineLibrary)
        {
            Ptr<PipelineLibrary> pipelineLibrary = Factory::Get().CreatePipelineLibrary();
            const RHI::ResultCode resultCode = pipelineLibrary->Init(*m_device, libraryEntry.m_pipelineLibraryDescriptor);
            if (resultCode != RHI::ResultCode::Success)
            {
                AZ_Warning(
                    "PipelineStateCache",
                    false,
                    "Failed to initialize the global pipeline library. PipelineLibrary usage is disabled.");
            }

            // Keep the object even if initialization failed so initialization is only attempted once.
            m_globalPipelineLibrary = AZStd::move(pipelineLibrary);
            m_globalPipelineLibraryHasData = serializedData != nullptr;
        }

        return handle;
    }

    void PipelineStateCache::ReleaseLibrary(PipelineLibraryHandle handle)
    {
        if (handle.IsValid())
        {
            AZStd::unique_lock<AZStd::shared_mutex> lock(m_mutex);
            AZ_Assert(m_globalLibraryActiveBits[handle.GetIndex()], "Releasing a library that is no longer valid.");

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

        AZStd::unique_lock<AZStd::shared_mutex> lock(m_mutex);
        const GlobalLibraryEntry& entry = m_globalLibrarySet[handle.GetIndex()];

        if (m_pipelineLibraryStrategy == PipelineLibraryStrategy::Global)
        {
            if (m_globalPipelineLibrary &&
                m_globalPipelineLibrary->IsInitialized() &&
                m_globalPipelineLibraryHasData)
            {
                return m_globalPipelineLibrary;
            }
            return nullptr;
        }

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

    void PipelineStateCache::Compact()
    {
        AZ_PROFILE_SCOPE(RHI, "PipelineStateCache: Compact");
        AZStd::unique_lock<AZStd::shared_mutex> lock(m_mutex, AZStd::try_to_lock);
        if (!lock.owns_lock())
        {
            return;
        }

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

                mergeResult.insert(globalLibraryEntry.m_readOnlyCache.begin(), globalLibraryEntry.m_readOnlyCache.end());
                for (const PipelineStateEntry& pendingEntry : globalLibraryEntry.m_pendingCache)
                {
                    if (pendingEntry.m_compileState &&
                        pendingEntry.m_compileState->m_isAsyncCompile &&
                        !pendingEntry.m_compileState->IsSuccessful())
                    {
                        continue;
                    }

                    PipelineStateEntry completedEntry = pendingEntry;
                    completedEntry.m_compileState = nullptr;
                    mergeResult.insert(AZStd::move(completedEntry));
                }

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

    const PipelineStateCache::PipelineStateEntry* PipelineStateCache::FindPipelineStateEntry(
        const PipelineStateSet& pipelineStateSet, const PipelineStateDescriptor& descriptor)
    {
        auto pipelineStateIt = pipelineStateSet.find(PipelineStateEntry(descriptor.GetHash(), nullptr, descriptor));
        if (pipelineStateIt != pipelineStateSet.end())
        {
            return &*pipelineStateIt;
        }
        return nullptr;
    }

    bool PipelineStateCache::InsertPipelineState(PipelineStateSet& pipelineStateSet, PipelineStateEntry pipelineStateEntry)
    {
        auto ret = pipelineStateSet.insert(pipelineStateEntry);
        return ret.second;
    }

    const PipelineState* PipelineStateCache::AcquirePipelineState(
        PipelineLibraryHandle handle,
        const PipelineStateDescriptor& descriptor,
        const AZ::Name& name /*= AZ::Name()*/,
        bool acquireOnlyIfCached /*= false*/)
    {
        return AcquirePipelineStateInternal(handle, descriptor, name, false, acquireOnlyIfCached);
    }

    const PipelineState* PipelineStateCache::AcquirePipelineStateAsync(
        PipelineLibraryHandle handle, const PipelineStateDescriptor& descriptor, const AZ::Name& name /*= AZ::Name()*/)
    {
        return AcquirePipelineStateInternal(handle, descriptor, name, true, false);
    }

    const PipelineState* PipelineStateCache::AcquirePipelineStateInternal(
        PipelineLibraryHandle handle,
        const PipelineStateDescriptor& descriptor,
        const AZ::Name& name,
        bool isAsyncAcquire,
        bool acquireOnlyIfCached)
    {
        if (handle.IsNull())
        {
            return nullptr;
        }
#if defined(CARBONATED)
        MEMORY_TAG(Shader);
#endif        

        AZStd::shared_lock<AZStd::shared_mutex> lock(m_mutex);

        GlobalLibraryEntry& globalLibraryEntry = m_globalLibrarySet[handle.GetIndex()];
        PipelineStateHash pipelineStateHash = descriptor.GetHash();

        // Search the read-only cache first.
        if (const PipelineStateEntry* pipelineStateEntry = FindPipelineStateEntry(globalLibraryEntry.m_readOnlyCache, descriptor))
        {
            return ResolvePipelineStateEntry(*pipelineStateEntry, isAsyncAcquire, acquireOnlyIfCached);
        }

        // Search the thread-local cache next.
        {
            ThreadLibrarySet& threadLibrarySet = m_threadLibrarySet.GetStorage();
            ThreadLibraryEntry& threadLibraryEntry = threadLibrarySet[handle.GetIndex()];
            PipelineStateSet& threadLocalCache = threadLibraryEntry.m_threadLocalCache;

            if (const PipelineStateEntry* pipelineStateEntry = FindPipelineStateEntry(threadLocalCache, descriptor))
            {
                return ResolvePipelineStateEntry(*pipelineStateEntry, isAsyncAcquire, acquireOnlyIfCached);
            }

            if (acquireOnlyIfCached)
            {
                // A completed PSO may still be in the pending cache until the next Compact().
                // Check it without allocating a thread-local library or starting compilation.
                AZStd::lock_guard<AZStd::mutex> pendingCacheLock(globalLibraryEntry.m_pendingCacheMutex);
                if (const PipelineStateEntry* pipelineStateEntry =
                    FindPipelineStateEntry(globalLibraryEntry.m_pendingCache, descriptor))
                {
                    return ResolvePipelineStateEntry(*pipelineStateEntry, false, true);
                }
                return nullptr;
            }

            // No entry in the thread-local set. Request a pipeline state from the pending cache and add
            // it to the thread-local cache to reduce contention on the pending cache.
            {
                PipelineLibrary* pipelineLibrary = nullptr;
                if (m_pipelineLibraryStrategy == PipelineLibraryStrategy::Global)
                {
                    if (m_globalPipelineLibrary && m_globalPipelineLibrary->IsInitialized())
                    {
                        pipelineLibrary = m_globalPipelineLibrary.get();
                    }
                }
                else
                {
                    // Lazy-init the per-thread library on first access.
                    if (!threadLibraryEntry.m_library)
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

                    if (threadLibraryEntry.m_library->IsInitialized())
                    {
                        pipelineLibrary = threadLibraryEntry.m_library.get();
                    }
                }

                PipelineStateAcquireResult acquireResult =
                    CompilePipelineState(
                        globalLibraryEntry, pipelineLibrary, descriptor, pipelineStateHash, name, isAsyncAcquire);

                if (acquireResult.m_pipelineState)
                {
                    [[maybe_unused]] bool success = InsertPipelineState(
                        threadLocalCache,
                        PipelineStateEntry(
                            pipelineStateHash,
                            acquireResult.m_pipelineState,
                            descriptor,
                            acquireResult.m_compileState));
                    AZ_Assert(success, "PipelineStateEntry already exists in the thread cache.");
                }

                return acquireResult.m_pipelineState.get();
            }
        }
    }

    const PipelineState* PipelineStateCache::ResolvePipelineStateEntry(
        const PipelineStateEntry& pipelineStateEntry,
        bool isAsyncAcquire,
        bool acquireOnlyIfCached)
    {
        if (acquireOnlyIfCached && pipelineStateEntry.m_compileState &&
            !pipelineStateEntry.m_compileState->IsCompleteAndSuccessful())
        {
            return nullptr;
        }

        if (pipelineStateEntry.m_compileState &&
            (isAsyncAcquire || pipelineStateEntry.m_compileState->m_isAsyncCompile) &&
            !pipelineStateEntry.m_compileState->WaitForCompletion())
        {
            return nullptr;
        }

        return pipelineStateEntry.m_pipelineState.get();
    }

    PipelineStateCache::PipelineStateAcquireResult PipelineStateCache::CompilePipelineState(
        GlobalLibraryEntry& globalLibraryEntry,
        PipelineLibrary* pipelineLibrary,
        const PipelineStateDescriptor& descriptor,
        PipelineStateHash pipelineStateHash,
        const AZ::Name& name,
        bool isAsyncAcquire)
    {
        PipelineStateAcquireResult acquireResult;
        Ptr<PipelineState> pipelineStateToCompile;
        bool ownsCompilation = false;

        {
            AZStd::lock_guard<AZStd::mutex> lock(globalLibraryEntry.m_pendingCacheMutex);

            // Another thread may have started compiling this pipeline state. Check the pending cache.
            if (const PipelineStateEntry* pipelineStateEntry =
                FindPipelineStateEntry(globalLibraryEntry.m_pendingCache, descriptor))
            {
                acquireResult.m_pipelineState = pipelineStateEntry->m_pipelineState;
                acquireResult.m_compileState = pipelineStateEntry->m_compileState;
            }
            else
            {
                // We need to create and insert the pipeline state into the locked cache. Create the pipeline state
                // but don't initialize it yet. We can safely allocate the 'empty' instance and cache it.
                pipelineStateToCompile = Factory::Get().CreatePipelineState();
                acquireResult.m_pipelineState = pipelineStateToCompile;
                acquireResult.m_compileState = AZStd::make_shared<PipelineStateCompileState>(isAsyncAcquire);

                [[maybe_unused]] bool success = InsertPipelineState(
                    globalLibraryEntry.m_pendingCache,
                    PipelineStateEntry(
                        pipelineStateHash,
                        acquireResult.m_pipelineState,
                        descriptor,
                        acquireResult.m_compileState));
                AZ_Assert(success, "PipelineStateEntry already exists in the pending cache.");
                ownsCompilation = true;
            }
        }

        if (!ownsCompilation)
        {
            if (acquireResult.m_compileState &&
                (isAsyncAcquire || acquireResult.m_compileState->m_isAsyncCompile) &&
                !acquireResult.m_compileState->WaitForCompletion())
            {
                acquireResult.m_pipelineState = nullptr;
            }
            return acquireResult;
        }

        [[maybe_unused]] ResultCode resultCode = ResultCode::InvalidArgument;

        // Increment the pending compile count on the global entry, which tracks how many pipeline states
        // are currently being compiled across all threads.
        if (Validation::IsEnabled())
        {
            ++globalLibraryEntry.m_pendingCompileCount;
        }

        // We no longer have the pending-cache lock, but we own compilation of this pipeline state.
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

        if (resultCode == ResultCode::Success && m_pipelineLibraryStrategy == PipelineLibraryStrategy::Global)
        {
            m_globalPipelineLibraryHasData = true;
        }

        if (Validation::IsEnabled())
        {
            --globalLibraryEntry.m_pendingCompileCount;
        }

        acquireResult.m_compileState->SetCompleted(resultCode == ResultCode::Success);

        AZ_Error("PipelineStateCache", resultCode == ResultCode::Success, "Failed to compile pipeline state. It will remain in an initialized state.");
        if (isAsyncAcquire && resultCode != ResultCode::Success)
        {
            acquireResult.m_pipelineState = nullptr;
        }
        return acquireResult;
    }

    PipelineStateCache::PipelineStateEntry::PipelineStateEntry(
        PipelineStateHash hash,
        ConstPtr<PipelineState> pipelineState,
        const PipelineStateDescriptor& descriptor,
        PipelineStateCompileStatePtr compileState)
        : m_hash{ hash }
        , m_pipelineState{ AZStd::move(pipelineState) }
        , m_compileState{ AZStd::move(compileState) }
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
