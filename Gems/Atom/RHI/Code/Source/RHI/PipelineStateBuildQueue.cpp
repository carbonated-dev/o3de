/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RHI/PipelineStateBuildQueue.h>
#include <Atom/RHI/PipelineStateCache.h>

#include <AzCore/Debug/Profiler.h>

namespace AZ::RHI
{
    PipelineStateBuildRequest::PipelineStateBuildRequest(
        PipelineStateBuildGroupId groupId,
        PipelineLibraryHandle pipelineLibraryHandle,
        const PipelineStateDescriptor& descriptor,
        const Name& name)
        : m_groupId(groupId)
        , m_pipelineLibraryHandle(pipelineLibraryHandle)
        , m_name(name)
    {
        switch (descriptor.GetType())
        {
        case PipelineStateType::Draw:
            m_descriptor = static_cast<const PipelineStateDescriptorForDraw&>(descriptor);
            break;
        case PipelineStateType::Dispatch:
            m_descriptor = static_cast<const PipelineStateDescriptorForDispatch&>(descriptor);
            break;
        case PipelineStateType::RayTracing:
            m_descriptor = static_cast<const PipelineStateDescriptorForRayTracing&>(descriptor);
            break;
        default:
            AZ_Assert(false, "Invalid pipeline state descriptor type specified.");
            break;
        }
    }

    PipelineStateBuildRequest::State PipelineStateBuildRequest::GetState() const
    {
        AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
        return m_state;
    }

    bool PipelineStateBuildRequest::IsComplete() const
    {
        const State state = GetState();
        return state == State::Succeeded || state == State::Failed || state == State::Cancelled;
    }

    bool PipelineStateBuildRequest::UsesSpecializationConstants() const
    {
        return !GetDescriptor().m_specializationData.empty();
    }

    ConstPtr<PipelineState> PipelineStateBuildRequest::GetPipelineState() const
    {
        AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
        return m_pipelineState;
    }

    const PipelineStateDescriptor& PipelineStateBuildRequest::GetDescriptor() const
    {
        switch (m_descriptor.index())
        {
        case 0:
            return AZStd::get<PipelineStateDescriptorForDraw>(m_descriptor);
        case 1:
            return AZStd::get<PipelineStateDescriptorForDispatch>(m_descriptor);
        case 2:
            return AZStd::get<PipelineStateDescriptorForRayTracing>(m_descriptor);
        default:
            AZ_Assert(false, "Invalid pipeline state descriptor variant.");
            return AZStd::get<PipelineStateDescriptorForDraw>(m_descriptor);
        }
    }

    PipelineStateBuildQueue::~PipelineStateBuildQueue()
    {
        Shutdown();
    }

    void PipelineStateBuildQueue::Init(Ptr<PipelineStateCache> pipelineStateCache)
    {
        AZ_Assert(pipelineStateCache, "PipelineStateBuildQueue requires a pipeline state cache.");
        if (m_isInitialized)
        {
            return;
        }

        m_pipelineStateCache = AZStd::move(pipelineStateCache);
        m_isShutdown = false;
        AZStd::thread_desc threadDesc{ "PipelineStateBuildQueue" };
        m_serviceThread = AZStd::thread(threadDesc, [this]()
        {
            ThreadServiceLoop();
        });
        m_isInitialized = true;
    }

    void PipelineStateBuildQueue::Shutdown()
    {
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
            if (!m_isInitialized)
            {
                return;
            }

            m_isShutdown = true;
            for (const PipelineStateBuildRequestPtr& request : m_pendingRequests)
            {
                AZStd::lock_guard<AZStd::mutex> requestLock(request->m_mutex);
                request->m_cancelRequested = true;
                request->m_state = PipelineStateBuildRequest::State::Cancelled;
            }
            m_pendingRequests.clear();

            if (m_activeRequest)
            {
                AZStd::lock_guard<AZStd::mutex> requestLock(m_activeRequest->m_mutex);
                m_activeRequest->m_cancelRequested = true;
            }
        }
        m_workCondition.notify_all();

