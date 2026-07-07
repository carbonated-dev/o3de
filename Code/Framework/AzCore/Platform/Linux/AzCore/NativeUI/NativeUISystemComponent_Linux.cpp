/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/NativeUI/NativeUISystemComponent.h>

#include <AzCore/std/parallel/atomic.h>
#include <AzCore/Debug/Trace.h>

#include <SDL.h>

// CARBONATED: gives Linux (desktop, real X11/Wayland session via SDL) the same native modal
// dialog Windows already has via DialogBoxIndirectParam. This is a dev/test convenience for a
// Linux desktop machine only - it does NOT help OCGA itself, which has no window compositor to
// display this (or any) window at all. OCGA still needs the in-game UI path
// (Carb::AssertUIHandler / bg_useInGameAssertDialog).

namespace AZ
{
    namespace NativeUI
    {
#if defined(CARBONATED)
        static AZStd::atomic<unsigned int> blockingDialogCounter{ 0 };
        bool NativeUISystem::IsDisplayingBlockingDialog() const
        {
            return blockingDialogCounter.load() > 0;
        }
#endif

        AZStd::string NativeUISystem::DisplayBlockingDialog(const AZStd::string& title, const AZStd::string& message, const AZStd::vector<AZStd::string>& options) const
        {
            if (options.empty())
            {
                return "";
            }

            AZStd::vector<SDL_MessageBoxButtonData> buttons;
            buttons.reserve(options.size());
            for (size_t i = 0; i < options.size(); ++i)
            {
                SDL_MessageBoxButtonData button = {};
                button.buttonid = static_cast<int>(i);
                button.text = options[i].c_str();
                if (i == 0)
                {
                    button.flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
                }
                if (i == options.size() - 1)
                {
                    button.flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
                }
                buttons.push_back(button);
            }

            SDL_MessageBoxData messageBoxData = {};
            messageBoxData.flags = SDL_MESSAGEBOX_INFORMATION;
            messageBoxData.window = nullptr;
            messageBoxData.title = title.c_str();
            messageBoxData.message = message.c_str();
            messageBoxData.numbuttons = static_cast<int>(buttons.size());
            messageBoxData.buttons = buttons.data();
            messageBoxData.colorScheme = nullptr;

#if defined(CARBONATED)
            blockingDialogCounter.fetch_add(1);
#endif
            int chosenButtonId = -1;
            const int result = SDL_ShowMessageBox(&messageBoxData, &chosenButtonId);
#if defined(CARBONATED)
            blockingDialogCounter.fetch_sub(1);
#endif

            if (result != 0)
            {
                AZ_Warning("NativeUISystem", false, "SDL_ShowMessageBox failed: %s", SDL_GetError());
                return "";
            }

            if (chosenButtonId < 0 || chosenButtonId >= static_cast<int>(options.size()))
            {
                // Dialog was closed without choosing a button (e.g. window closed).
                return "";
            }

            return options[chosenButtonId];
        }
    } // namespace NativeUI
} // namespace AZ
