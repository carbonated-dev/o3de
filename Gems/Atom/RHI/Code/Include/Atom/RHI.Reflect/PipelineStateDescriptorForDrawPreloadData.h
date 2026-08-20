/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#if defined(CARBONATED) && defined(CARBONATED_SHADER_PRELOAD)

#include <Atom/RHI.Reflect/ConstantsLayout.h>
#include <Atom/RHI.Reflect/InputStreamLayout.h>
#include <Atom/RHI.Reflect/RenderAttachmentLayout.h>
#include <Atom/RHI.Reflect/RenderStates.h>

namespace AZ::RHI
{
    class PipelineStateDescriptorForDraw;

    struct PipelineStateDescriptorForDrawPreloadData
    {
        AZ_TYPE_INFO(PipelineStateDescriptorForDrawPreloadData, "{A7DE47C5-A9B5-4A83-BF79-81328E9FC3E9}");
        static void Reflect(ReflectContext* context);
        PipelineStateDescriptorForDrawPreloadData() = default;
        PipelineStateDescriptorForDrawPreloadData(
            const AZStd::string& shaderPath, const PipelineStateDescriptorForDraw& psd, const RenderStates& mainRenderStates);

        AZStd::string m_shaderPath;
        InputStreamLayout m_inputStreamLayout;
        RenderAttachmentConfiguration m_renderAttachmentConfiguration;
        RenderStates m_renderStates;
        RenderStates m_mainRenderStates;
    };
}

#endif
