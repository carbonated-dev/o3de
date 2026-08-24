/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include <Atom/RHI/FrameGraph.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/std/parallel/thread.h>
#include <RHI/FrameGraphExecuteGroupSecondaryHandler.h>
#include <RHI/FrameGraphExecuteGroupPrimaryHandler.h>
#include <RHI/FrameGraphExecuteGroupSecondary.h>
#include <RHI/FrameGraphExecuteGroupPrimary.h>
#include <RHI/FrameGraphExecuter.h>
#include <RHI/Device.h>
#include <RHI/SwapChain.h>
#include <RHI/Scope.h>
#include <RHI/CommandQueueContext.h>


namespace AZ
{
    namespace Vulkan
    {
        RHI::Ptr<FrameGraphExecuter> FrameGraphExecuter::Create()
        {
            return aznew FrameGraphExecuter();
        }

        Device& FrameGraphExecuter::GetDevice() const
        {
            return static_cast<Device&>(Base::GetDevice());
        }
        
        FrameGraphExecuter::FrameGraphExecuter()
        {
            RHI::JobPolicy graphJobPolicy = RHI::JobPolicy::Parallel;
#if defined(AZ_FORCE_CPU_GPU_INSYNC)
            graphJobPolicy = RHI::JobPolicy::Serial;
#endif
            SetJobPolicy(graphJobPolicy);
        }
        
        RHI::ResultCode FrameGraphExecuter::InitInternal(const RHI::FrameGraphExecuterDescriptor& descriptor)
        {
            const RHI::ConstPtr<RHI::PlatformLimitsDescriptor> rhiPlatformLimitsDescriptor = descriptor.m_platformLimitsDescriptor;
            if (RHI::ConstPtr<PlatformLimitsDescriptor> vkPlatformLimitsDesc = azrtti_cast<const PlatformLimitsDescriptor*>(rhiPlatformLimitsDescriptor))
            {
                m_frameGraphExecuterData = vkPlatformLimitsDesc->m_frameGraphExecuterData;
            }
            return RHI::ResultCode::Success;
        }

        void FrameGraphExecuter::ShutdownInternal()
        {
            // do nothing
        }

