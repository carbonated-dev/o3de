/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// carbonated begin enable_carbonated_2: Methods called from o3de-gruber
#pragma once
#if defined(CARBONATED)
#include <AzCore/EBus/EBus.h>
#include <AzCore/Component/ComponentBus.h>

namespace EMotionFX
{
    // Networking request bus
    class AnimGraphNetSyncRequests
        : public AZ::ComponentBus
    {
    public:

        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        // Verbose names so as not to conflict with widely popular Paused/Resume/IsActive naming
        virtual void PauseClientSync() = 0;
        virtual void ResumeClientSync() = 0;
        virtual bool IsClientSyncActive() = 0;

    };

    using AnimGraphNetSyncRequestBus = AZ::EBus<AnimGraphNetSyncRequests>;
}
#endif
// carbonated end enable_carbonated_2
