/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RPI.Public/PipelineStateBuildQueue.h>
#include <Atom/RPI.Public/RPISystemInterface.h>

namespace AZ::RPI
{
    PipelineStateBuildRequest::PipelineStateBuildRequest(
        PipelineStateBuildItemList pipelineStateBuildItems)
        : m_pipelineStateBuildItems(AZStd::move(pipelineStateBuildItems))
    {
        UpdateBuildFingerprint();
    }

    PipelineStateBuildRequest::PipelineStateBuildRequest(
        PrepareFunction prepareFunction)
        : m_prepareFunction(AZStd::move(prepareFunction))
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

    bool PipelineStateBuildRequest::TryAcquireFromCache()
    {
        AZ_Assert(
            GetState() == State::Pending && !m_prepareFunction,
            "Cache-only acquisition requires a prepared, pending pipeline-state request.");
        if (GetState() != State::Pending || m_prepareFunction ||
            m_cancelRequested.load(AZStd::memory_order_acquire))
        {
            return false;
        }

        m_pipelineStates.clear();
        for (size_t itemIndex = 0;
             itemIndex < m_pipelineStateBuildItems.size();
             ++itemIndex)
        {
            const PipelineStateBuildItem& buildItem =
                m_pipelineStateBuildItems[itemIndex];
            const RHI::PipelineState* pipelineState = nullptr;
            for (size_t previousIndex = 0;
                 previousIndex < itemIndex;
                 ++previousIndex)
            {
                const PipelineStateBuildItem& previousItem =
                    m_pipelineStateBuildItems[previousIndex];
                if (previousItem.m_shader.get() == buildItem.m_shader.get() &&
                    previousItem.m_descriptor == buildItem.m_descriptor)
                {
                    pipelineState = m_pipelineStates[previousIndex].get();
                    break;
                }
            }

            if (!pipelineState)
            {
                pipelineState = buildItem.m_shader->AcquirePipelineState(
                    buildItem.m_descriptor,
                    true);
            }
            if (!pipelineState)
            {
                m_pipelineStates.clear();
                return false;
            }
            m_pipelineStates.emplace_back(pipelineState);
        }

        SetTerminalState(State::Succeeded);
        return true;
    }

