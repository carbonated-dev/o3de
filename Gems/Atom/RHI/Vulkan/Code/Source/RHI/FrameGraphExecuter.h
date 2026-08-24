/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <Atom/RHI/FrameGraph.h>
#include <Atom/RHI/FrameGraphExecuter.h>
#include <RHI/CommandQueue.h>
#include <RHI/FrameGraphExecuteGroupHandler.h>

#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/parallel/mutex.h>
#include <Atom/RHI.Reflect/Vulkan/PlatformLimitsDescriptor.h>
namespace AZ
{
    namespace Vulkan
    {
        class Device;


        class FrameGraphExecuter final
            : public RHI::FrameGraphExecuter
        {
            using Base = RHI::FrameGraphExecuter;

        public:
            AZ_CLASS_ALLOCATOR(FrameGraphExecuter, AZ::SystemAllocator);
            AZ_RTTI(FrameGraphExecuter, "22B6E224-9469-4D8B-828F-A81C83B6EEEC", Base);

            static RHI::Ptr<FrameGraphExecuter> Create();

            Device& GetDevice() const;

        private:
            FrameGraphExecuter();

            //////////////////////////////////////////////////////////////////////////
            // RHI::FrameGraphExecuter
            RHI::ResultCode InitInternal(const RHI::FrameGraphExecuterDescriptor& descriptor) override;
            void ShutdownInternal() override;
            void BeginInternal(const RHI::FrameGraph& frameGraph) override;
            void ExecuteGroupInternal(RHI::FrameGraphExecuteGroup& group) override;
            void EndInternal() override;
            //////////////////////////////////////////////////////////////////////////

            // Adds a handler for a list of execute groups.
            void AddExecuteGroupHandler(const RHI::GraphGroupId& groupId, const AZStd::vector<RHI::FrameGraphExecuteGroup*>& groups);
            void ExecuteReadyHandlers(RHI::HardwareQueueClass hardwareQueueClass);

            // List of handlers for execute groups.
            // A graph group may need to be split at a semaphore boundary. Use the
            // execute-group address as the lookup key so multiple handlers can own
            // portions of the same graph group.
            AZStd::unordered_map<const RHI::FrameGraphExecuteGroup*, FrameGraphExecuteGroupHandler*> m_groupHandlers;
            AZStd::vector<AZStd::unique_ptr<FrameGraphExecuteGroupHandler>> m_groupHandlerStorage;
            AZStd::array<AZStd::vector<FrameGraphExecuteGroupHandler*>, RHI::HardwareQueueClassCount> m_handlersByQueue;
            AZStd::array<size_t, RHI::HardwareQueueClassCount> m_nextHandlerByQueue{};
            AZStd::mutex m_handlerExecutionMutex;
            FrameGraphExecuterData m_frameGraphExecuterData;
        };
    }
}
