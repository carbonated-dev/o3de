/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

namespace EMotionFX::Integration
{
    class CVars
    {
    public:
        static inline int emfx_updateEnabled = 1;
        static inline int emfx_ragdollManipulatorsEnabled = 1;
        static inline int emfx_actorRenderEnabled = 1;
        static inline int emfx_schedulerPrint = 0;      // When non-zero, prints scheduler step info each tick
        static inline int emfx_animGraphTickLog = 0;     // When non-zero, logs timestamp+thread for every anim graph tick
        static inline int emfx_transitionLog = 0;        // When non-zero, logs every anim graph state transition start
    };
};
