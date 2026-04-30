/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <Atom/RPI.Public/Pass/RenderPass.h>
#include <Atom/RPI.Public/Pass/Pass.h>
#include <Atom/RPI.Public/Pass/ParentPass.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>
#include <Atom/RPI.Public/Shader/Shader.h>

namespace AZ::Render
{
    // Parent pass that contains and creates all child passes for the Jump Flood Algorithm used for Outlines.
    // The number of passes depends on the max size of the outline (r_silhouetteMaxOutlineSize)
    class SilhouetteJFAStepParentPass
        : public RPI::ParentPass
    {
        AZ_RPI_PASS(SilhouetteJFAStepParentPass);

    public:
        AZ_RTTI(SilhouetteJFAStepParentPass, "{336A2AD3-4960-44F5-827A-A794D6B1E276}", RPI::ParentPass);
        AZ_CLASS_ALLOCATOR(SilhouetteJFAStepParentPass, SystemAllocator);
          
        //! Creates a new pass without a PassTemplate
        static RPI::Ptr<SilhouetteJFAStepParentPass> Create(const RPI::PassDescriptor& descriptor);
        virtual ~SilhouetteJFAStepParentPass();
            
    protected:
        explicit SilhouetteJFAStepParentPass(const RPI::PassDescriptor& descriptor);

        // Pass Behaviour Overrides...
        void ResetInternal() override;
        void BuildInternal() override;

        // ParentPass Behaviour Overrides...
        void CreateChildPassesInternal() override;

    private:

        const Name m_outputReferencePassName = Name{ "This" };
        const Name m_outputAttachmentName = Name{ "SilhouetteJFAOutput" };
        Name m_jfaImages[2] = { Name("SilhouetteJFAPing"), Name("SilhouetteJFAPong") };
    };
}   // namespace AZ::Render
