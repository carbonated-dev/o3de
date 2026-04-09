/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>
#include <AzFramework/SDLConnectionManager.h>
#include <AzFramework/SDLEventHandler.h>
#include <AzFramework/SDLInterface.h>

// The maximum number of raw input axis this mouse device supports.
constexpr uint32_t MAX_XI_RAW_AXIS = 2;

// The sensitivity of the wheel.
constexpr float MAX_XI_WHEEL_SENSITIVITY = 140.0f;

namespace AzFramework
{
    class SDLInputDeviceMouse
        : public InputDeviceMouse::Implementation
        , public SDLEventHandlerBus::Handler
    {
    public:
        AZ_CLASS_ALLOCATOR(SDLInputDeviceMouse, AZ::SystemAllocator);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Constructor
        //! \param[in] inputDevice Reference to the input device being implemented
        SDLInputDeviceMouse(InputDeviceMouse& inputDevice);

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! Destructor
        virtual ~SDLInputDeviceMouse();

        static SDLInputDeviceMouse::Implementation* Create(InputDeviceMouse& inputDevice);

    protected:
        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref AzFramework::InputDeviceMouse::Implementation::IsConnected
        bool IsConnected() const override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref AzFramework::InputDeviceMouse::Implementation::SetSystemCursorState
        void SetSystemCursorState(SystemCursorState systemCursorState) override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref AzFramework::InputDeviceMouse::Implementation::GetSystemCursorState
        SystemCursorState GetSystemCursorState() const override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref AzFramework::InputDeviceMouse::Implementation::SetSystemCursorPositionNormalized
        void SetSystemCursorPositionNormalized(AZ::Vector2 positionNormalized) override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref AzFramework::InputDeviceMouse::Implementation::GetSystemCursorPositionNormalized
        AZ::Vector2 GetSystemCursorPositionNormalized() const override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        //! \ref AzFramework::InputDeviceMouse::Implementation::TickInputDevice
        void TickInputDevice() override;

        //! Handle X11 events.
        void HandleSDLEvent(const SDL_Event& event) override;

        //! Initialize XFixes extension. Used for barriers.
        static bool InitializeXFixes();

        //! Initialize XInput extension. Used for raw input during confinement and showing/hiding the cursor.
        static bool InitializeXInput();

        //! Create barriers.
        void CreateBarriers(bool create);

        //! Helper function.
        void SystemCursorStateToLogic(SystemCursorState systemCursorState, bool& confined, bool& cursorShown);

        //! Shows/Hides the cursor.
        void ShowCursor(bool show);

        //! Get the normalized cursor position. The coordinates returned are relative to the specified window.
        AZ::Vector2 GetSystemCursorPositionNormalizedInternal() const;

        //! Set the normalized cursor position. The normalized position will be relative to the specified window.
        void SetSystemCursorPositionNormalizedInternal(AZ::Vector2 positionNormalized);

        //! Handle button press/release events.
        void HandleButtonPressEvents(uint32_t detail, bool pressed);

        //! Will set cursor states and confinement modes.
        void HandleCursorState(SystemCursorState systemCursorState);

        //! Will handle all raw input events.
        void HandleRawInputEvents();

        const InputChannelId* InputChannelFromMouseEvent(bool& isWheel, float& direction) const
        {
            return nullptr;
        }

        // Barriers work only with positive values. We clamp here to zero.
        inline int16_t Clamp(int16_t value) const
        {
            return value < 0 ? 0 : value;
        }

        const InputChannelId* InputChannelFromSDLButton(uint8_t button) const;

    private:
        //! The current system cursor state
        SystemCursorState m_systemCursorState;

        //! The cursor position before it got hidden.
        AZ::Vector2 m_cursorHiddenPosition;

        AZ::Vector2 m_systemCursorPositionNormalized;

        //! Will be true if the xfixes extension could be initialized.
        static bool m_xfixesInitialized;

        //! Will be true if the xinput2 extension could be initialized.
        static bool m_xInputInitialized;

        //! Will be true if the cursor is shown else false.
        bool m_cursorShown;
    };
} // namespace AzFramework
