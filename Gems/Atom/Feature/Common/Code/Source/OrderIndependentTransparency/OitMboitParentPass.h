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
    class OitMboitParentPass final
        : public RPI::ParentPass
    {
        using Base = RPI::ParentPass;
        AZ_RPI_PASS(OitMboitParentPass);

    public:
        AZ_RTTI(OitMboitParentPass, "{90E67B3E-B42B-4246-8C9E-0A18E5662B24}", Base);
        AZ_CLASS_ALLOCATOR(OitMboitParentPass, SystemAllocator);

        static RPI::Ptr<OitMboitParentPass> Create(const RPI::PassDescriptor& descriptor);

    private:
        explicit OitMboitParentPass(const RPI::PassDescriptor& descriptor);

        // Four moments need one screen slice; six moments need a second vertical slice.
        void BuildInternal() override;

        const AZ::Name m_oitMboitMomentsImageName = AZ::Name("OitMboitMomentsImage");
    };
} // namespace AZ::Render
