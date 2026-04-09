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

#include <SDL2/SDL.h>

namespace AzFramework
{
    bool SDLInputDeviceMouse::m_xfixesInitialized = true;
    bool SDLInputDeviceMouse::m_xInputInitialized = true;

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
        /*const auto* interface = AzFramework::SDLConnectionManagerInterface::Get();
        if (!interface)
        {
            AZ_Warning("SDLInput", false, "SDL interface not available");
            return nullptr;
        }*/

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
        return m_xfixesInitialized;
    }

    bool SDLInputDeviceMouse::InitializeXInput()
    {
        return m_xInputInitialized;
    }

    void SDLInputDeviceMouse::SetSystemCursorState(SystemCursorState systemCursorState)
    {
        if (m_systemCursorState != systemCursorState)
        {
            m_systemCursorState = systemCursorState;
            HandleCursorState(systemCursorState);
        }
    }

    void SDLInputDeviceMouse::HandleCursorState(SystemCursorState systemCursorState)
    {
        //bool confined = (systemCursorState == SystemCursorState::ConstrainedAndHidden) ||
        //                (systemCursorState == SystemCursorState::ConstrainedAndVisible);

        bool visible = (systemCursorState == SystemCursorState::ConstrainedAndVisible) ||
                       (systemCursorState == SystemCursorState::UnconstrainedAndVisible) ||
                       (systemCursorState == SystemCursorState::Unknown);

        if (systemCursorState == SystemCursorState::ConstrainedAndHidden)
        {
            SDL_SetRelativeMouseMode(SDL_TRUE);
        }
        else
        {
            SDL_SetRelativeMouseMode(SDL_FALSE);
            SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE);
        }
        
        m_cursorShown = visible;
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
        SDL_Window* window = SDL_GetMouseFocus();
        if (!window) return;

        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        
        SDL_WarpMouseInWindow(window, 
            static_cast<int>(positionNormalized.GetX() * w), 
            static_cast<int>(positionNormalized.GetY() * h));
    }

    AZ::Vector2 SDLInputDeviceMouse::GetSystemCursorPositionNormalizedInternal() const
    {
        AZ::Vector2 position = AZ::Vector2::CreateZero();

        return position;
    }

    AZ::Vector2 SDLInputDeviceMouse::GetSystemCursorPositionNormalized() const
    {
        int x, y, w, h;
        SDL_Window* window = SDL_GetMouseFocus();
        if (!window) return AZ::Vector2::CreateZero();

        SDL_GetMouseState(&x, &y);
        SDL_GetWindowSize(window, &w, &h);

        return AZ::Vector2(static_cast<float>(x) / w, static_cast<float>(y) / h);
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

    void SDLInputDeviceMouse::HandleSDLEvent(const SDL_Event& event)
    {
        switch (event.type)
        {
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            {
                const auto* buttonId = InputChannelFromSDLButton(event.button.button);
                if (buttonId)
                {
                    QueueRawButtonEvent(*buttonId, event.type == SDL_MOUSEBUTTONDOWN);
                }
                break;
            }
            case SDL_MOUSEMOTION:
            {
                // Если мы в Relative Mode (курсор скрыт и захвачен), используем xrel/yrel
                if (SDL_GetRelativeMouseMode())
                {
                    QueueRawMovementEvent(InputDeviceMouse::Movement::X, static_cast<float>(event.motion.xrel));
                    QueueRawMovementEvent(InputDeviceMouse::Movement::Y, static_cast<float>(event.motion.yrel));
                }
                else
                {
                    // Обычное движение (абсолютные координаты внутри окна)
                    // O3DE ожидает дельту для осей Movement::X/Y в Raw событиях
                    QueueRawMovementEvent(InputDeviceMouse::Movement::X, static_cast<float>(event.motion.xrel));
                    QueueRawMovementEvent(InputDeviceMouse::Movement::Y, static_cast<float>(event.motion.yrel));
                }
                break;
            }
            case SDL_MOUSEWHEEL:
            {
                float delta = static_cast<float>(event.wheel.y) * MAX_XI_WHEEL_SENSITIVITY;
                QueueRawMovementEvent(InputDeviceMouse::Movement::Z, delta);
                break;
            }
        }
    }

    const InputChannelId* SDLInputDeviceMouse::InputChannelFromSDLButton(uint8_t button) const
    {
        switch (button)
        {
            case SDL_BUTTON_LEFT:   return &InputDeviceMouse::Button::Left;
            case SDL_BUTTON_MIDDLE: return &InputDeviceMouse::Button::Middle;
            case SDL_BUTTON_RIGHT:  return &InputDeviceMouse::Button::Right;
            default: return nullptr;
        }
    }    
} // namespace AzFramework