        if (m_serviceThread.joinable())
        {
            m_serviceThread.join();
        }

        {
            AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
            m_completedRequestsByGroup.clear();
            m_activeGroups.clear();
            m_activeRequest.reset();
            m_pipelineStateCache = nullptr;
            m_serviceThread = AZStd::thread();
            m_isInitialized = false;
        }
    }

    PipelineStateBuildGroupId PipelineStateBuildQueue::CreateRequestGroup()
    {
        AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
        if (!m_isInitialized || m_isShutdown)
        {
            return {};
        }

        const PipelineStateBuildGroupId groupId{ m_nextGroupId++ };
        m_activeGroups.insert(groupId);
        return groupId;
    }

    void PipelineStateBuildQueue::ReleaseRequestGroup(PipelineStateBuildGroupId groupId)
    {
        if (groupId.IsNull())
        {
            return;
        }

        AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
        if (m_activeGroups.erase(groupId) == 0)
        {
            return;
        }

        for (auto pendingIt = m_pendingRequests.begin(); pendingIt != m_pendingRequests.end();)
        {
            const PipelineStateBuildRequestPtr& request = *pendingIt;
            if (request->m_groupId == groupId)
            {
                AZStd::lock_guard<AZStd::mutex> requestLock(request->m_mutex);
                request->m_cancelRequested = true;
                request->m_state = PipelineStateBuildRequest::State::Cancelled;
                pendingIt = m_pendingRequests.erase(pendingIt);
            }
            else
            {
                ++pendingIt;
            }
        }

        if (m_activeRequest && m_activeRequest->m_groupId == groupId)
        {
            AZStd::lock_guard<AZStd::mutex> requestLock(m_activeRequest->m_mutex);
            m_activeRequest->m_cancelRequested = true;
        }

        auto completedGroupIt = m_completedRequestsByGroup.find(groupId);
        if (completedGroupIt != m_completedRequestsByGroup.end())
        {
            for (const PipelineStateBuildRequestPtr& request : completedGroupIt->second)
            {
                AZStd::lock_guard<AZStd::mutex> requestLock(request->m_mutex);
                request->m_cancelRequested = true;
                request->m_state = PipelineStateBuildRequest::State::Cancelled;
                request->m_pipelineState = nullptr;
            }
            m_completedRequestsByGroup.erase(completedGroupIt);
        }
    }

    PipelineStateBuildRequestPtr PipelineStateBuildQueue::QueuePipelineStateBuild(
        PipelineStateBuildGroupId groupId,
        PipelineLibraryHandle pipelineLibraryHandle,
        const PipelineStateDescriptor& descriptor,
        const Name& name)
    {
        PipelineStateBuildRequestPtr request(
            aznew PipelineStateBuildRequest(groupId, pipelineLibraryHandle, descriptor, name));
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
            if (!m_isInitialized || m_isShutdown || !m_activeGroups.contains(groupId))
            {
                AZStd::lock_guard<AZStd::mutex> requestLock(request->m_mutex);
                request->m_cancelRequested = true;
                request->m_state = PipelineStateBuildRequest::State::Cancelled;
                return request;
            }
            m_pendingRequests.push_back(request);
        }
        m_workCondition.notify_one();
        return request;
    }

    void PipelineStateBuildQueue::Cancel(const PipelineStateBuildRequestPtr& request)
    {
        if (!request)
        {
            return;
        }

        // Completed requests have already left the queue and no longer need cancellation. This is also
        // important for completed requests shared by many MeshDrawPackets: releasing the final owner
        // must not perform linear searches through the pending and completed request containers.
        if (request->IsComplete())
        {
            return;
        }

        AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
        {
            AZStd::lock_guard<AZStd::mutex> requestLock(request->m_mutex);
            if (request->m_state == PipelineStateBuildRequest::State::Succeeded ||
                request->m_state == PipelineStateBuildRequest::State::Failed ||
                request->m_state == PipelineStateBuildRequest::State::Cancelled)
            {
                return;
            }

            request->m_cancelRequested = true;
            request->m_state = PipelineStateBuildRequest::State::Cancelled;
            request->m_pipelineState = nullptr;
        }

        for (auto pendingIt = m_pendingRequests.begin(); pendingIt != m_pendingRequests.end(); ++pendingIt)
        {
            if (*pendingIt == request)
            {
                m_pendingRequests.erase(pendingIt);
                break;
            }
        }

        auto completedGroupIt = m_completedRequestsByGroup.find(request->m_groupId);
        if (completedGroupIt != m_completedRequestsByGroup.end())
        {
            PipelineStateBuildRequestList& completedRequests = completedGroupIt->second;
            for (auto completedIt = completedRequests.begin(); completedIt != completedRequests.end(); ++completedIt)
            {
                if (*completedIt == request)
                {
                    completedRequests.erase(completedIt);
                    break;
                }
            }
            if (completedRequests.empty())
            {
                m_completedRequestsByGroup.erase(completedGroupIt);
            }
        }
    }

    PipelineStateBuildRequestList PipelineStateBuildQueue::TakeCompletedRequests(PipelineStateBuildGroupId groupId)
    {
        AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
        auto completedGroupIt = m_completedRequestsByGroup.find(groupId);
        if (completedGroupIt == m_completedRequestsByGroup.end())
        {
            return {};
        }

        PipelineStateBuildRequestList completedRequests = AZStd::move(completedGroupIt->second);
        m_completedRequestsByGroup.erase(completedGroupIt);
        return completedRequests;
    }

    void PipelineStateBuildQueue::ThreadServiceLoop()
    {
        for (;;)
        {
            PipelineStateBuildRequestPtr request;
            {
                AZStd::unique_lock<AZStd::mutex> lock(m_mutex);
                m_workCondition.wait(lock, [this]()
                {
                    return m_isShutdown || !m_pendingRequests.empty();
                });

                if (m_isShutdown)
                {
                    return;
                }

                request = AZStd::move(m_pendingRequests.front());
                m_pendingRequests.pop_front();
                m_activeRequest = request;

                AZStd::lock_guard<AZStd::mutex> requestLock(request->m_mutex);
                if (request->m_cancelRequested)
                {
                    request->m_state = PipelineStateBuildRequest::State::Cancelled;
                    m_activeRequest.reset();
                    continue;
                }
                request->m_state = PipelineStateBuildRequest::State::Building;
            }

            AZ_PROFILE_SCOPE(RHI, "PipelineStateBuildQueue: Compile");
            ConstPtr<PipelineState> pipelineState = m_pipelineStateCache->AcquirePipelineState(
                request->m_pipelineLibraryHandle,
                request->GetDescriptor(),
                PipelineStateAcquireFlags::NoShare | PipelineStateAcquireFlags::ThreadLocalCache,
                request->m_name);

            {
                AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
                AZStd::lock_guard<AZStd::mutex> requestLock(request->m_mutex);
                if (request->m_cancelRequested || m_isShutdown)
                {
                    request->m_state = PipelineStateBuildRequest::State::Cancelled;
                    request->m_pipelineState = nullptr;
                }
                else
                {
                    const bool compilationSucceeded = pipelineState != nullptr;
                    request->m_pipelineState = AZStd::move(pipelineState);
                    request->m_state = compilationSucceeded
                        ? PipelineStateBuildRequest::State::Succeeded
                        : PipelineStateBuildRequest::State::Failed;
                    m_completedRequestsByGroup[request->m_groupId].push_back(request);
                }
                m_activeRequest.reset();
            }
        }
    }
} // namespace AZ::RHI
