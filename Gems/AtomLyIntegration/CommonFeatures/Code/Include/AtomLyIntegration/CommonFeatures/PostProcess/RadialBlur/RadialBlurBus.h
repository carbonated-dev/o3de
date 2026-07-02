/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/Feature/PostProcess/RadialBlur/RadialBlurConstants.h>
#include <AzCore/Component/Component.h>

namespace AZ
{
    namespace Render
    {
        //! Request bus for controlling radial blur component settings from script or code.
        class RadialBlurRequests : public ComponentBus
        {
        public:
            AZ_RTTI(AZ::Render::RadialBlurRequests, "{B4DA79B4-0999-4DAB-83A7-CF8C0A81C219}");

            static const EBusHandlerPolicy HandlerPolicy = EBusHandlerPolicy::Single;
            virtual ~RadialBlurRequests() = default;

            //! Generated virtual getters and setters for all radial blur component parameters.
#include <Atom/Feature/ParamMacros/StartParamFunctionsVirtual.inl>
#include <Atom/Feature/PostProcess/RadialBlur/RadialBlurParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
        };

        //! EBus used to send radial blur requests to a component on an entity.
        using RadialBlurRequestBus = AZ::EBus<RadialBlurRequests>;
    } // namespace Render
} // namespace AZ
