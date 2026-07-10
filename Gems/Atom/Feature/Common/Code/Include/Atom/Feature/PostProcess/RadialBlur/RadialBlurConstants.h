/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/base.h>
#include <AzCore/Math/Vector2.h>

namespace AZ
{
    namespace Render
    {
        namespace RadialBlur
        {
            inline const AZ::Vector2 DefaultCenter = AZ::Vector2(0.5f, 0.5f);
            static constexpr float DefaultAmount = 0.0f;
            static constexpr float DefaultInnerRadius = 0.0f;
            static constexpr uint32_t DefaultSampleCount = 16;
            static constexpr uint32_t MinSampleCount = 1;
            static constexpr uint32_t MaxSampleCount = 64;
        } // namespace RadialBlur
    } // namespace Render
} // namespace AZ
