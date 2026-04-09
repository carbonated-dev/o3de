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
        [[nodiscard]] const InputChannelId* InputChannelFromSDLScancode(SDL_Scancode scancode) const;

        //static AZStd::string TextFromKeycode();
        //void UpdateState();

        bool m_initialized{false};
        bool m_hasTextEntryStarted{false};
    };
} // namespace AzFramework
