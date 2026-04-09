/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/RTTI.h>

#include <xcb/xcb.h>

namespace AzFramework
{
    class SDLConnectionManager
    {
    public:
        AZ_RTTI(SDLConnectionManager, "{036AC6B2-84E7-431D-93A8-29FC3B830A2D}");

        virtual ~SDLConnectionManager() = default;

        virtual xcb_connection_t* GetXcbConnection() const = 0;

        //! Enables/Disables XInput Raw Input events.
        virtual void SetEnableXInput(xcb_connection_t* connection, bool enable) = 0;
    };

    class SDLConnectionManagerBusTraits
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////
    };

    using SDLConnectionManagerBus = AZ::EBus<SDLConnectionManager, SDLConnectionManagerBusTraits>;
    using SDLConnectionManagerInterface = AZ::Interface<SDLConnectionManager>;
} // namespace AzFramework