        void FrameGraphExecuter::BeginInternal(const RHI::FrameGraph& frameGraph)
        {
            for (AZStd::vector<FrameGraphExecuteGroupHandler*>& handlers : m_handlersByQueue)
            {
                handlers.clear();
            }
            m_nextHandlerByQueue.fill(0);

            Device& device = GetDevice();
            AZStd::vector<const Scope*> mergedScopes;
            const Scope* scopePrev = nullptr;
            const Scope* scopeNext = nullptr;
            const AZStd::vector<RHI::Scope*>& scopes = frameGraph.GetScopes();

#if defined(AZ_FORCE_CPU_GPU_INSYNC)
            // Forces all scopes to issue a dedicated merged scope group with one command list.
            // This will ensure that the Execute is done on only one scope and if an error happens
            // we can be sure about the work gpu was working on before the crash.
            for (auto it = scopes.begin(); it != scopes.end(); ++it)
            {
                const Scope& scope = *static_cast<const Scope*>(*it);
                auto nextIter = it + 1;
                scopeNext = nextIter != scopes.end() ? static_cast<const Scope*>(*nextIter) : nullptr;
                const bool subpassGroup = (scopeNext && scopeNext->GetFrameGraphGroupId() == scope.GetFrameGraphGroupId()) ||
                                          (scopePrev && scopePrev->GetFrameGraphGroupId() == scope.GetFrameGraphGroupId());
                
                if (subpassGroup)
                {
                    FrameGraphExecuteGroupSecondary* scopeContextGroup = AddGroup<FrameGraphExecuteGroupSecondary>();
                    scopeContextGroup->Init(device, scope, 1, GetJobPolicy());
                }
                else
                {
                    mergedScopes.push_back(&scope);
                    FrameGraphExecuteGroupPrimary* multiScopeContextGroup = AddGroup<FrameGraphExecuteGroupPrimary>();
                    multiScopeContextGroup->SetName(scope.GetName());
                    multiScopeContextGroup->Init(device, AZStd::move(mergedScopes));
                }
                scopePrev = &scope;
            }
#else
            
            RHI::HardwareQueueClass mergedHardwareQueueClass = RHI::HardwareQueueClass::Graphics;
            uint32_t mergedGroupCost = 0;
            uint32_t mergedSwapchainCount = 0;

            for (auto it = scopes.begin(); it != scopes.end(); ++it)
            {
                const Scope& scope = *static_cast<const Scope*>(*it);
                auto nextIter = it + 1;
                scopeNext = nextIter != scopes.end() ? static_cast<const Scope*>(*nextIter) : nullptr;

                // Reset merged hardware queue class to match current scope if empty.
                if (mergedGroupCost == 0)
                {
                    mergedHardwareQueueClass = scope.GetHardwareQueueClass();
                }

                const uint32_t estimatedItemCount = scope.GetEstimatedItemCount();

                const uint32_t CommandListCostThreshold =
                    AZStd::max(
                        m_frameGraphExecuterData.m_commandListCostThresholdMin,
                        AZ::DivideAndRoundUp(estimatedItemCount, m_frameGraphExecuterData.m_commandListsPerScopeMax));

                /**
                    * Computes a cost heuristic based on the number of items and number of attachments in
                    * the scope. This cost is used to partition command list generation.
                    */
                const uint32_t totalScopeCost =
                    estimatedItemCount * m_frameGraphExecuterData.m_itemCost +
                    static_cast<uint32_t>(scope.GetAttachments().size()) * m_frameGraphExecuterData.m_attachmentCost;

                // Check if we are in a middle of a framegraph group.
                const bool subpassGroup =
                    (scopeNext && scopeNext->GetFrameGraphGroupId() == scope.GetFrameGraphGroupId()) ||
                    (scopePrev && scopePrev->GetFrameGraphGroupId() == scope.GetFrameGraphGroupId());

                const uint32_t swapchainCount = static_cast<uint32_t>(scope.GetSwapChainsToPresent().size());

                // Detect if we are able to continue merging.
                {
                    // Check if the group fits into the current running merge queue. If not, we have to flush the queue.
                    const bool exceededCommandCost = (mergedGroupCost + totalScopeCost) > CommandListCostThreshold;

                    // Check if the swap chains fit into this group.
                    const bool exceededSwapChainLimit = (mergedSwapchainCount + swapchainCount) > m_frameGraphExecuterData.m_swapChainsPerCommandList;

                    // Check if the hardware queue classes match.
                    const bool hardwareQueueMismatch = scope.GetHardwareQueueClass() != mergedHardwareQueueClass;

                    // Check if we are straddling the boundary of a fence/semaphore.
                    const bool onSyncBoundaries = !scope.GetWaitSemaphores().empty() || !scope.GetWaitFences().empty() ||
                        (scopePrev && (!scopePrev->GetSignalSemaphores().empty() || !scopePrev->GetSignalFences().empty()));

                    // If we exceeded limits, then flush the group.
                    const bool flushMergedScopes = exceededCommandCost || exceededSwapChainLimit || hardwareQueueMismatch || onSyncBoundaries || subpassGroup;

                    if (flushMergedScopes && mergedScopes.size())
                    {
                        // All merged scopes use a single primary command list
                        mergedGroupCost = 0;
                        mergedSwapchainCount = 0;
                        mergedHardwareQueueClass = scope.GetHardwareQueueClass();
                        FrameGraphExecuteGroupPrimary* multiScopeContextGroup = AddGroup<FrameGraphExecuteGroupPrimary>();
                        multiScopeContextGroup->Init(device, AZStd::move(mergedScopes));                    
                    }
                }

                // Attempt to merge the current scope.
                if (!subpassGroup && totalScopeCost < CommandListCostThreshold)
                {
                    mergedScopes.push_back(&scope);
                    mergedGroupCost += totalScopeCost;
                    mergedSwapchainCount += swapchainCount;
                }
                // Not mergeable, create a dedicated context group for it.
                else
                {
                    // And then create a new group for the current scope with dedicated [1, N] secondary command lists
                    const uint32_t commandListCount = AZStd::max(AZ::DivideAndRoundUp(totalScopeCost, CommandListCostThreshold), 1u);
                    FrameGraphExecuteGroupSecondary* scopeContextGroup = AddGroup<FrameGraphExecuteGroupSecondary>();
                    scopeContextGroup->Init(device, scope, commandListCount, GetJobPolicy());
                }
                scopePrev = &scope;
            }

            // Merge all pending scopes
            if (mergedScopes.size())
            {
                mergedGroupCost = 0;
                mergedSwapchainCount = 0;
                FrameGraphExecuteGroupPrimary* multiScopeContextGroup = AddGroup<FrameGraphExecuteGroupPrimary>();
                multiScopeContextGroup->Init(device, AZStd::move(mergedScopes));
            }
#endif
            // Create the handlers to manage the execute groups.
            // Handlers manage one or multiple execute groups by creating a shared renderpass/framebuffer
            // or advancing the subpass if needed.
            auto groups = GetGroups();
            AZStd::vector<RHI::FrameGraphExecuteGroup*> groupRefs;
            groupRefs.reserve(groups.size());

            auto groupHasWait = [](const FrameGraphExecuteGroup& group)
            {
                const auto scopes = group.GetScopes();
                return !scopes.empty() &&
                    (!scopes.front()->GetWaitSemaphores().empty() || !scopes.front()->GetWaitFences().empty());
            };

            auto groupHasSignal = [](const FrameGraphExecuteGroup& group)
            {
                const auto scopes = group.GetScopes();
                return !scopes.empty() &&
                    (!scopes.back()->GetSignalSemaphores().empty() || !scopes.back()->GetSignalFences().empty());
            };

            RHI::GraphGroupId groupId;
            const FrameGraphExecuteGroup* previousGroup = nullptr;
            for (const auto& groupPtr : groups)
            {
                const FrameGraphExecuteGroup* group = static_cast<const FrameGraphExecuteGroup*>(groupPtr.get());
                const bool groupIdChanged = !groupRefs.empty() && groupId != group->GetGroupId();
                const bool syncBoundary = !groupRefs.empty() &&
                    (groupHasWait(*group) || (previousGroup && groupHasSignal(*previousGroup)));

                if (groupIdChanged || syncBoundary)
                {
                    const auto currentScopes = group->GetScopes();
                    const auto previousScopes = previousGroup ? previousGroup->GetScopes() : AZStd::span<const Scope* const>{};
                   /* AZ_TracePrintf(
                        "FrameGraph",
                        "Split Vulkan execute-group handler before '%s' (wait=%s, previousSignal=%s)\n",
                        currentScopes.empty() ? "<empty>" : currentScopes.front()->GetId().GetCStr(),
                        groupHasWait(*group) ? "yes" : "no",
                        previousScopes.empty() ? "no" : (groupHasSignal(*previousGroup) ? "yes" : "no"));*/
                    AddExecuteGroupHandler(groupId, groupRefs);
                    groupRefs.clear();
                }

                if (groupRefs.empty())
                {
                    groupId = group->GetGroupId();
                }
                groupRefs.push_back(groupPtr.get());
                previousGroup = group;
            }

            // Add the final handler for the remaining groups.
            AddExecuteGroupHandler(groupId, groupRefs);
        }