    bool PipelineStateBuildRequest::CompleteFromCachedPipelineStates(
        AZStd::span<const RHI::ConstPtr<RHI::PipelineState>> pipelineStates)
    {
        if (GetState() != State::Pending || m_prepareFunction ||
            m_cancelRequested.load(AZStd::memory_order_acquire) ||
            pipelineStates.size() != m_pipelineStateBuildItems.size() ||
            pipelineStates.size() > m_pipelineStates.max_size())
        {
            return false;
        }

        m_pipelineStates.clear();
        for (const RHI::ConstPtr<RHI::PipelineState>& pipelineState : pipelineStates)
        {
            if (!pipelineState)
            {
                m_pipelineStates.clear();
                return false;
            }
            m_pipelineStates.emplace_back(pipelineState);
        }

        SetTerminalState(State::Succeeded);
        return true;
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

    AZ::HashValue64 PipelineStateBuildRequest::GetBuildFingerprint() const
    {
        return m_buildFingerprint;
    }

    AZStd::span<const RHI::ConstPtr<RHI::PipelineState>>
    PipelineStateBuildRequest::GetPipelineStates() const
    {
        AZ_Assert(IsSuccessful(), "Pipeline states are only available after a successful build.");
        return m_pipelineStates;
    }

    AZStd::span<const PipelineStateBuildItem>
    PipelineStateBuildRequest::GetBuildItems() const
    {
        AZ_Assert(IsSuccessful(), "Pipeline-state build items are only available after a successful build.");
        return m_pipelineStateBuildItems;
    }

    PipelineStateBuildItemList PipelineStateBuildRequest::TakeBuildItems()
    {
        AZ_Assert(IsSuccessful(), "Pipeline-state build items are only available after a successful build.");
        return AZStd::move(m_pipelineStateBuildItems);
    }

    void PipelineStateBuildRequest::BuildInternal(
        bool useAsyncCacheAcquire,
        BatchPipelineStateList* batchPipelineStates)
    {
        if (m_cancelRequested.load(AZStd::memory_order_acquire))
        {
            m_prepareFunction = {};
            SetTerminalState(State::Cancelled);
            return;
        }

        if (m_prepareFunction)
        {
            bool preparationSucceeded;
            preparationSucceeded =
                m_prepareFunction(m_pipelineStateBuildItems);
            m_prepareFunction = {};
            if (!preparationSucceeded)
            {
                SetTerminalState(
                    m_cancelRequested.load(AZStd::memory_order_acquire)
                    ? State::Cancelled
                    : State::Failed);
                return;
            }
            UpdateBuildFingerprint();
        }

        m_pipelineStates.clear();

        {
            for (const PipelineStateBuildItem& buildItem : m_pipelineStateBuildItems)
            {
                if (m_cancelRequested.load(AZStd::memory_order_acquire))
                {
                    m_pipelineStates.clear();
                    SetTerminalState(State::Cancelled);
                    return;
                }

                const AZ::HashValue64 descriptorHash =
                    buildItem.m_descriptor.GetHash();
                const RHI::PipelineState* pipelineState = nullptr;
                if (batchPipelineStates)
                {
                    for (const BatchPipelineState& batchPipelineState :
                         *batchPipelineStates)
                    {
                        if (batchPipelineState.m_shader ==
                                buildItem.m_shader.get() &&
                            batchPipelineState.m_descriptorHash ==
                                descriptorHash &&
                            *batchPipelineState.m_descriptor ==
                                buildItem.m_descriptor)
                        {
                            pipelineState =
                                batchPipelineState.m_pipelineState.get();
                            break;
                        }
                    }
                }

                if (!pipelineState)
                {
                    pipelineState = useAsyncCacheAcquire
                        ? buildItem.m_shader->AcquirePipelineStateAsync(
                            buildItem.m_descriptor)
                        : buildItem.m_shader->AcquirePipelineState(
                            buildItem.m_descriptor);
                    if (pipelineState && batchPipelineStates)
                    {
                        batchPipelineStates->push_back(
                            BatchPipelineState{
                                buildItem.m_shader.get(),
                                &buildItem.m_descriptor,
                                descriptorHash,
                                pipelineState });
                    }
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
        }

        SetTerminalState(
            m_cancelRequested.load(AZStd::memory_order_acquire)
            ? State::Cancelled
            : State::Succeeded);
    }

    void PipelineStateBuildRequest::UpdateBuildFingerprint()
    {
        const size_t itemCount = m_pipelineStateBuildItems.size();
        AZ::HashValue64 fingerprint = AZ::TypeHash64(itemCount);
        for (const PipelineStateBuildItem& buildItem :
             m_pipelineStateBuildItems)
        {
            const uintptr_t shaderAddress =
                reinterpret_cast<uintptr_t>(buildItem.m_shader.get());
            fingerprint = AZ::TypeHash64(shaderAddress, fingerprint);
            const AZ::HashValue64 descriptorHash =
                buildItem.m_descriptor.GetHash();
            fingerprint = AZ::TypeHash64(descriptorHash, fingerprint);
        }
        m_buildFingerprint = fingerprint;
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
#if defined(AZ_PLATFORM_WINDOWS)
        // Driver compilation is throughput work and must not preempt the mesh-init workers
        // that the frame is synchronously waiting on.
        threadDesc.m_priority = -1; // THREAD_PRIORITY_BELOW_NORMAL
#endif
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
            AZStd::deque<PipelineStateBuildRequestPtr> requests;
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

                requests.swap(m_pendingRequests);
            }

            PipelineStateBuildRequest::BatchPipelineStateList
                batchPipelineStates;
            batchPipelineStates.reserve(
                requests.size() *
                RHI::DrawPacketBuilder::DrawItemCountMax);
            for (const PipelineStateBuildRequestPtr& request : requests)
            {
                request->BuildInternal(true, &batchPipelineStates);
            }
        }
    }
} // namespace AZ::RPI
