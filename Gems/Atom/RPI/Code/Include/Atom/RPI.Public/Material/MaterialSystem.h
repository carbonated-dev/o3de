/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <Atom/RPI.Reflect/Asset/AssetHandler.h>

namespace AZ
{
    class ReflectContext;

    namespace RPI
    {
        class LuaMaterialBehaviorContext;

        //! Manages system-wide initialization and support for material classes
        class MaterialSystem
        {
        public:
            static void Reflect(AZ::ReflectContext* context);
            static void GetAssetHandlers(AssetHandlerPtrList& assetHandlers);

            void Init();
            void Shutdown();

        private:
            LuaMaterialBehaviorContext* m_luaMaterialBehaviorContext = nullptr;
        };

    } // namespace RPI
} // namespace AZ
