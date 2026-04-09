/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>
#include <AzFramework/SDLEventHandler.h>
#include <AzFramework/SDLConnectionManager.h>
#include <AzFramework/SDLInputDeviceKeyboard.h>

namespace AzFramework
{

    SDLInputDeviceKeyboard::SDLInputDeviceKeyboard(InputDeviceKeyboard& inputDevice)
        : InputDeviceKeyboard::Implementation(inputDevice)
    {
        SDLEventHandlerBus::Handler::BusConnect();

        auto* interface = AzFramework::SDLConnectionManagerInterface::Get();
        if (!interface)
        {
            AZ_Warning("ApplicationLinux", false, "SDL interface not available");
            return;
        }

        m_initialized = true;
    }

    bool SDLInputDeviceKeyboard::IsConnected() const
    {
        return false;
    }

    bool SDLInputDeviceKeyboard::HasTextEntryStarted() const
    {
        return m_hasTextEntryStarted;
    }

    void SDLInputDeviceKeyboard::TextEntryStart(const InputDeviceKeyboard::VirtualKeyboardOptions& options)
    {
        m_hasTextEntryStarted = true;
    }

    void SDLInputDeviceKeyboard::TextEntryStop()
    {
        m_hasTextEntryStarted = false;
    }

    void SDLInputDeviceKeyboard::TickInputDevice()
    {
        ProcessRawEventQueues();
    }

    void SDLInputDeviceKeyboard::HandleSDLEvent()
    {
        if (!m_initialized)
        {
            return;
        }
    }

    [[nodiscard]] const InputChannelId* SDLInputDeviceKeyboard::InputChannelFromKeyEvent() const
    {
        return nullptr;
    }

    AZStd::string SDLInputDeviceKeyboard::TextFromKeycode()
    {
        AZStd::string chars;
        return chars;
    }

    void SDLInputDeviceKeyboard::UpdateState()
    {
    }
} // namespace AzFramework
