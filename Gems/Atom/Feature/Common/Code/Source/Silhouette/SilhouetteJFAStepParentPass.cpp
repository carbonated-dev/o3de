/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <math.h>

#include <Atom/RPI.Public/Pass/PassDefines.h>
#include <Atom/RPI.Public/Pass/PassFactory.h>
#include <Atom/RPI.Public/Pass/PassSystemInterface.h>
#include <Atom/RPI.Public/Pass/PassUtils.h>
#include <Atom/RPI.Public/RenderPipeline.h>

#include <AzCore/Console/IConsole.h>

#include <Silhouette/SilhouetteJFAStepParentPass.h>

namespace AZ::Render
{
    RPI::Ptr<SilhouetteJFAStepParentPass> SilhouetteJFAStepParentPass::Create(const RPI::PassDescriptor& descriptor)
    {
        RPI::Ptr<SilhouetteJFAStepParentPass> pass = aznew SilhouetteJFAStepParentPass(descriptor);
        return pass;
    }

    SilhouetteJFAStepParentPass::SilhouetteJFAStepParentPass(const RPI::PassDescriptor& descriptor)
        : ParentPass(descriptor)
    {
    }

    SilhouetteJFAStepParentPass::~SilhouetteJFAStepParentPass()
    {
    }
    
    void SilhouetteJFAStepParentPass::ResetInternal()
    {
        RemoveChildren();
        m_flags.m_createChildren = true;
    }

    // Pass behavior functions...

    void SilhouetteJFAStepParentPass::BuildInternal()
    {
        ParentPass::BuildInternal();

        if (!m_children.empty())
        {
            // Set the last pass output as the Parent output
            RPI::PassConnection inConnection;
            inConnection.m_localSlot = m_outputAttachmentName;
            inConnection.m_attachmentRef.m_pass = m_children.back()->GetName();
            inConnection.m_attachmentRef.m_attachment = Name("JFAOutput");
            ProcessConnection(inConnection);
        }
    }

    // ParentPass Behaviour functions...

    void SilhouetteJFAStepParentPass::CreateChildPassesInternal()
    {
        RPI::PassSystemInterface* passSystem = RPI::PassSystemInterface::Get();

        const auto jfaStepTemplateName = Name("SilhouetteJFAStepPassTemplate");

        uint32_t maxOutline = 1;
        if (auto* console = AZ::Interface<AZ::IConsole>::Get(); console != nullptr)
        {
            console->GetCvarValue("r_silhouetteMaxOutlineSize", maxOutline);
        }
        // First calculates how many passes we are going to need based on the max size of the outline
        int numMips = aznumeric_cast<int>(AZStd::ceil(log2(static_cast<float>(maxOutline) + 1.0f)));
        int jfaIter = numMips - 1;

        RHI::ShaderInputNameIndex stepInputIndex = RHI::ShaderInputNameIndex("m_stepSize");
        // Create the child passes. Each of them will do a JFA step with different step values.
        for (int i = jfaIter; i >= 0; i--)
        {
            uint32_t inputIndex = (jfaIter - i) % 2;
            uint32_t outputIndex = (inputIndex + 1) % 2;

            RPI::PassRequest jfaPassRequest;
            jfaPassRequest.m_passName = Name(AZStd::string::format("StencilJfaStep%d", jfaIter - i + 1));
            jfaPassRequest.m_templateName = jfaStepTemplateName;

            jfaPassRequest.AddInputConnection(
                RPI::PassConnection{ Name("PrevJFA"), RPI::PassAttachmentRef{ Name("Parent"), m_jfaImages[inputIndex] } });
            jfaPassRequest.AddInputConnection(
                RPI::PassConnection{ Name("JFAOutput"), RPI::PassAttachmentRef{ Name("Parent"), m_jfaImages[outputIndex] } });
            auto childPass = passSystem->CreatePassFromRequest(&jfaPassRequest);
            if (!childPass)
            {
                AZ_Error("SilhouetteJFAStepParentPass", false, "Failed to create child pass");
                break;
            }

            float stepUV = static_cast<float>(pow(2, i)) + 0.5f;
            RPI::RenderPass* renderPass = static_cast<AZ::RPI::RenderPass*>(childPass.get());
            auto srg = renderPass->GetShaderResourceGroup();
            if (srg)
            {
                srg->SetConstant(stepInputIndex, stepUV);
            }

            AddChild(childPass);
        }
    }
}   // namespace AZ::Render
