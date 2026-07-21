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
#include <Atom/RHI/DrawPacketBuilder.h>
#include <Atom/RPI.Public/Shader/Shader.h>

#include <AzCore/std/containers/deque.h>
#include <AzCore/std/containers/fixed_vector.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/function/function_template.h>
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

    //! A draw packet cannot contain more than DrawItemCountMax items. Keeping the request
    //! storage inline avoids one allocation per mesh packet during large rebuild bursts.
    using PipelineStateBuildItemList =
        AZStd::fixed_vector<PipelineStateBuildItem, RHI::DrawPacketBuilder::DrawItemCountMax>;
    using PipelineStateList =
        AZStd::fixed_vector<RHI::ConstPtr<RHI::PipelineState>, RHI::DrawPacketBuilder::DrawItemCountMax>;

    //! Owns one immutable set of pipeline-state descriptors and its completed pipeline states.
    class PipelineStateBuildRequest
        : public AZStd::enable_shared_from_this<PipelineStateBuildRequest>
    {
    public:
        using PrepareFunction =
            AZStd::function<bool(PipelineStateBuildItemList&)>;

        enum class State : uint8_t
        {
            Pending,
            Building,
            Succeeded,
            Failed,
            Cancelled
        };

        explicit PipelineStateBuildRequest(PipelineStateBuildItemList pipelineStateBuildItems);
        explicit PipelineStateBuildRequest(PrepareFunction prepareFunction);

        //! Builds the pipeline states on the calling thread. If the request is already building,
        //! waits for that build to complete.
        void Build();

        //! Tries to acquire every pipeline state without compiling or waiting. This must be called
        //! while the request is still privately owned and pending. On a miss the request remains
        //! pending; on success it becomes a completed request that can be published immediately.
        bool TryAcquireFromCache();

        //! Completes a prepared request with an externally retained set of pipeline states.
        //! This is used by MeshDrawPacket to switch directly to an exact retained fallback.
        bool CompleteFromCachedPipelineStates(
            AZStd::span<const RHI::ConstPtr<RHI::PipelineState>> pipelineStates);

        //! Submits the request to the renderer-wide pipeline-state build queue.
        void Queue();

        //! Prevents a pending request from starting, or asks an active request to stop between PSO builds.
        void Cancel();

        //! Waits until the request reaches a terminal state.
        void Wait();

        State GetState() const;
        bool IsComplete() const;
        bool IsSuccessful() const;

        //! Stable for the lifetime of this request after its build items have been prepared.
        AZ::HashValue64 GetBuildFingerprint() const;

        AZStd::span<const RHI::ConstPtr<RHI::PipelineState>> GetPipelineStates() const;
        AZStd::span<const PipelineStateBuildItem> GetBuildItems() const;
        PipelineStateBuildItemList TakeBuildItems();

    private:
        friend class PipelineStateBuildQueue;

        struct BatchPipelineState
        {
            const Shader* m_shader = nullptr;
            const RHI::PipelineStateDescriptorForDraw* m_descriptor = nullptr;
            AZ::HashValue64 m_descriptorHash;
            RHI::ConstPtr<RHI::PipelineState> m_pipelineState;
        };
        using BatchPipelineStateList = AZStd::vector<BatchPipelineState>;

        bool TryBeginBuild();
        void BuildInternal(
            bool useAsyncCacheAcquire,
            BatchPipelineStateList* batchPipelineStates = nullptr);
        void UpdateBuildFingerprint();
        void SetTerminalState(State state);

        PipelineStateBuildItemList m_pipelineStateBuildItems;
        PipelineStateList m_pipelineStates;
        PrepareFunction m_prepareFunction;
        AZ::HashValue64 m_buildFingerprint;

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
