/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <Atom/RHI.Reflect/Handle.h>
#include <Atom/RHI/PipelineLibrary.h>
#include <Atom/RHI/PipelineLibraryNotificationBus.h>
#include <Atom/RHI/PipelineState.h>
#include <Atom/RHI/PipelineStateDescriptor.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/deque.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/parallel/condition_variable.h>
#include <AzCore/std/parallel/mutex.h>
#include <AzCore/std/parallel/thread.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/containers/variant.h>

namespace AZ::RHI
{
    class PipelineStateCache;
    class PipelineStateBuildQueue;

    using PipelineStateBuildGroupId = Handle<uint64_t, PipelineStateBuildQueue>;

    //! A generic request to compile one pipeline state. It deliberately carries no feature-processor-specific data.
    class PipelineStateBuildRequest final
    {
    public:
        AZ_CLASS_ALLOCATOR(PipelineStateBuildRequest, SystemAllocator);

        enum class State : uint8_t
        {
            Pending,
            Building,
            Succeeded,
            Failed,
            Cancelled
        };

        State GetState() const;
        bool IsComplete() const;
        bool UsesSpecializationConstants() const;
        ConstPtr<PipelineState> GetPipelineState() const;

    private:
        friend class PipelineStateBuildQueue;

        PipelineStateBuildRequest(
            PipelineStateBuildGroupId groupId,
            PipelineLibraryHandle pipelineLibraryHandle,
            const PipelineStateDescriptor& descriptor,
            const Name& name);

        const PipelineStateDescriptor& GetDescriptor() const;

        using PipelineStateDescriptorVariant = AZStd::variant<
            PipelineStateDescriptorForDraw,
            PipelineStateDescriptorForDispatch,
            PipelineStateDescriptorForRayTracing>;

        PipelineStateBuildGroupId m_groupId;
        PipelineLibraryHandle m_pipelineLibraryHandle;
        PipelineStateDescriptorVariant m_descriptor;
        Name m_name;

        mutable AZStd::mutex m_mutex;
        State m_state = State::Pending;
        bool m_cancelRequested = false;
        ConstPtr<PipelineState> m_pipelineState;
    };

    using PipelineStateBuildRequestPtr = AZStd::shared_ptr<PipelineStateBuildRequest>;
    using PipelineStateBuildRequestList = AZStd::vector<PipelineStateBuildRequestPtr>;
    using PipelineStateBuildRequestSet = AZStd::unordered_set<const PipelineStateBuildRequest*>;

    //! Compiles pipeline states in FIFO order on one persistent worker thread.
    class PipelineStateBuildQueue final
        : private PipelineLibraryNotificationBus::Handler
    {
    public:
        AZ_CLASS_ALLOCATOR(PipelineStateBuildQueue, SystemAllocator);

        PipelineStateBuildQueue() = default;
        ~PipelineStateBuildQueue();
        AZ_DISABLE_COPY_MOVE(PipelineStateBuildQueue);

        void Init(Ptr<PipelineStateCache> pipelineStateCache);
        void Shutdown();

        //! Creates an opaque group used to submit and collect related requests.
        PipelineStateBuildGroupId CreateRequestGroup();

        //! Cancels all requests in the group and discards completed results that have not been taken.
        void ReleaseRequestGroup(PipelineStateBuildGroupId groupId);

        PipelineStateBuildRequestPtr QueuePipelineStateBuild(
            PipelineStateBuildGroupId groupId,
            PipelineLibraryHandle pipelineLibraryHandle,
            const PipelineStateDescriptor& descriptor,
            const Name& name = Name());

        //! Cancels a pending request and removes a completed result that has not been taken. An active compilation
        //! is allowed to finish, but its result is discarded.
        void Cancel(const PipelineStateBuildRequestPtr& request);

        //! Moves all completed requests for the group out of the queue under one lock.
        PipelineStateBuildRequestList TakeCompletedRequests(PipelineStateBuildGroupId groupId);

    private:
        // PipelineLibraryNotificationBus::Handler
        void OnPipelineLibraryRelease(
            const PipelineStateCache* pipelineStateCache, PipelineLibraryHandle pipelineLibraryHandle) override;

        void ThreadServiceLoop();

        Ptr<PipelineStateCache> m_pipelineStateCache;
        AZStd::thread m_serviceThread;
        AZStd::mutex m_mutex;
        AZStd::condition_variable m_workCondition;
        AZStd::deque<PipelineStateBuildRequestPtr> m_pendingRequests;
        AZStd::unordered_map<PipelineStateBuildGroupId, PipelineStateBuildRequestList> m_completedRequestsByGroup;
        AZStd::unordered_set<PipelineStateBuildGroupId> m_activeGroups;
        PipelineStateBuildRequestPtr m_activeRequest;
        uint64_t m_nextGroupId = 1;
        bool m_isInitialized = false;
        bool m_isShutdown = true;
    };
} // namespace AZ::RHI
