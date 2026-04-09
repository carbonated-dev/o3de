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

#include <SDL/xinput.h>

namespace AzFramework
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    class SDLConnectionManagerImpl
        : public SDLConnectionManagerBus::Handler
    {
    public:
        SDLConnectionManagerImpl()
            : m_SDLConnection(SDL_connect(nullptr, nullptr))
        {
            AZ_Error("Application", m_SDLConnection != nullptr, "Unable to connect to X11 Server.");
            SDLConnectionManagerBus::Handler::BusConnect();
        }

        ~SDLConnectionManagerImpl() override
        {
            SDLConnectionManagerBus::Handler::BusDisconnect();
        }

        SDL_connection_t* GetSDLConnection() const override
        {
            return m_SDLConnection.get();
        }

        void SetEnableXInput(SDL_connection_t* connection, bool enable) override
        {
            struct Mask
            {
                SDL_input_event_mask_t head;
                SDL_input_xi_event_mask_t mask;
            };
            const Mask mask {
                /*.head=*/{
                    /*.device_id=*/SDL_INPUT_DEVICE_ALL_MASTER,
                    /*.mask_len=*/1
                },
                /*.mask=*/ enable ?
                    (SDL_input_xi_event_mask_t)(SDL_INPUT_XI_EVENT_MASK_RAW_MOTION | SDL_INPUT_XI_EVENT_MASK_RAW_BUTTON_PRESS | SDL_INPUT_XI_EVENT_MASK_RAW_BUTTON_RELEASE) :
                    (SDL_input_xi_event_mask_t)SDL_NONE
            };

            const SDL_setup_t* SDLSetup = SDL_get_setup(connection);
            const SDL_screen_t* SDLScreen = SDL_setup_roots_iterator(SDLSetup).data;

            SDL_input_xi_select_events(connection, SDLScreen->root, 1, &mask.head);

            SDL_flush(connection);
        }

    private:
        SDLUniquePtr<SDL_connection_t, SDL_disconnect> m_SDLConnection = nullptr;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////
    SDLApplication::SDLApplication()
    {
        LinuxLifecycleEvents::Bus::Handler::BusConnect();
        m_SDLConnectionManager = AZStd::make_unique<SDLConnectionManagerImpl>();
        if (SDLConnectionManagerInterface::Get() == nullptr)
        {
            SDLConnectionManagerInterface::Register(m_SDLConnectionManager.get());
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    SDLApplication::~SDLApplication()
    {
        if (SDLConnectionManagerInterface::Get() == m_SDLConnectionManager.get())
        {
            SDLConnectionManagerInterface::Unregister(m_SDLConnectionManager.get());
        }
        m_SDLConnectionManager.reset();
        LinuxLifecycleEvents::Bus::Handler::BusDisconnect();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void SDLApplication::PumpSystemEventLoopOnce()
    {
        if (SDL_connection_t* SDLConnection = m_SDLConnectionManager->GetSDLConnection())
        {
            if (auto event = SDLStdFreePtr<SDL_generic_event_t>{SDL_poll_for_event(SDLConnection)})
            {
                SDLEventHandlerBus::Broadcast(&SDLEventHandlerBus::Events::HandleSDLEvent, event.get());
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void SDLApplication::PumpSystemEventLoopUntilEmpty()
    {
        if (SDL_connection_t* SDLConnection = m_SDLConnectionManager->GetSDLConnection())
        {
            while (auto event = SDLStdFreePtr<SDL_generic_event_t>{SDL_poll_for_event(SDLConnection)})
            {
                SDLEventHandlerBus::Broadcast(&SDLEventHandlerBus::Events::HandleSDLEvent, event.get());
            }
        }
    }

} // namespace AzFramework
