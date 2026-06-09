/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#if defined(CARBONATED)

#include <Atom/RPI.Public/Pass/Pass.h>

namespace AZ::Render
{
    //! This pass holds the image attachment for the previous frame that it's used in the reflections.
    //! This pass doesn't do the copy, only holds the image attachment. The copy pass cannot hold the image attachment
    //! because it's goes after the reflection passes and they need to reference it.
    //! Passes can reference this attachment either directly or by it's global name.
    class ReflectionPreviousFramePass
        : public RPI::Pass
    {
        AZ_RPI_PASS(ReflectionPreviousFramePass);

    public:
        AZ_RTTI(ReflectionPreviousFramePass, "{599743AF-C99E-47FD-A4C0-8F1AF86CC730}", RPI::Pass);
        AZ_CLASS_ALLOCATOR(ReflectionPreviousFramePass, SystemAllocator);

        //! Creates a new pass without a PassTemplate
        static RPI::Ptr<ReflectionPreviousFramePass> Create(const RPI::PassDescriptor& descriptor);

    private:
        explicit ReflectionPreviousFramePass(const RPI::PassDescriptor& descriptor);

        // Pass Overrides...
        void BuildInternal() override;

        Data::Instance<RPI::AttachmentImage> m_previousFrameImageAttachment;
    };
}   // namespace AZ::Render

#endif
