/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzFramework/Application/Application.h>
#include <AzFramework/Windowing/NativeWindow.h>
#include <AzFramework/SDLConnectionManager.h>
#include <AzFramework/SDLInterface.h>
#include <AzFramework/SDLNativeWindow.h>

namespace AzFramework
{
    [[maybe_unused]] const char SDLXcbErrorWindow[] = "SDLNativeWindow";
    static constexpr uint8_t s_SDLFormatDataSize = 32; // Format indicator for xcb for client messages
    static constexpr uint16_t s_DefaultSDLWindowBorderWidth = 4; // The default border with in pixels if a border was specified

#define _NET_WM_STATE_REMOVE 0l
#define _NET_WM_STATE_ADD 1l
#define _NET_WM_STATE_TOGGLE 2l

    ////////////////////////////////////////////////////////////////////////////////////////////////
    SDLNativeWindow::SDLNativeWindow()
        : NativeWindow::Implementation()
    {
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    SDLNativeWindow::~SDLNativeWindow()
    {
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void SDLNativeWindow::InitWindowInternal(const AZStd::string& title, const WindowGeometry& geometry, const WindowStyleMasks& styleMasks)
    {
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////

    void SDLNativeWindow::InitializeAtoms()
    {
    }

    void SDLNativeWindow::GetWMStates()
    {
        m_fullscreenState = false;
        m_horizontalyMaximized = false;
        m_verticallyMaximized = false;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void SDLNativeWindow::Activate()
    {
        SDLEventHandlerBus::Handler::BusConnect();

        if (!m_activated) // nothing to do if window was already activated
        {
            m_activated = true;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void SDLNativeWindow::Deactivate()
    {
        if (m_activated) // nothing to do if window was already deactivated
        {
            m_activated = false;
        }
        SDLEventHandlerBus::Handler::BusDisconnect();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    NativeWindowHandle SDLNativeWindow::GetWindowHandle() const
    {
        return nullptr;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void SDLNativeWindow::SetWindowTitle(const AZStd::string& title)
    {
        // Set the title of both the window and the task bar by using
        // a buffer to hold the title twice, separated by a null-terminator
        auto doubleTitleSize = (title.size() + 1) * 2;
        AZStd::string doubleTitle(doubleTitleSize, '\0');
        azstrncpy(doubleTitle.data(), doubleTitleSize, title.c_str(), title.size());
        azstrncpy(&doubleTitle.data()[title.size() + 1], title.size(), title.c_str(), title.size());
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void SDLNativeWindow::ResizeClientArea(WindowSize clientAreaSize, const WindowPosOptions& options)
    {
        //Notify the RHI to rebuild swapchain and swapchain images after updating the surface
        WindowSizeChanged(clientAreaSize.m_width, clientAreaSize.m_height);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    bool SDLNativeWindow::SupportsClientAreaResize() const
    {
        return true;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    uint32_t SDLNativeWindow::GetDisplayRefreshRate() const
    {
        // [GFX TODO][GHI - 2678]
        // Using 60 for now until proper support is added

        return 60;
    }

    bool SDLNativeWindow::GetFullScreenState() const
    {
        return m_fullscreenState;
    }

    void SDLNativeWindow::SetFullScreenState(bool fullScreenState)
    {
        // TODO This is a pretty basic full-screen implementation using WM's _NET_WM_STATE_FULLSCREEN state.
        // Do we have to provide also the old way?

        GetWMStates();

        m_fullscreenState = fullScreenState;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void SDLNativeWindow::HandleSDLEvent()
    {
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void SDLNativeWindow::WindowSizeChanged(const uint32_t width, const uint32_t height)
    {
        if (m_width != width || m_height != height)
        {
            m_width = width;
            m_height = height;

            if (m_activated)
            {
            }
        }
    }
} // namespace AzFramework
