/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <OrderIndependentTransparency/OitMboitParentPass.h>

#include <Atom/RPI.Public/Pass/PassAttachment.h>
#include <OrderIndependentTransparency/OrderIndependentTransparencyFeatureProcessor.h>

namespace AZ::Render
{
    OitMboitParentPass::OitMboitParentPass(const RPI::PassDescriptor& descriptor)
        : Base(descriptor)
    {
    }

    RPI::Ptr<OitMboitParentPass> OitMboitParentPass::Create(const RPI::PassDescriptor& descriptor)
    {
        return aznew OitMboitParentPass(descriptor);
    }

    void OitMboitParentPass::BuildInternal()
    {
        if (RPI::Ptr<RPI::PassAttachment> attachment = GetOwnedAttachment(m_oitMboitMomentsImageName))
        {
            // Four-moment mode uses one full-resolution slice; six-moment mode needs a second slice in the same texture.
            attachment->m_sizeMultipliers.m_heightMultiplier =
                OrderIndependentTransparencyFeatureProcessor::GetMboitMomentCount() > 4 ? 2.0f : 1.0f;
            attachment->Update();
        }

        Base::BuildInternal();
    }
} // namespace AZ::Render
