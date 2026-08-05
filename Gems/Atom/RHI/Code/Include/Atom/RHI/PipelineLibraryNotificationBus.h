/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <Atom/RHI/PipelineLibrary.h>
#include <AzCore/EBus/EBus.h>

namespace AZ::RHI
{
    class PipelineStateCache;

    //! Notifications about changes to pipeline libraries owned by PipelineStateCache.
    class PipelineLibraryNotification
        : public AZ::EBusTraits
    {
    public:
        virtual ~PipelineLibraryNotification() = default;

        //! Called immediately before a pipeline library is released by PipelineStateCache.
        virtual void OnPipelineLibraryRelease(
            const PipelineStateCache* pipelineStateCache, PipelineLibraryHandle pipelineLibraryHandle) = 0;

        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
    };

    using PipelineLibraryNotificationBus = AZ::EBus<PipelineLibraryNotification>;
} // namespace AZ::RHI
