#
# Copyright (c) Contributors to the Open 3D Engine Project.
# For complete copyright and license terms please see the LICENSE at the root of this distribution.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

# Platform specific cmake file for configuring target compiler/link properties
# based on the active platform
# NOTE: functions in cmake are global, therefore adding functions to this file
# is being avoided to prevent overriding functions declared in other targets platfrom
# specific cmake files

set(LY_BUILD_DEPENDENCIES
    PRIVATE
        pthread
        3rdParty::unwind
        atomic
    PUBLIC
        ${CMAKE_DL_LIBS}
)

# CARBONATED: NativeUISystemComponent_Linux.cpp uses SDL_ShowMessageBox for a native assert/OK/
# yes-no dialog on Linux (dev/test parity with the Windows DialogBoxIndirectParam path - does not
# apply to OCGA itself, which has no window compositor). Only add this when the project is
# actually configured to use SDL as its Linux window manager (see platform_nativeui_linux.cmake in
# AzFramework for the same trait check).
if(PAL_TRAIT_LINUX_WINDOW_MANAGER STREQUAL "sdl")
    list(APPEND LY_BUILD_DEPENDENCIES
        PRIVATE
            SDL2
    )
endif()
