/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/std/typetraits/integral_constant.h>
#include <AzFramework/API/ApplicationAPI_Linux.h>
#include <AzFramework/SDLConnectionManager.h>
#include <AzFramework/SDLInputDeviceMouse.h>

namespace AzFramework
{
    bool SDLInputDeviceMouse::m_xfixesInitialized = false;
    bool SDLInputDeviceMouse::m_xInputInitialized = false;

    SDLInputDeviceMouse::SDLInputDeviceMouse(InputDeviceMouse& inputDevice)
        : InputDeviceMouse::Implementation(inputDevice)
        , m_systemCursorState(SystemCursorState::Unknown)
        , m_systemCursorPositionNormalized(0.5f, 0.5f)
        , m_cursorShown(true)
    {
        SDLEventHandlerBus::Handler::BusConnect();

        SetSystemCursorState(SystemCursorState::Unknown);
    }

    SDLInputDeviceMouse::~SDLInputDeviceMouse()
    {
        SDLEventHandlerBus::Handler::BusDisconnect();

        SetSystemCursorState(SystemCursorState::Unknown);
    }

    InputDeviceMouse::Implementation* SDLInputDeviceMouse::Create(InputDeviceMouse& inputDevice)
    {
        const auto* interface = AzFramework::SDLConnectionManagerInterface::Get();
        if (!interface)
        {
            AZ_Warning("SDLInput", false, "SDL interface not available");
            return nullptr;
        }

        // Initialize XFixes extension which we use to create pointer barriers.
        if (!InitializeXFixes())
        {
            AZ_Warning("SDLInput", false, "SDL XFixes initialization failed");
            return nullptr;
        }

        // Initialize XInput extension which is used to get RAW Input events.
        if (!InitializeXInput())
        {
            AZ_Warning("SDLInput", false, "SDL XInput initialization failed");
            return nullptr;
        }

        return aznew SDLInputDeviceMouse(inputDevice);
    }

    bool SDLInputDeviceMouse::IsConnected() const
    {
        return true;
    }

    void SDLInputDeviceMouse::CreateBarriers(bool create)
    {
        // Don't create any barriers if we are debugging. This will cause artifacts but better then
        // a confined cursor during debugging.
        if (AZ::Debug::Trace::Instance().IsDebuggerPresent())
        {
            AZ_Warning("SDLInput", false, "Debugger running. Barriers will not be created.");
            return;
        }
    }

    bool SDLInputDeviceMouse::InitializeXFixes()
    {
        m_xfixesInitialized = false;

        return m_xfixesInitialized;
    }

    bool SDLInputDeviceMouse::InitializeXInput()
    {
        m_xInputInitialized = false;

        return m_xInputInitialized;
    }

    void SDLInputDeviceMouse::SetSystemCursorState(SystemCursorState systemCursorState)
    {
    }

    void SDLInputDeviceMouse::HandleCursorState(SystemCursorState systemCursorState)
    {
        if (m_captureCursor)
        {
            const bool confined = (systemCursorState == SystemCursorState::ConstrainedAndHidden) ||
                (systemCursorState == SystemCursorState::ConstrainedAndVisible);
            const bool cursorShown = (systemCursorState == SystemCursorState::ConstrainedAndVisible) ||
                (systemCursorState == SystemCursorState::UnconstrainedAndVisible);

            CreateBarriers(confined);
            ShowCursor(cursorShown);
        }
    }

    SystemCursorState SDLInputDeviceMouse::GetSystemCursorState() const
    {
        return m_systemCursorState;
    }

    void SDLInputDeviceMouse::SetSystemCursorPositionNormalizedInternal(AZ::Vector2 positionNormalized)
    {
    }

    void SDLInputDeviceMouse::SetSystemCursorPositionNormalized(AZ::Vector2 positionNormalized)
    {
    }

    AZ::Vector2 SDLInputDeviceMouse::GetSystemCursorPositionNormalizedInternal() const
    {
        AZ::Vector2 position = AZ::Vector2::CreateZero();

        return position;
    }

    AZ::Vector2 SDLInputDeviceMouse::GetSystemCursorPositionNormalized() const
    {
        AZ::Vector2 position = AZ::Vector2::CreateZero();

        return position;
    }

    void SDLInputDeviceMouse::TickInputDevice()
    {
        ProcessRawEventQueues();
    }

    void SDLInputDeviceMouse::ShowCursor(bool show)
    {
    }

    void SDLInputDeviceMouse::HandleButtonPressEvents(uint32_t detail, bool pressed)
    {
    }

    void SDLInputDeviceMouse::HandleRawInputEvents()
    {
    }

    void SDLInputDeviceMouse::HandleSDLEvent()
    {
    }
} // namespace AzFramework
