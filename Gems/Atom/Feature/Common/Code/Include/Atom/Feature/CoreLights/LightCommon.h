/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <AzCore/Math/Internal/MathTypes.h>
#include <Atom/Feature/Utils/GpuBufferHandler.h>
#include <Atom/RPI.Public/Base.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/Scene.h>

namespace AZ::Render::LightCommon
{
    inline float GetRadiusFromInvRadiusSquared(float invRadiusSqaured)
    {
        return (invRadiusSqaured <= 0.0f) ? 1.0f : Sqrt(1.0f / invRadiusSqaured);
    }

    // Check if a view is being used by a pipeline that has a GPU culling pass.
    bool HasGPUCulling(
        RPI::Scene* parentScene,
        const RPI::ViewPtr& view,
        AZStd::unordered_set<AZStd::pair<const RPI::RenderPipeline*, const RPI::View*>>& gpuCullingPasses);

    // Update gpuCullingData to hold information related to gpu culling passes in order to Check if render pipeline is using GPU culling
     void CacheGPUCullingPipelineInfo(
        RPI::RenderPipeline* renderPipeline,
        RPI::ViewPtr newView,
        RPI::ViewPtr previousView,
        AZStd::unordered_set<AZStd::pair<const RPI::RenderPipeline*, const RPI::View*>>& gpuCullingData);

     // Get or create a buffer that will be used for visibility when doing CPU culling.
     GpuBufferHandler& GetOrCreateVisibleBuffer(
         uint32_t& visibleBufferUsedCount,
         AZStd::vector<GpuBufferHandler>& visibleBufferHandlers,
         const AZStd::string_view& bufferName,
         const AZStd::string_view& bufferSrgName,
         const AZStd::string_view& elementCountSrgName);

} // namespace AZ::Render::LightCommon

