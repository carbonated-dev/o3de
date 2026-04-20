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

//CARBONATED
#if defined(CARBONATED) && defined(VK_KHR_SELECTED_SURFACE_EXTENSION_NAME)
// we can use XCB and XLIB
#define AZ_VULKAN_SURFACE_EXTENSION_NAME VK_KHR_SELECTED_SURFACE_EXTENSION_NAME
#else
#define AZ_VULKAN_SURFACE_EXTENSION_NAME VK_KHR_XCB_SURFACE_EXTENSION_NAME
#endif
