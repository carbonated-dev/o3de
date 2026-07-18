/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/RHI/PipelineState.h>
#include <Atom/RHI/PipelineStateDescriptor.h>
#include <Atom/RPI.Public/Shader/Shader.h>

#include <AzCore/std/containers/deque.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/parallel/condition_variable.h>
#include <AzCore/std/parallel/mutex.h>
#include <AzCore/std/parallel/thread.h>
#include <AzCore/std/smart_ptr/enable_shared_from_this.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>

namespace AZ::RPI
{
    struct PipelineStateBuildItem
    {
        Data::Instance<Shader> m_shader;
        RHI::PipelineStateDescriptorForDraw m_descriptor;
    };

    using PipelineStateBuildItemList = AZStd::vector<PipelineStateBuildItem>;

    //! Owns one immutable set of pipeline-state descriptors and its completed pipeline states.
    class PipelineStateBuildRequest
        : public AZStd::enable_shared_from_this<PipelineStateBuildRequest>
    {
    public:
        enum class State : uint8_t
        {
            Pending,
            Building,
            Succeeded,
            Failed,
            Cancelled
        };

        explicit PipelineStateBuildRequest(PipelineStateBuildItemList pipelineStateBuildItems);

        //! Builds the pipeline states on the calling thread. If the request is already building,
        //! waits for that build to complete.
        void Build();

        //! Submits the request to the renderer-wide pipeline-state build queue.
        void Queue();

        //! Prevents a pending request from starting, or asks an active request to stop between PSO builds.
        void Cancel();

        //! Waits until the request reaches a terminal state.
        void Wait();

        State GetState() const;
        bool IsComplete() const;
        bool IsSuccessful() const;

        AZStd::span<const RHI::ConstPtr<RHI::PipelineState>> GetPipelineStates() const;
        PipelineStateBuildItemList TakeBuildItems();

    private:
        friend class PipelineStateBuildQueue;

        bool TryBeginBuild();
        void BuildInternal(bool useAsyncCacheAcquire);
        void SetTerminalState(State state);

        PipelineStateBuildItemList m_pipelineStateBuildItems;
        AZStd::vector<RHI::ConstPtr<RHI::PipelineState>> m_pipelineStates;

        AZStd::atomic<State> m_state{ State::Pending };
        AZStd::atomic_bool m_cancelRequested{ false };
        AZStd::mutex m_completionMutex;
        AZStd::condition_variable m_completionCondition;
    };

    using PipelineStateBuildRequestPtr = AZStd::shared_ptr<PipelineStateBuildRequest>;

    //! Serializes asynchronous pipeline-state creation through one dedicated worker thread.
    class PipelineStateBuildQueue final
    {
    public:
        PipelineStateBuildQueue() = default;
        ~PipelineStateBuildQueue();

        AZ_DISABLE_COPY_MOVE(PipelineStateBuildQueue);

        void Init();
        void Shutdown();

        void Queue(PipelineStateBuildRequestPtr request);

    private:
        void ThreadServiceLoop();

        AZStd::thread m_serviceThread;
        AZStd::atomic_bool m_isServiceShutdown{ true };
        AZStd::mutex m_mutex;
        AZStd::condition_variable m_workCondition;
        AZStd::deque<PipelineStateBuildRequestPtr> m_pendingRequests;
    };
} // namespace AZ::RPI
