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

#include <xcb/xinput.h>

namespace AzFramework
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    class SDLConnectionManagerImpl
        : public SDLConnectionManagerBus::Handler
    {
    public:
        SDLConnectionManagerImpl()
            : m_xcbConnection(xcb_connect(nullptr, nullptr))
        {
            AZ_Error("Application", m_xcbConnection != nullptr, "Unable to connect to X11 Server.");
            SDLConnectionManagerBus::Handler::BusConnect();
        }

        ~SDLConnectionManagerImpl() override
        {
            SDLConnectionManagerBus::Handler::BusDisconnect();
        }

        xcb_connection_t* GetXcbConnection() const override
        {
            return m_xcbConnection.get();
        }

        void SetEnableXInput(xcb_connection_t* connection, bool enable) override
        {
            struct Mask
            {
                xcb_input_event_mask_t head;
                xcb_input_xi_event_mask_t mask;
            };
            const Mask mask {
                /*.head=*/{
                    /*.device_id=*/XCB_INPUT_DEVICE_ALL_MASTER,
                    /*.mask_len=*/1
                },
                /*.mask=*/ enable ?
                    (xcb_input_xi_event_mask_t)(XCB_INPUT_XI_EVENT_MASK_RAW_MOTION | XCB_INPUT_XI_EVENT_MASK_RAW_BUTTON_PRESS | XCB_INPUT_XI_EVENT_MASK_RAW_BUTTON_RELEASE) :
                    (xcb_input_xi_event_mask_t)XCB_NONE
            };

            const xcb_setup_t* xcbSetup = xcb_get_setup(connection);
            const xcb_screen_t* xcbScreen = xcb_setup_roots_iterator(xcbSetup).data;

            xcb_input_xi_select_events(connection, xcbScreen->root, 1, &mask.head);

            xcb_flush(connection);
        }

    private:
        SDLUniquePtr<xcb_connection_t, xcb_disconnect> m_xcbConnection = nullptr;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////
    SDLApplication::SDLApplication()
    {
        LinuxLifecycleEvents::Bus::Handler::BusConnect();
        m_sdlConnectionManager = AZStd::make_unique<SDLConnectionManagerImpl>();
        if (SDLConnectionManagerInterface::Get() == nullptr)
        {
            SDLConnectionManagerInterface::Register(m_sdlConnectionManager.get());
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    SDLApplication::~SDLApplication()
    {
        if (SDLConnectionManagerInterface::Get() == m_sdlConnectionManager.get())
        {
            SDLConnectionManagerInterface::Unregister(m_sdlConnectionManager.get());
        }
        m_sdlConnectionManager.reset();
        LinuxLifecycleEvents::Bus::Handler::BusDisconnect();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void SDLApplication::PumpSystemEventLoopOnce()
    {
        if (xcb_connection_t* xcbConnection = m_sdlConnectionManager->GetXcbConnection())
        {
            if (auto event = SDLStdFreePtr<xcb_generic_event_t>{xcb_poll_for_event(xcbConnection)})
            {
                SDLEventHandlerBus::Broadcast(&SDLEventHandlerBus::Events::HandleSDLEvent, event.get());
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void SDLApplication::PumpSystemEventLoopUntilEmpty()
    {
        if (xcb_connection_t* xcbConnection = m_sdlConnectionManager->GetXcbConnection())
        {
            while (auto event = SDLStdFreePtr<xcb_generic_event_t>{xcb_poll_for_event(xcbConnection)})
            {
                SDLEventHandlerBus::Broadcast(&SDLEventHandlerBus::Events::HandleSDLEvent, event.get());
            }
        }
    }

} // namespace AzFramework
