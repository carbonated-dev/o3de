/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#if defined(CARBONATED)

#include <AzCore/NativeUI/NativeUIRequests.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/string/string.h>

namespace AZ::NativeUI
{
    //! Optional, dependency-free extension point checked by AZ::Debug::Trace::Assert before it falls back to
    //! NativeUIRequestBus::DisplayAssertDialog. Lets a Gem that owns an in-game UI (e.g. LyShine) service asserts
    //! on platforms without a window compositor (e.g. OCGA), where the native OS message box can never be shown.
    //! Registered via AZ::Interface<InGameAssertUIRequests>, not an EBus, since NativeUIRequestBus is a
    //! Single-handler bus already claimed by the default NativeUISystem and cannot take a second handler.
    class InGameAssertUIRequests
    {
    public:
        AZ_RTTI(InGameAssertUIRequests, "{5F0C6D2E-3C2C-4E1B-9C5D-2E9E7B5A6B4C}");
        virtual ~InGameAssertUIRequests() = default;

        //! Waits (blocking the calling thread) for the user to select an option before returning.
        //! Implementations must never return AssertAction::BREAK - abort/break is intentionally not
        //! supported by the in-game assert UI.
        virtual AssertAction DisplayAssertDialog(const AZStd::string& message) = 0;
    };
} // namespace AZ::NativeUI

#endif // defined(CARBONATED)
