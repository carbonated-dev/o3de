/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/base.h>
#include <AzCore/Math/Internal/MathTypes.h>

# define AZ_MATH_FORCE_INLINE AZ_FORCE_INLINE

namespace AZ
{
    //define our simd type. Should not be used in FPU mode, but declared anyway to reduce #ifdef's in class declarations.
    
    // gruber patch begin aoreshko: MADPORT-12
#if defined (_MSC_VER) || defined (MAC)
    typedef __m128 SimdVectorType;
#else
    struct SimdVectorType {};   //using a struct to make sure we don't get any unexpected conversions
                                // aoreshko to AVK - sounds good but not working with VectorFloatWin32.Inl
#endif
    
    // gruber patch end aoreshko
}

// gruber patch beg
// math will use floats only.
#if defined (_MSC_VER) || defined (MAC)
#define AZ_ALLOW_SIMD 1
#else
#define AZ_ALLOW_SIMD 0
#endif
// gruber patch end

// declare 2 macros, AZ_SIMD which indicates we are using SIMD instructions of some kind, and AZ_TRAIT_USE_PLATFORM_SIMD to specify we
// are using SIMD on the specified platform.
#if AZ_ALLOW_SIMD && AZ_TRAIT_USE_PLATFORM_SIMD
#define AZ_SIMD 1
#endif

#define AZ_FLT_MAX          FLT_MAX
#define AZ_FLT_EPSILON      FLT_EPSILON
