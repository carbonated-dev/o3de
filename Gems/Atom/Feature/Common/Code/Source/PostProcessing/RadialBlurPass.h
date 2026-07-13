/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/RPI.Public/Pass/ComputePass.h>

namespace AZ
{
    namespace Render
    {
        //! Compute pass that applies the radial blur post-processing effect.
        class RadialBlurPass final : public RPI::ComputePass
        {
            AZ_RPI_PASS(RadialBlurPass);

        public:
            AZ_RTTI(RadialBlurPass, "{B28BFCDF-1415-4895-86E3-AFBF2A8C1298}", AZ::RPI::ComputePass);
            AZ_CLASS_ALLOCATOR(RadialBlurPass, SystemAllocator, 0);

            ~RadialBlurPass() = default;
            //! Create a radial blur pass from an RPI pass descriptor.
            static RPI::Ptr<RadialBlurPass> Create(const RPI::PassDescriptor& descriptor);

            bool IsEnabled() const override;

        private:
            RadialBlurPass(const RPI::PassDescriptor& descriptor);

            void FrameBeginInternal(FramePrepareParams params) override;

            AZ::RHI::ShaderInputNameIndex m_constantsIndex = "m_constants";
        };
    } // namespace Render
} // namespace AZ
