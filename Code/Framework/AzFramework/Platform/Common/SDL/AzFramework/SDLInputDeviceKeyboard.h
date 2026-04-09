/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>
#include <AzFramework/SDLEventHandler.h>
#include <AzFramework/SDLInterface.h>

#include <xcb/xcb.h>
#include <xkbcommon/xkbcommon.h>

struct xcb_xkb_state_notify_event_t;

namespace AzFramework
{
    class SDLInputDeviceKeyboard
        : public InputDeviceKeyboard::Implementation
        , public SDLEventHandlerBus::Handler
    {
    public:
        AZ_CLASS_ALLOCATOR(SDLInputDeviceKeyboard, AZ::SystemAllocator);

        using InputDeviceKeyboard::Implementation::Implementation;
        SDLInputDeviceKeyboard(InputDeviceKeyboard& inputDevice);

        bool IsConnected() const override;

        bool HasTextEntryStarted() const override;
        void TextEntryStart(const InputDeviceKeyboard::VirtualKeyboardOptions& options) override;
        void TextEntryStop() override;
        void TickInputDevice() override;

        void HandleSDLEvent(xcb_generic_event_t* event) override;

    private:
        [[nodiscard]] const InputChannelId* InputChannelFromKeyEvent(xcb_keycode_t code) const;

        static AZStd::string TextFromKeycode(xkb_state* state, xkb_keycode_t code);

        void UpdateState(const xcb_xkb_state_notify_event_t* state);

        SDLUniquePtr<xkb_context, xkb_context_unref> m_xkbContext;
        SDLUniquePtr<xkb_keymap, xkb_keymap_unref> m_xkbKeymap;
        SDLUniquePtr<xkb_state, xkb_state_unref> m_xkbState;
        int m_coreDeviceId{-1};
        uint8_t m_xkbEventCode{0};
        bool m_initialized{false};
        bool m_hasTextEntryStarted{false};
    };
} // namespace AzFramework
