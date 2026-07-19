/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/RPI.Public/Pass/ParentPass.h>
#include <AzCore/Memory/SystemAllocator.h>

namespace AZ::Render
{
    class OitMlabParentPass final
        : public RPI::ParentPass
    {
        using Base = RPI::ParentPass;
        AZ_RPI_PASS(OitMlabParentPass);

    public:
        AZ_RTTI(OitMlabParentPass, "{E7464887-8030-4456-B72B-D3B1E9AF091D}", Base);
        AZ_CLASS_ALLOCATOR(OitMlabParentPass, SystemAllocator);

        static RPI::Ptr<OitMlabParentPass> Create(const RPI::PassDescriptor& descriptor);

    private:
        explicit OitMlabParentPass(const RPI::PassDescriptor& descriptor);

        void BuildInternal() override;
        // The MLAB atlas stores N layers by stacking them vertically below each screen pixel.
        void SetMlabAtlasHeightMultiplier(AZ::Name attachmentName);

        const AZ::Name m_oitMlabFragmentColorImageName = AZ::Name("OitMlabFragmentColorImage");
        const AZ::Name m_oitMlabFragmentDepthTransmissionImageName = AZ::Name("OitMlabFragmentDepthTransmissionImage");
    };
} // namespace AZ::Render
