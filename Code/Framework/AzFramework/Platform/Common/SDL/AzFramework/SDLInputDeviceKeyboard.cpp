/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

//#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>
#include <AzFramework/SDLEventHandler.h>
#include <AzFramework/SDLConnectionManager.h>
#include <AzFramework/SDLInputDeviceKeyboard.h>

namespace AzFramework
{

    SDLInputDeviceKeyboard::SDLInputDeviceKeyboard(InputDeviceKeyboard& inputDevice)
        : InputDeviceKeyboard::Implementation(inputDevice)
    {
        SDLEventHandlerBus::Handler::BusConnect();

        /*auto* interface = AzFramework::SDLConnectionManagerInterface::Get();
        if (!interface)
        {
            AZ_Warning("ApplicationLinux", false, "SDL interface not available");
            return;
        }*/

        InitKeys();

        m_initialized = true;
    }

    bool SDLInputDeviceKeyboard::IsConnected() const
    {
        return m_initialized;
    }

    bool SDLInputDeviceKeyboard::HasTextEntryStarted() const
    {
        return m_hasTextEntryStarted;
    }

    void SDLInputDeviceKeyboard::TextEntryStart(const InputDeviceKeyboard::VirtualKeyboardOptions& options)
    {
        m_hasTextEntryStarted = true;
        SDL_StartTextInput();
    }

    void SDLInputDeviceKeyboard::TextEntryStop()
    {
        m_hasTextEntryStarted = false;
        SDL_StopTextInput();        
    }

    void SDLInputDeviceKeyboard::TickInputDevice()
    {
        ProcessRawEventQueues();
    }

