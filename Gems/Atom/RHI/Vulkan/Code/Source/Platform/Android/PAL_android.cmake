#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

set(PAL_TRAIT_ATOM_RHI_VULKAN_SUPPORTED TRUE)
set(PAL_TRAIT_AFTERMATH_AVAILABLE FALSE)

# CARBONATED begin
# Allow project override to disable Vulkan validation layers
if(DEFINED DISABLE_VULKAN_VALIDATION_LAYER AND DISABLE_VULKAN_VALIDATION_LAYER)
    message(STATUS "Vulkan validation layers disabled by project override")
    unset(VULKAN_VALIDATION_LAYER CACHE)
    set(VULKAN_VALIDATION_LAYER "" CACHE STRING "Validation layers disabled")
else()
    message(STATUS "Vulkan validation layers included")
    set(VULKAN_VALIDATION_LAYER 3rdParty::vulkan-validationlayers)
endif()
# CARBONATED end
