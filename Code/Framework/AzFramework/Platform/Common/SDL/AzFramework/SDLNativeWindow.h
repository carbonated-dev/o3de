/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzFramework/Application/Application.h>
#include <AzFramework/Windowing/NativeWindow.h>
#include <AzFramework/SDLEventHandler.h>

#include <SDL2/SDL.h>

namespace AzFramework
{
    class SDLNativeWindow final
        : public NativeWindow::Implementation
        , public SDLEventHandlerBus::Handler
    {
    public:
        AZ_CLASS_ALLOCATOR(SDLNativeWindow, AZ::SystemAllocator);
        SDLNativeWindow();
        ~SDLNativeWindow() override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        // NativeWindow::Implementation
        void InitWindowInternal(const AZStd::string& title, const WindowGeometry& geometry, const WindowStyleMasks& styleMasks) override;
        void Activate() override;
        void Deactivate() override;
        NativeWindowHandle GetWindowHandle() const override;
        void SetWindowTitle(const AZStd::string& title) override;
        void ResizeClientArea(WindowSize clientAreaSize, const WindowPosOptions& options) override;
        bool SupportsClientAreaResize() const override;
        uint32_t GetDisplayRefreshRate() const override;

        bool GetFullScreenState() const override;
        void SetFullScreenState(bool fullScreenState) override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        // SDLEventHandlerBus::Handler
        void HandleSDLEvent() override;

    private:
        void WindowSizeChanged(const uint32_t width, const uint32_t height);

        // Initialize all used atoms.
        void InitializeAtoms();
        void GetWMStates();

        bool m_fullscreenState = false;
        bool m_horizontalyMaximized = false;
        bool m_verticallyMaximized = false;

        SDL_Window* m_window = nullptr;
    };
} // namespace AzFramework
