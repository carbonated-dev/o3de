/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/Feature/PostProcess/RadialBlur/RadialBlurConstants.h>
#include <AzCore/RTTI/RTTI.h>

namespace AZ
{
    namespace Render
    {
        //! Public interface for the radial blur post-processing settings.
        class RadialBlurSettingsInterface
        {
        public:
            AZ_RTTI(AZ::Render::RadialBlurSettingsInterface, "{4089D292-970F-4F5E-9C98-8DCF854A9F66}");

            //! Generated virtual getters and setters for all radial blur parameters.
#include <Atom/Feature/ParamMacros/StartParamFunctionsVirtual.inl>
#include <Atom/Feature/PostProcess/RadialBlur/RadialBlurParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

            //! Notify the owning post-process settings that radial blur configuration changed.
            virtual void OnConfigChanged() = 0;
        };
    } // namespace Render
} // namespace AZ
