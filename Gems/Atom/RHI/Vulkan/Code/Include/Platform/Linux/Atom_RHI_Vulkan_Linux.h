/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/base.h>
#include <AzCore/PlatformIncl.h>
#include <AzCore/std/algorithm.h>
#include <vulkan/vulkan.h>
#include <limits.h>
#include <RHI/Vulkan.h>

#if defined (CARBONATED)
#if PAL_TRAIT_LINUX_WINDOW_MANAGER_XCB
    #define AZ_VULKAN_SURFACE_EXTENSION_NAME VK_KHR_XCB_SURFACE_EXTENSION_NAME
#elif PAL_TRAIT_LINUX_WINDOW_MANAGER_WAYLAND
    #define AZ_VULKAN_SURFACE_EXTENSION_NAME VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
#elif PAL_TRAIT_LINUX_WINDOW_MANAGER_SDL
    // Temporary: SDL backend currently uses X11 Vulkan surface extension on Linux
    #define AZ_VULKAN_SURFACE_EXTENSION_NAME "VK_KHR_xcb_surface"
#else
    #error "Unsupported Linux window manager for Vulkan surface extension"
#endif
#else
#define AZ_VULKAN_SURFACE_EXTENSION_NAME VK_KHR_XCB_SURFACE_EXTENSION_NAME
#endif
