/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RPI.Public/PipelineStateBuildQueue.h>
#include <Atom/RPI.Public/RPISystemInterface.h>
#include <AzCore/Debug/Profiler.h>

namespace AZ::RPI
{
    PipelineStateBuildRequest::PipelineStateBuildRequest(
        PipelineStateBuildItemList pipelineStateBuildItems)
        : m_pipelineStateBuildItems(AZStd::move(pipelineStateBuildItems))
    {
    }

    bool PipelineStateBuildRequest::TryBeginBuild()
    {
        State expectedState = State::Pending;
        return m_state.compare_exchange_strong(
            expectedState,
            State::Building,
            AZStd::memory_order_acq_rel,
            AZStd::memory_order_acquire);
    }

    void PipelineStateBuildRequest::Build()
    {
        if (TryBeginBuild())
        {
            BuildInternal(false);
        }
        else if (GetState() == State::Building)
        {
            Wait();
        }
    }

    void PipelineStateBuildRequest::Queue()
    {
        RPISystemInterface* rpiSystem = RPISystemInterface::Get();
        if (!rpiSystem)
        {
            AZ_Error("PipelineStateBuildQueue", false, "Cannot queue a pipeline-state build without an RPI system.");
            Cancel();
            return;
        }

        rpiSystem->GetPipelineStateBuildQueue()->Queue(shared_from_this());
    }

    void PipelineStateBuildRequest::Cancel()
    {
        m_cancelRequested.store(true, AZStd::memory_order_release);

        State expectedState = State::Pending;
        if (m_state.compare_exchange_strong(
                expectedState,
                State::Cancelled,
                AZStd::memory_order_acq_rel,
                AZStd::memory_order_acquire))
        {
            m_completionCondition.notify_all();
        }
    }

    void PipelineStateBuildRequest::Wait()
    {
        AZStd::unique_lock<AZStd::mutex> lock(m_completionMutex);
        m_completionCondition.wait(
            lock,
            [this]()
            {
                return IsComplete();
            });
    }

    PipelineStateBuildRequest::State PipelineStateBuildRequest::GetState() const
    {
        return m_state.load(AZStd::memory_order_acquire);
    }

    bool PipelineStateBuildRequest::IsComplete() const
    {
        const State state = GetState();
        return state == State::Succeeded ||
            state == State::Failed ||
            state == State::Cancelled;
    }

    bool PipelineStateBuildRequest::IsSuccessful() const
    {
        return GetState() == State::Succeeded;
    }

    AZStd::span<const RHI::ConstPtr<RHI::PipelineState>>
    PipelineStateBuildRequest::GetPipelineStates() const
    {
        AZ_Assert(IsSuccessful(), "Pipeline states are only available after a successful build.");
        return m_pipelineStates;
    }

    PipelineStateBuildItemList PipelineStateBuildRequest::TakeBuildItems()
    {
        AZ_Assert(IsSuccessful(), "Pipeline-state build items are only available after a successful build.");
        return AZStd::move(m_pipelineStateBuildItems);
    }

    void PipelineStateBuildRequest::BuildInternal(bool useAsyncCacheAcquire)
    {
        AZ_PROFILE_SCOPE(
            RPI,
            "PipelineStateBuildRequest::Build Count=%zu Async=%d",
            m_pipelineStateBuildItems.size(),
            static_cast<int>(useAsyncCacheAcquire));

        m_pipelineStates.clear();
        m_pipelineStates.reserve(m_pipelineStateBuildItems.size());

        for (const PipelineStateBuildItem& buildItem : m_pipelineStateBuildItems)
        {
            if (m_cancelRequested.load(AZStd::memory_order_acquire))
            {
                m_pipelineStates.clear();
                SetTerminalState(State::Cancelled);
                return;
            }

            const RHI::PipelineState* pipelineState = nullptr;
            {
                AZ_PROFILE_SCOPE(
                    RPI,
                    "PipelineStateBuildRequest::AcquirePipelineState Shader=%s PSOHash=0x%llx Async=%d",
                    buildItem.m_shader->GetAsset()->GetName().GetCStr(),
                    static_cast<unsigned long long>(
                        static_cast<uint64_t>(buildItem.m_descriptor.GetHash())),
                    static_cast<int>(useAsyncCacheAcquire));
                pipelineState =
                    useAsyncCacheAcquire
                    ? buildItem.m_shader->AcquirePipelineStateAsync(
                        buildItem.m_descriptor)
                    : buildItem.m_shader->AcquirePipelineState(
                        buildItem.m_descriptor);
            }
            if (!pipelineState)
            {
                AZ_Error(
                    "PipelineStateBuildQueue",
                    false,
                    "Shader '%s'. Failed to acquire pipeline state",
                    buildItem.m_shader->GetAsset()->GetName().GetCStr());
                m_pipelineStates.clear();
                SetTerminalState(State::Failed);
                return;
            }
            m_pipelineStates.emplace_back(pipelineState);
        }

        SetTerminalState(
            m_cancelRequested.load(AZStd::memory_order_acquire)
            ? State::Cancelled
            : State::Succeeded);
    }

    void PipelineStateBuildRequest::SetTerminalState(State state)
    {
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_completionMutex);
            m_state.store(state, AZStd::memory_order_release);
        }
        m_completionCondition.notify_all();
    }

    PipelineStateBuildQueue::~PipelineStateBuildQueue()
    {
        Shutdown();
    }

    void PipelineStateBuildQueue::Init()
    {
        if (!m_isServiceShutdown.exchange(false))
        {
            return;
        }

        AZStd::thread_desc threadDesc;
        threadDesc.m_name = "PipelineStateBuildQueue";
        m_serviceThread = AZStd::thread(
            threadDesc,
            [this]()
            {
                ThreadServiceLoop();
            });
    }

    void PipelineStateBuildQueue::Shutdown()
    {
        if (m_isServiceShutdown.exchange(true))
        {
            return;
        }

        AZStd::deque<PipelineStateBuildRequestPtr> pendingRequests;
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
            pendingRequests.swap(m_pendingRequests);
        }

        for (const auto& request : pendingRequests)
        {
            request->m_cancelRequested.store(true, AZStd::memory_order_release);
            request->SetTerminalState(PipelineStateBuildRequest::State::Cancelled);
        }

        m_workCondition.notify_one();
        if (m_serviceThread.joinable())
        {
            m_serviceThread.join();
        }
    }

    void PipelineStateBuildQueue::Queue(
        PipelineStateBuildRequestPtr request)
    {
        if (!request || !request->TryBeginBuild())
        {
            return;
        }

        {
            AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
            if (m_isServiceShutdown.load(AZStd::memory_order_acquire))
            {
                request->SetTerminalState(
                    PipelineStateBuildRequest::State::Cancelled);
                return;
            }
            m_pendingRequests.emplace_back(AZStd::move(request));
        }
        m_workCondition.notify_one();
    }

    void PipelineStateBuildQueue::ThreadServiceLoop()
    {
        while (true)
        {
            PipelineStateBuildRequestPtr request;
            {
                AZStd::unique_lock<AZStd::mutex> lock(m_mutex);
                m_workCondition.wait(
                    lock,
                    [this]()
                    {
                        return m_isServiceShutdown.load(AZStd::memory_order_acquire) ||
                            !m_pendingRequests.empty();
                    });

                if (m_isServiceShutdown.load(AZStd::memory_order_acquire))
                {
                    return;
                }

                request = AZStd::move(m_pendingRequests.front());
                m_pendingRequests.pop_front();
            }

            request->BuildInternal(true);
        }
    }
} // namespace AZ::RPI
