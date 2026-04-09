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
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void SDLApplication::PumpSystemEventLoopUntilEmpty()
    {
    }

} // namespace AzFramework
