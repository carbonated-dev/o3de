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

#include <SDL2/SDL.h>
#include <xkbcommon/xkbcommon.h>

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

        void HandleSDLEvent(const SDL_Event& event) override;

    private:
        [[nodiscard]] const InputChannelId* InputChannelFromKeyEvent(SDL_Scancode code) const;

        static AZStd::string TextFromKeycode(xkb_state* state, SDL_Scancode code);

        // TODO void UpdateState(const xcb_xkb_state_notify_event_t* state);

        //XcbUniquePtr<xkb_context, xkb_context_unref> m_xkbContext;
        //XcbUniquePtr<xkb_keymap, xkb_keymap_unref> m_xkbKeymap;
        //XcbUniquePtr<xkb_state, xkb_state_unref> m_xkbState;

        struct XkbContextDeleter
        {
            void operator()(xkb_context* ptr) const { if (ptr) xkb_context_unref(ptr); }
        };

        struct XkbKeymapDeleter
        {
            void operator()(xkb_keymap* ptr) const { if (ptr) xkb_keymap_unref(ptr); }
        };

        struct XkbStateDeleter
        {
            void operator()(xkb_state* ptr) const { if (ptr) xkb_state_unref(ptr); }
        };

        AZStd::unique_ptr<xkb_context, XkbContextDeleter> m_xkbContext;
        AZStd::unique_ptr<xkb_keymap, XkbKeymapDeleter> m_xkbKeymap;
        AZStd::unique_ptr<xkb_state, XkbStateDeleter> m_xkbState;

        //int m_coreDeviceId{-1};
        //uint8_t m_xkbEventCode{0};
        bool m_initialized{false};
        bool m_hasTextEntryStarted{false};
    };
} // namespace AzFramework
