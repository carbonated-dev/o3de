/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <OrderIndependentTransparency/OitMlabParentPass.h>

#include <Atom/RPI.Public/Pass/PassAttachment.h>
#include <OrderIndependentTransparency/OrderIndependentTransparencyFeatureProcessor.h>

namespace AZ::Render
{
    OitMlabParentPass::OitMlabParentPass(const RPI::PassDescriptor& descriptor)
        : Base(descriptor)
    {
    }

    RPI::Ptr<OitMlabParentPass> OitMlabParentPass::Create(const RPI::PassDescriptor& descriptor)
    {
        return aznew OitMlabParentPass(descriptor);
    }

    void OitMlabParentPass::BuildInternal()
    {
        // Match the atlas texture height to r_oitMlabLayerCount before pass attachments are built.
        SetMlabAtlasHeightMultiplier(m_oitMlabFragmentColorImageName);
        SetMlabAtlasHeightMultiplier(m_oitMlabFragmentDepthTransmissionImageName);

        Base::BuildInternal();
    }

    void OitMlabParentPass::SetMlabAtlasHeightMultiplier(AZ::Name attachmentName)
    {
        if (RPI::Ptr<RPI::PassAttachment> attachment = GetOwnedAttachment(attachmentName))
        {
            attachment->m_sizeMultipliers.m_heightMultiplier = static_cast<float>(OrderIndependentTransparencyFeatureProcessor::GetMlabLayerCount());
            attachment->Update();
        }
    }
} // namespace AZ::Render
