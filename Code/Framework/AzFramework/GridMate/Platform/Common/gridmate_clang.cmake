#
# Copyright (c) Contributors to the Open 3D Engine Project. For complete copyright and license terms please see the LICENSE at the root of this distribution.
# 
# SPDX-License-Identifier: Apache-2.0 OR MIT
#
#

# aefimov: we need it no more, but we can override some compilation keys here if we have to

#ly_add_source_properties(
#    SOURCES
#        GridMate/Carrier/SecureSocketDriver.cpp
#        GridMate/Carrier/StreamSecureSocketDriver.cpp
#    PROPERTY COMPILE_OPTIONS
#    VALUES -Wno-deprecated-declarations -fexceptions
# )

#if(${PAL_PLATFORM_NAME} STREQUAL "Android")
#    set(LY_COMPILE_DEFINITIONS PRIVATE 
#            LINUX64
#            _LINUX
#            LINUX
#            ANDROID
#            MOBILE
#            _HAS_C9X
#            ENABLE_TYPE_INFO
#    )
#    set(LY_COMPILE_OPTIONS PRIVATE 
#           -Wno-unused-variable
#           -Wno-bitwise-instead-of-logical
#           -Wno-unused-but-set-variable
#    )
#endif()
