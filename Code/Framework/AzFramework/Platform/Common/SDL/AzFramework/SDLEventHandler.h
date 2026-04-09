/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzCore/RTTI/RTTI.h>

#include <xcb/xcb.h>

namespace AzFramework
{
    static constexpr inline uint8_t s_SDLResponseTypeMask = 0x7f; // Mask to extract the specific event type from an xcb event

    class SDLEventHandler
    {
    public:
        AZ_RTTI(SDLEventHandler, "{0F1CB937-421E-4A68-8778-F3319A0DC698}");

        virtual ~SDLEventHandler() = default;

        virtual void HandleSDLEvent(xcb_generic_event_t* event) = 0;
    };

    class SDLEventHandlerBusTraits : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////
    };

    using SDLEventHandlerBus = AZ::EBus<SDLEventHandler, SDLEventHandlerBusTraits>;
} // namespace AzFramework
