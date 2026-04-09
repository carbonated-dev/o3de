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

        /*auto* interface = AzFramework::SDLConnectionManagerInterface::Get();
        if (!interface)
        {
            AZ_Warning("ApplicationLinux", false, "SDL interface not available");
            return;
        }*/

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

    [[nodiscard]] const InputChannelId* SDLInputDeviceKeyboard::InputChannelFromSDLScancode(SDL_Scancode scancode) const
    {
        // Маппинг SDL Scancode в O3DE InputChannelId
        switch (scancode)
        {
            case SDL_SCANCODE_0: return &InputDeviceKeyboard::Key::Alphanumeric0;
            case SDL_SCANCODE_1: return &InputDeviceKeyboard::Key::Alphanumeric1;
            case SDL_SCANCODE_2: return &InputDeviceKeyboard::Key::Alphanumeric2;
            case SDL_SCANCODE_3: return &InputDeviceKeyboard::Key::Alphanumeric3;
            case SDL_SCANCODE_4: return &InputDeviceKeyboard::Key::Alphanumeric4;
            case SDL_SCANCODE_5: return &InputDeviceKeyboard::Key::Alphanumeric5;
            case SDL_SCANCODE_6: return &InputDeviceKeyboard::Key::Alphanumeric6;
            case SDL_SCANCODE_7: return &InputDeviceKeyboard::Key::Alphanumeric7;
            case SDL_SCANCODE_8: return &InputDeviceKeyboard::Key::Alphanumeric8;
            case SDL_SCANCODE_9: return &InputDeviceKeyboard::Key::Alphanumeric9;
            
            case SDL_SCANCODE_A: return &InputDeviceKeyboard::Key::AlphanumericA;
            case SDL_SCANCODE_B: return &InputDeviceKeyboard::Key::AlphanumericB;
            case SDL_SCANCODE_C: return &InputDeviceKeyboard::Key::AlphanumericC;
            case SDL_SCANCODE_D: return &InputDeviceKeyboard::Key::AlphanumericD;
            case SDL_SCANCODE_E: return &InputDeviceKeyboard::Key::AlphanumericE;
            case SDL_SCANCODE_F: return &InputDeviceKeyboard::Key::AlphanumericF;
            case SDL_SCANCODE_G: return &InputDeviceKeyboard::Key::AlphanumericG;
            case SDL_SCANCODE_H: return &InputDeviceKeyboard::Key::AlphanumericH;
            case SDL_SCANCODE_I: return &InputDeviceKeyboard::Key::AlphanumericI;
            case SDL_SCANCODE_J: return &InputDeviceKeyboard::Key::AlphanumericJ;
            case SDL_SCANCODE_K: return &InputDeviceKeyboard::Key::AlphanumericK;
            case SDL_SCANCODE_L: return &InputDeviceKeyboard::Key::AlphanumericL;
            case SDL_SCANCODE_M: return &InputDeviceKeyboard::Key::AlphanumericM;
            case SDL_SCANCODE_N: return &InputDeviceKeyboard::Key::AlphanumericN;
            case SDL_SCANCODE_O: return &InputDeviceKeyboard::Key::AlphanumericO;
            case SDL_SCANCODE_P: return &InputDeviceKeyboard::Key::AlphanumericP;
            case SDL_SCANCODE_Q: return &InputDeviceKeyboard::Key::AlphanumericQ;
            case SDL_SCANCODE_R: return &InputDeviceKeyboard::Key::AlphanumericR;
            case SDL_SCANCODE_S: return &InputDeviceKeyboard::Key::AlphanumericS;
            case SDL_SCANCODE_T: return &InputDeviceKeyboard::Key::AlphanumericT;
            case SDL_SCANCODE_U: return &InputDeviceKeyboard::Key::AlphanumericU;
            case SDL_SCANCODE_V: return &InputDeviceKeyboard::Key::AlphanumericV;
            case SDL_SCANCODE_W: return &InputDeviceKeyboard::Key::AlphanumericW;
            case SDL_SCANCODE_X: return &InputDeviceKeyboard::Key::AlphanumericX;
            case SDL_SCANCODE_Y: return &InputDeviceKeyboard::Key::AlphanumericY;
            case SDL_SCANCODE_Z: return &InputDeviceKeyboard::Key::AlphanumericZ;

            case SDL_SCANCODE_BACKSPACE: return &InputDeviceKeyboard::Key::EditBackspace;
            case SDL_SCANCODE_CAPSLOCK:  return &InputDeviceKeyboard::Key::EditCapsLock;
            case SDL_SCANCODE_RETURN:    return &InputDeviceKeyboard::Key::EditEnter;
            case SDL_SCANCODE_SPACE:     return &InputDeviceKeyboard::Key::EditSpace;
            case SDL_SCANCODE_TAB:       return &InputDeviceKeyboard::Key::EditTab;
            case SDL_SCANCODE_ESCAPE:    return &InputDeviceKeyboard::Key::Escape;

            case SDL_SCANCODE_F1:  return &InputDeviceKeyboard::Key::Function01;
            case SDL_SCANCODE_F2:  return &InputDeviceKeyboard::Key::Function02;
            case SDL_SCANCODE_F3:  return &InputDeviceKeyboard::Key::Function03;
            case SDL_SCANCODE_F4:  return &InputDeviceKeyboard::Key::Function04;
            case SDL_SCANCODE_F5:  return &InputDeviceKeyboard::Key::Function05;
            case SDL_SCANCODE_F6:  return &InputDeviceKeyboard::Key::Function06;
            case SDL_SCANCODE_F7:  return &InputDeviceKeyboard::Key::Function07;
            case SDL_SCANCODE_F8:  return &InputDeviceKeyboard::Key::Function08;
            case SDL_SCANCODE_F9:  return &InputDeviceKeyboard::Key::Function09;
            case SDL_SCANCODE_F10: return &InputDeviceKeyboard::Key::Function10;
            case SDL_SCANCODE_F11: return &InputDeviceKeyboard::Key::Function11;
            case SDL_SCANCODE_F12: return &InputDeviceKeyboard::Key::Function12;

            case SDL_SCANCODE_LALT:   return &InputDeviceKeyboard::Key::ModifierAltL;
            case SDL_SCANCODE_RALT:   return &InputDeviceKeyboard::Key::ModifierAltR;
            case SDL_SCANCODE_LCTRL:  return &InputDeviceKeyboard::Key::ModifierCtrlL;
            case SDL_SCANCODE_RCTRL:  return &InputDeviceKeyboard::Key::ModifierCtrlR;
            case SDL_SCANCODE_LSHIFT: return &InputDeviceKeyboard::Key::ModifierShiftL;
            case SDL_SCANCODE_RSHIFT: return &InputDeviceKeyboard::Key::ModifierShiftR;
            case SDL_SCANCODE_LGUI:   return &InputDeviceKeyboard::Key::ModifierSuperL;
            case SDL_SCANCODE_RGUI:   return &InputDeviceKeyboard::Key::ModifierSuperR;

            case SDL_SCANCODE_DOWN:  return &InputDeviceKeyboard::Key::NavigationArrowDown;
            case SDL_SCANCODE_LEFT:  return &InputDeviceKeyboard::Key::NavigationArrowLeft;
            case SDL_SCANCODE_RIGHT: return &InputDeviceKeyboard::Key::NavigationArrowRight;
            case SDL_SCANCODE_UP:    return &InputDeviceKeyboard::Key::NavigationArrowUp;
            
            case SDL_SCANCODE_DELETE:   return &InputDeviceKeyboard::Key::NavigationDelete;
            case SDL_SCANCODE_END:      return &InputDeviceKeyboard::Key::NavigationEnd;
            case SDL_SCANCODE_HOME:     return &InputDeviceKeyboard::Key::NavigationHome;
            case SDL_SCANCODE_INSERT:   return &InputDeviceKeyboard::Key::NavigationInsert;
            case SDL_SCANCODE_PAGEDOWN: return &InputDeviceKeyboard::Key::NavigationPageDown;
            case SDL_SCANCODE_PAGEUP:   return &InputDeviceKeyboard::Key::NavigationPageUp;

            case SDL_SCANCODE_NUMLOCKCLEAR: return &InputDeviceKeyboard::Key::NumLock;
            case SDL_SCANCODE_KP_0: return &InputDeviceKeyboard::Key::NumPad0;
            case SDL_SCANCODE_KP_1: return &InputDeviceKeyboard::Key::NumPad1;
            case SDL_SCANCODE_KP_2: return &InputDeviceKeyboard::Key::NumPad2;
            case SDL_SCANCODE_KP_3: return &InputDeviceKeyboard::Key::NumPad3;
            case SDL_SCANCODE_KP_4: return &InputDeviceKeyboard::Key::NumPad4;
            case SDL_SCANCODE_KP_5: return &InputDeviceKeyboard::Key::NumPad5;
            case SDL_SCANCODE_KP_6: return &InputDeviceKeyboard::Key::NumPad6;
            case SDL_SCANCODE_KP_7: return &InputDeviceKeyboard::Key::NumPad7;
            case SDL_SCANCODE_KP_8: return &InputDeviceKeyboard::Key::NumPad8;
            case SDL_SCANCODE_KP_9: return &InputDeviceKeyboard::Key::NumPad9;
            case SDL_SCANCODE_KP_PLUS:     return &InputDeviceKeyboard::Key::NumPadAdd;
            case SDL_SCANCODE_KP_PERIOD:   return &InputDeviceKeyboard::Key::NumPadDecimal;
            case SDL_SCANCODE_KP_DIVIDE:   return &InputDeviceKeyboard::Key::NumPadDivide;
            case SDL_SCANCODE_KP_ENTER:    return &InputDeviceKeyboard::Key::NumPadEnter;
            case SDL_SCANCODE_KP_MULTIPLY: return &InputDeviceKeyboard::Key::NumPadMultiply;
            case SDL_SCANCODE_KP_MINUS:    return &InputDeviceKeyboard::Key::NumPadSubtract;

            case SDL_SCANCODE_APOSTROPHE: return &InputDeviceKeyboard::Key::PunctuationApostrophe;
            case SDL_SCANCODE_BACKSLASH:  return &InputDeviceKeyboard::Key::PunctuationBackslash;
            case SDL_SCANCODE_LEFTBRACKET:  return &InputDeviceKeyboard::Key::PunctuationBracketL;
            case SDL_SCANCODE_RIGHTBRACKET: return &InputDeviceKeyboard::Key::PunctuationBracketR;
            case SDL_SCANCODE_COMMA:      return &InputDeviceKeyboard::Key::PunctuationComma;
            case SDL_SCANCODE_EQUALS:     return &InputDeviceKeyboard::Key::PunctuationEquals;
            case SDL_SCANCODE_MINUS:      return &InputDeviceKeyboard::Key::PunctuationHyphen;
            case SDL_SCANCODE_PERIOD:     return &InputDeviceKeyboard::Key::PunctuationPeriod;
            case SDL_SCANCODE_SEMICOLON:  return &InputDeviceKeyboard::Key::PunctuationSemicolon;
            case SDL_SCANCODE_SLASH:      return &InputDeviceKeyboard::Key::PunctuationSlash;
            case SDL_SCANCODE_GRAVE:      return &InputDeviceKeyboard::Key::PunctuationTilde;

            case SDL_SCANCODE_PAUSE:       return &InputDeviceKeyboard::Key::WindowsSystemPause;
            case SDL_SCANCODE_PRINTSCREEN: return &InputDeviceKeyboard::Key::WindowsSystemPrint;
            case SDL_SCANCODE_SCROLLLOCK:  return &InputDeviceKeyboard::Key::WindowsSystemScrollLock;

            default: return nullptr;
        }
    }    

    //AZStd::string SDLInputDeviceKeyboard::TextFromKeycode() { return {}; }

    //void SDLInputDeviceKeyboard::UpdateState() { }
} // namespace AzFramework
