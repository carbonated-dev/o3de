/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RHI.Reflect/VkAllocator.h>
#include <Atom/RHI.Profiler/GraphicsProfilerBus.h>
#if defined(CARBONATED)
#include <AzCore/Console/IConsole.h>
#endif
#include <AzCore/Memory/SystemAllocator.h>

#if defined(CARBONATED)
AZ_CVAR(
    bool,
    r_vkUseCustomAllocator,
    true,
    nullptr,
    AZ::ConsoleFunctorFlags::NeedsReload,
    "Enable Vulkan VkAllocationCallbacks backed by AZ::SystemAllocator. Must be set before Vulkan objects are created; changing it at runtime requires reload/restart.");
#endif

namespace AZ
{
    namespace Vulkan
    {
        static void* RHI_vkAllocationFunction(
            [[maybe_unused]]                            void* pUserData,
            size_t                                      size,
            size_t                                      alignment,
            [[maybe_unused]] VkSystemAllocationScope    allocationScope)
        {
            return AZ::AllocatorInstance<AZ::SystemAllocator>::Get().Allocate(size, alignment, 0, "RHI::Vulkan", __FILE__, __LINE__);
        }

        static void* RHI_vkReallocationFunction(
            [[maybe_unused]]                         void* pUserData,
            void*                                    pOriginal,
            size_t                                   size,
            size_t                                   alignment, 
            [[maybe_unused]] VkSystemAllocationScope allocationScope)
        {
            return AZ::AllocatorInstance<AZ::SystemAllocator>::Get().ReAllocate(pOriginal, size, alignment);
        }

        static void RHI_vkFreeFunction([[maybe_unused]] void* pUserData,
            void*                                             pMemory)
        {
            AZ::AllocatorInstance<AZ::SystemAllocator>::Get().DeAllocate(pMemory);
        }

        VkSystemAllocator::VkSystemAllocator()
        {
            // For instance creation/destruction use nullptr for VkAllocationCallbacks* as using VkSystemAllocator::Get() crashes RenderDoc
            // when used with openxr enabled projects. We think it's because RenderDoc is maybe injecting something when doing allocations.
            // Because of this we deactivate callbacks when using Renderdoc.
#if defined(CARBONATED)
            if (r_vkUseCustomAllocator && !RHI::GraphicsProfilerBus::HasHandlers())
#else
            if (!RHI::GraphicsProfilerBus::HasHandlers())
#endif
            {
                m_allocationCallbacks = AZStd::make_unique<VkAllocationCallbacks>();
                m_allocationCallbacks->pfnFree = RHI_vkFreeFunction;
                m_allocationCallbacks->pfnReallocation = RHI_vkReallocationFunction;
                m_allocationCallbacks->pfnAllocation = RHI_vkAllocationFunction;
                m_allocationCallbacks->pfnInternalAllocation = nullptr;
                m_allocationCallbacks->pfnInternalFree = nullptr;
            }
        }

        VkSystemAllocator::~VkSystemAllocator()
        {
        }

        VkAllocationCallbacks* VkSystemAllocator::Get()
        {
            static VkSystemAllocator allocator;
            return allocator.m_allocationCallbacks.get();
        }
    }
}