    void SDLInputDeviceKeyboard::HandleSDLEvent(const SDL_Event& event)
    {
        if (!m_initialized)
        {
            return;
        }

        switch (event.type)
        {
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                const bool isPressed = (event.type == SDL_KEYDOWN);
                const InputChannelId* channelId = InputChannelFromSDLScancode(event.key.keysym.scancode);
                
                if (channelId)
                {
                    QueueRawKeyEvent(*channelId, isPressed);
                }
                break;
            }

            case SDL_TEXTINPUT:
            {
                if (m_hasTextEntryStarted)
                {
                    QueueRawTextEvent(AZStd::string(event.text.text));
                }
                break;
            }
        }        
    }

    const InputChannelId* SDLInputDeviceKeyboard::InputChannelFromSDLScancode(SDL_Scancode scancode) const
    {
        int index = int(scancode);
        if (index < 0 || index >= sizeof(m_keyDecode) / sizeof(m_keyDecode[0]))
        {
            AZ_Error("SDL", false, "Key %d is out of range 0..%d", index, sizeof(m_keyDecode) / sizeof(m_keyDecode[0]));
            return nullptr;
        }
        return m_keyDecode[index];
    }

    void SDLInputDeviceKeyboard::InitKeys()
    {
        for (int i=0; i < sizeof(m_keyDecode) / sizeof(m_keyDecode[0]); i++)
        {
            m_keyDecode[i] = nullptr;
        }

        m_keyDecode[SDL_SCANCODE_0] = &InputDeviceKeyboard::Key::Alphanumeric0;
        m_keyDecode[SDL_SCANCODE_1] = &InputDeviceKeyboard::Key::Alphanumeric1;
        m_keyDecode[SDL_SCANCODE_2] = &InputDeviceKeyboard::Key::Alphanumeric2;
        m_keyDecode[SDL_SCANCODE_3] = &InputDeviceKeyboard::Key::Alphanumeric3;
        m_keyDecode[SDL_SCANCODE_4] = &InputDeviceKeyboard::Key::Alphanumeric4;
        m_keyDecode[SDL_SCANCODE_5] = &InputDeviceKeyboard::Key::Alphanumeric5;
        m_keyDecode[SDL_SCANCODE_6] = &InputDeviceKeyboard::Key::Alphanumeric6;
        m_keyDecode[SDL_SCANCODE_7] = &InputDeviceKeyboard::Key::Alphanumeric7;
        m_keyDecode[SDL_SCANCODE_8] = &InputDeviceKeyboard::Key::Alphanumeric8;
        m_keyDecode[SDL_SCANCODE_9] = &InputDeviceKeyboard::Key::Alphanumeric9;
            
        m_keyDecode[SDL_SCANCODE_A] = &InputDeviceKeyboard::Key::AlphanumericA;
        m_keyDecode[SDL_SCANCODE_B] = &InputDeviceKeyboard::Key::AlphanumericB;
        m_keyDecode[SDL_SCANCODE_C] = &InputDeviceKeyboard::Key::AlphanumericC;
        m_keyDecode[SDL_SCANCODE_D] = &InputDeviceKeyboard::Key::AlphanumericD;
        m_keyDecode[SDL_SCANCODE_E] = &InputDeviceKeyboard::Key::AlphanumericE;
        m_keyDecode[SDL_SCANCODE_F] = &InputDeviceKeyboard::Key::AlphanumericF;
        m_keyDecode[SDL_SCANCODE_G] = &InputDeviceKeyboard::Key::AlphanumericG;
        m_keyDecode[SDL_SCANCODE_H] = &InputDeviceKeyboard::Key::AlphanumericH;
        m_keyDecode[SDL_SCANCODE_I] = &InputDeviceKeyboard::Key::AlphanumericI;
        m_keyDecode[SDL_SCANCODE_J] = &InputDeviceKeyboard::Key::AlphanumericJ;
        m_keyDecode[SDL_SCANCODE_K] = &InputDeviceKeyboard::Key::AlphanumericK;
        m_keyDecode[SDL_SCANCODE_L] = &InputDeviceKeyboard::Key::AlphanumericL;
        m_keyDecode[SDL_SCANCODE_M] = &InputDeviceKeyboard::Key::AlphanumericM;
        m_keyDecode[SDL_SCANCODE_N] = &InputDeviceKeyboard::Key::AlphanumericN;
        m_keyDecode[SDL_SCANCODE_O] = &InputDeviceKeyboard::Key::AlphanumericO;
        m_keyDecode[SDL_SCANCODE_P] = &InputDeviceKeyboard::Key::AlphanumericP;
        m_keyDecode[SDL_SCANCODE_Q] = &InputDeviceKeyboard::Key::AlphanumericQ;
        m_keyDecode[SDL_SCANCODE_R] = &InputDeviceKeyboard::Key::AlphanumericR;
        m_keyDecode[SDL_SCANCODE_S] = &InputDeviceKeyboard::Key::AlphanumericS;
        m_keyDecode[SDL_SCANCODE_T] = &InputDeviceKeyboard::Key::AlphanumericT;
        m_keyDecode[SDL_SCANCODE_U] = &InputDeviceKeyboard::Key::AlphanumericU;
        m_keyDecode[SDL_SCANCODE_V] = &InputDeviceKeyboard::Key::AlphanumericV;
        m_keyDecode[SDL_SCANCODE_W] = &InputDeviceKeyboard::Key::AlphanumericW;
        m_keyDecode[SDL_SCANCODE_X] = &InputDeviceKeyboard::Key::AlphanumericX;
        m_keyDecode[SDL_SCANCODE_Y] = &InputDeviceKeyboard::Key::AlphanumericY;
        m_keyDecode[SDL_SCANCODE_Z] = &InputDeviceKeyboard::Key::AlphanumericZ;

        m_keyDecode[SDL_SCANCODE_BACKSPACE] = &InputDeviceKeyboard::Key::EditBackspace;
        m_keyDecode[SDL_SCANCODE_CAPSLOCK] = &InputDeviceKeyboard::Key::EditCapsLock;
        m_keyDecode[SDL_SCANCODE_RETURN] = &InputDeviceKeyboard::Key::EditEnter;
        m_keyDecode[SDL_SCANCODE_SPACE] = &InputDeviceKeyboard::Key::EditSpace;
        m_keyDecode[SDL_SCANCODE_TAB] = &InputDeviceKeyboard::Key::EditTab;
        m_keyDecode[SDL_SCANCODE_ESCAPE] = &InputDeviceKeyboard::Key::Escape;

        m_keyDecode[SDL_SCANCODE_F1] = &InputDeviceKeyboard::Key::Function01;
        m_keyDecode[SDL_SCANCODE_F2] = &InputDeviceKeyboard::Key::Function02;
        m_keyDecode[SDL_SCANCODE_F3] = &InputDeviceKeyboard::Key::Function03;
        m_keyDecode[SDL_SCANCODE_F4] = &InputDeviceKeyboard::Key::Function04;
        m_keyDecode[SDL_SCANCODE_F5] = &InputDeviceKeyboard::Key::Function05;
        m_keyDecode[SDL_SCANCODE_F6] = &InputDeviceKeyboard::Key::Function06;
        m_keyDecode[SDL_SCANCODE_F7] = &InputDeviceKeyboard::Key::Function07;
        m_keyDecode[SDL_SCANCODE_F8] = &InputDeviceKeyboard::Key::Function08;
        m_keyDecode[SDL_SCANCODE_F9] = &InputDeviceKeyboard::Key::Function09;
        m_keyDecode[SDL_SCANCODE_F10] = &InputDeviceKeyboard::Key::Function10;
        m_keyDecode[SDL_SCANCODE_F11] = &InputDeviceKeyboard::Key::Function11;
        m_keyDecode[SDL_SCANCODE_F12] = &InputDeviceKeyboard::Key::Function12;

        m_keyDecode[SDL_SCANCODE_LALT] = &InputDeviceKeyboard::Key::ModifierAltL;
        m_keyDecode[SDL_SCANCODE_RALT] = &InputDeviceKeyboard::Key::ModifierAltR;
        m_keyDecode[SDL_SCANCODE_LCTRL] = &InputDeviceKeyboard::Key::ModifierCtrlL;
        m_keyDecode[SDL_SCANCODE_RCTRL] = &InputDeviceKeyboard::Key::ModifierCtrlR;
        m_keyDecode[SDL_SCANCODE_LSHIFT] = &InputDeviceKeyboard::Key::ModifierShiftL;
        m_keyDecode[SDL_SCANCODE_RSHIFT] = &InputDeviceKeyboard::Key::ModifierShiftR;
        m_keyDecode[SDL_SCANCODE_LGUI] = &InputDeviceKeyboard::Key::ModifierSuperL;
        m_keyDecode[SDL_SCANCODE_RGUI] = &InputDeviceKeyboard::Key::ModifierSuperR;

        m_keyDecode[SDL_SCANCODE_DOWN] = &InputDeviceKeyboard::Key::NavigationArrowDown;
        m_keyDecode[SDL_SCANCODE_LEFT] = &InputDeviceKeyboard::Key::NavigationArrowLeft;
        m_keyDecode[SDL_SCANCODE_RIGHT] = &InputDeviceKeyboard::Key::NavigationArrowRight;
        m_keyDecode[SDL_SCANCODE_UP] = &InputDeviceKeyboard::Key::NavigationArrowUp;
            
        m_keyDecode[SDL_SCANCODE_DELETE] = &InputDeviceKeyboard::Key::NavigationDelete;
        m_keyDecode[SDL_SCANCODE_END] = &InputDeviceKeyboard::Key::NavigationEnd;
        m_keyDecode[SDL_SCANCODE_HOME] = &InputDeviceKeyboard::Key::NavigationHome;
        m_keyDecode[SDL_SCANCODE_INSERT] = &InputDeviceKeyboard::Key::NavigationInsert;
        m_keyDecode[SDL_SCANCODE_PAGEDOWN] = &InputDeviceKeyboard::Key::NavigationPageDown;
        m_keyDecode[SDL_SCANCODE_PAGEUP] = &InputDeviceKeyboard::Key::NavigationPageUp;

        m_keyDecode[SDL_SCANCODE_NUMLOCKCLEAR] = &InputDeviceKeyboard::Key::NumLock;
        m_keyDecode[SDL_SCANCODE_KP_0] = &InputDeviceKeyboard::Key::NumPad0;
        m_keyDecode[SDL_SCANCODE_KP_1] = &InputDeviceKeyboard::Key::NumPad1;
        m_keyDecode[SDL_SCANCODE_KP_2] = &InputDeviceKeyboard::Key::NumPad2;
        m_keyDecode[SDL_SCANCODE_KP_3] = &InputDeviceKeyboard::Key::NumPad3;
        m_keyDecode[SDL_SCANCODE_KP_4] = &InputDeviceKeyboard::Key::NumPad4;
        m_keyDecode[SDL_SCANCODE_KP_5] = &InputDeviceKeyboard::Key::NumPad5;
        m_keyDecode[SDL_SCANCODE_KP_6] = &InputDeviceKeyboard::Key::NumPad6;
        m_keyDecode[SDL_SCANCODE_KP_7] = &InputDeviceKeyboard::Key::NumPad7;
        m_keyDecode[SDL_SCANCODE_KP_8] = &InputDeviceKeyboard::Key::NumPad8;
        m_keyDecode[SDL_SCANCODE_KP_9] = &InputDeviceKeyboard::Key::NumPad9;
        m_keyDecode[SDL_SCANCODE_KP_PLUS] = &InputDeviceKeyboard::Key::NumPadAdd;
        m_keyDecode[SDL_SCANCODE_KP_PERIOD] = &InputDeviceKeyboard::Key::NumPadDecimal;
        m_keyDecode[SDL_SCANCODE_KP_DIVIDE] = &InputDeviceKeyboard::Key::NumPadDivide;
        m_keyDecode[SDL_SCANCODE_KP_ENTER] = &InputDeviceKeyboard::Key::NumPadEnter;
        m_keyDecode[SDL_SCANCODE_KP_MULTIPLY] = &InputDeviceKeyboard::Key::NumPadMultiply;
        m_keyDecode[SDL_SCANCODE_KP_MINUS] = &InputDeviceKeyboard::Key::NumPadSubtract;

        m_keyDecode[SDL_SCANCODE_APOSTROPHE] = &InputDeviceKeyboard::Key::PunctuationApostrophe;
        m_keyDecode[SDL_SCANCODE_BACKSLASH] = &InputDeviceKeyboard::Key::PunctuationBackslash;
        m_keyDecode[SDL_SCANCODE_LEFTBRACKET] = &InputDeviceKeyboard::Key::PunctuationBracketL;
        m_keyDecode[SDL_SCANCODE_RIGHTBRACKET] = &InputDeviceKeyboard::Key::PunctuationBracketR;
        m_keyDecode[SDL_SCANCODE_COMMA] = &InputDeviceKeyboard::Key::PunctuationComma;
        m_keyDecode[SDL_SCANCODE_EQUALS] = &InputDeviceKeyboard::Key::PunctuationEquals;
        m_keyDecode[SDL_SCANCODE_MINUS] = &InputDeviceKeyboard::Key::PunctuationHyphen;
        m_keyDecode[SDL_SCANCODE_PERIOD] = &InputDeviceKeyboard::Key::PunctuationPeriod;
        m_keyDecode[SDL_SCANCODE_SEMICOLON] = &InputDeviceKeyboard::Key::PunctuationSemicolon;
        m_keyDecode[SDL_SCANCODE_SLASH] = &InputDeviceKeyboard::Key::PunctuationSlash;
        m_keyDecode[SDL_SCANCODE_GRAVE] = &InputDeviceKeyboard::Key::PunctuationTilde;

        m_keyDecode[SDL_SCANCODE_PAUSE] = &InputDeviceKeyboard::Key::WindowsSystemPause;
        m_keyDecode[SDL_SCANCODE_PRINTSCREEN] = &InputDeviceKeyboard::Key::WindowsSystemPrint;
        m_keyDecode[SDL_SCANCODE_SCROLLLOCK] = &InputDeviceKeyboard::Key::WindowsSystemScrollLock;
    }    

    //AZStd::string SDLInputDeviceKeyboard::TextFromKeycode() { return {}; }
    //void SDLInputDeviceKeyboard::UpdateState() { }
} // namespace AzFramework
