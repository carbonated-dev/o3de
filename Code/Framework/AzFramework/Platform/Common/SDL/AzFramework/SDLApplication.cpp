/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzFramework/SDLApplication.h>
#include <AzFramework/SDLEventHandler.h>
#include <AzFramework/SDLInterface.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>

namespace AzFramework
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    class SDLConnectionManagerImpl
        : public SDLConnectionManagerBus::Handler
    {
    public:
        SDLConnectionManagerImpl()
        {
            SDLConnectionManagerBus::Handler::BusConnect();
        }

        ~SDLConnectionManagerImpl() override
        {
            SDLConnectionManagerBus::Handler::BusDisconnect();
        }
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////
    SDLApplication::SDLApplication()
    {
        LinuxLifecycleEvents::Bus::Handler::BusConnect();

        /*
        m_sdlConnectionManager = AZStd::make_unique<SDLConnectionManagerImpl>();
        if (SDLConnectionManagerInterface::Get() == nullptr)
        {
            SDLConnectionManagerInterface::Register(m_sdlConnectionManager.get());
        }
        */

        /*
        SDL2
        SDL_INIT_TIMER: timer subsystem
        SDL_INIT_AUDIO: audio subsystem
        SDL_INIT_VIDEO: video subsystem; automatically initializes the events subsystem
        SDL_INIT_JOYSTICK: joystick subsystem; automatically initializes the events subsystem
        SDL_INIT_HAPTIC: haptic (force feedback) subsystem
        SDL_INIT_GAMECONTROLLER: controller subsystem; automatically initializes the joystick subsystem
        SDL_INIT_EVENTS: events subsystem
        SDL_INIT_EVERYTHING: all of the above subsystems
        */
        const uint32_t flags = SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_AUDIO;
        const int res = SDL_Init(flags);
        if (res)
        {
            const char* error = SDL_GetError();
            AZ_Error("SDL", false, "Cannot initialize SDL with flags %x: return code %d, error '%s'", flags, res, error);
            AZ_Assert(false, "Cannot initialize SDL");
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    SDLApplication::~SDLApplication()
    {
        SDL_Quit();

        /*
        if (SDLConnectionManagerInterface::Get() == m_sdlConnectionManager.get())
        {
            SDLConnectionManagerInterface::Unregister(m_sdlConnectionManager.get());
        }
        m_sdlConnectionManager.reset();
        */

        LinuxLifecycleEvents::Bus::Handler::BusDisconnect();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void SDLApplication::PumpSystemEventLoopOnce()
    {
        SDL_PumpEvents();  // do we need it here?
        SDL_Event event;
        if (SDL_PollEvent(&event))
        {
            SDLEventHandlerBus::Broadcast(&SDLEventHandlerBus::Events::HandleSDLEvent, event);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void SDLApplication::PumpSystemEventLoopUntilEmpty()
    {
        SDL_PumpEvents();  // do we need it here?
        SDL_Event event;
        for (;;)
        {
            if (!SDL_PollEvent(&event))
            {
                break;
            }
            SDLEventHandlerBus::Broadcast(&SDLEventHandlerBus::Events::HandleSDLEvent, event);
        }
    }

} // namespace AzFramework
