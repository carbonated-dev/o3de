/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/base.h>

namespace AZ::Render
{
    //! Must match the OitMethod shader option enum in OitCommon.azsli.
    enum class OitMethod : uint32_t
    {
        Off = 0,
        Mlab = 1,
        Wboit = 2,
        Mboit = 3,
    };
} // namespace AZ::Render