        void FrameGraphExecuter::ExecuteGroupInternal(RHI::FrameGraphExecuteGroup& groupBase)
        {
            FrameGraphExecuteGroup& group = static_cast<FrameGraphExecuteGroup&>(groupBase);
            auto findIter = m_groupHandlers.find(&group);
            AZ_Assert(findIter != m_groupHandlers.end(), "Could not find group handler for execute group");
            FrameGraphExecuteGroupHandler* handler = findIter->second;
            AZ_UNUSED(handler);

            // Execute handlers in frame-graph order for this hardware queue. Command-list
            // recording is parallel, so a later handler may become complete first; submitting
            // it immediately would reverse image layout transitions on the same VkQueue.
            AZStd::scoped_lock lock(m_handlerExecutionMutex);
            ExecuteReadyHandlers(group.GetHardwareQueueClass());
        }

        void FrameGraphExecuter::ExecuteReadyHandlers(RHI::HardwareQueueClass hardwareQueueClass)
        {
            const uint32_t queueIndex = static_cast<uint32_t>(hardwareQueueClass);
            AZ_Assert(queueIndex < RHI::HardwareQueueClassCount, "Invalid hardware queue class");

            AZStd::vector<FrameGraphExecuteGroupHandler*>& handlers = m_handlersByQueue[queueIndex];
            size_t& nextHandler = m_nextHandlerByQueue[queueIndex];
            while (nextHandler < handlers.size())
            {
                FrameGraphExecuteGroupHandler* handler = handlers[nextHandler];
                if (!handler->IsComplete())
                {
                    break;
                }

                if (!handler->IsExecuted())
                {
                    handler->End();
                }
                ++nextHandler;
            }
        }

        void FrameGraphExecuter::EndInternal()
        {
            m_groupHandlers.clear();
            m_groupHandlerStorage.clear();
            for (AZStd::vector<FrameGraphExecuteGroupHandler*>& handlers : m_handlersByQueue)
            {
                handlers.clear();
            }
            m_nextHandlerByQueue.fill(0);
        }

        void FrameGraphExecuter::AddExecuteGroupHandler([[maybe_unused]] const RHI::GraphGroupId& groupId, const AZStd::vector<RHI::FrameGraphExecuteGroup*>& groups)
        {
            if (groups.empty())
            {
                return;
            }

            // Add the handler depending on the number of execute groups.
            AZStd::unique_ptr<FrameGraphExecuteGroupHandler> handler(groups.size() == 1 && azrtti_cast<FrameGraphExecuteGroupPrimary*>(groups.front()) ?
                static_cast<FrameGraphExecuteGroupHandler*>(aznew FrameGraphExecuteGroupPrimaryHandler) :
                static_cast<FrameGraphExecuteGroupHandler*>(aznew FrameGraphExecuteGroupSecondaryHandler));

            handler->Init(GetDevice(), groups);
            FrameGraphExecuteGroupHandler* handlerPtr = handler.get();
            const RHI::HardwareQueueClass hardwareQueueClass =
                static_cast<FrameGraphExecuteGroup*>(groups.front())->GetHardwareQueueClass();
            const uint32_t queueIndex = static_cast<uint32_t>(hardwareQueueClass);
            AZ_Assert(queueIndex < RHI::HardwareQueueClassCount, "Invalid hardware queue class");
            for (RHI::FrameGraphExecuteGroup* group : groups)
            {
                AZ_Assert(
                    static_cast<FrameGraphExecuteGroup*>(group)->GetHardwareQueueClass() == hardwareQueueClass,
                    "A Vulkan execute-group handler cannot span hardware queues");
            }
            m_handlersByQueue[queueIndex].push_back(handlerPtr);
            m_groupHandlerStorage.push_back(AZStd::move(handler));
            for (RHI::FrameGraphExecuteGroup* group : groups)
            {
                m_groupHandlers.insert({ group, handlerPtr });
            }
        }
    }
}
