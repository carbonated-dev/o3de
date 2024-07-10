/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>

namespace Profiler
{
#if defined (CARBONATED)
    struct ProfilerExternalTimingData
    {
        struct TimingEntry
        {
            const char* m_regionName; // it should be a pointer to the const static string
            AZStd::string m_groupName;
            uint64_t m_startTick = 0;
            uint64_t m_endTick = 0;
        };
        AZStd::vector<TimingEntry> m_timingEntries;
    };
#endif

    class ProfilerImGuiRequests
    {
    public:
        AZ_RTTI(ProfilerImGuiRequests, "{E0443400-D108-4D3F-8FF5-4F076FCF6D13}");
        virtual ~ProfilerImGuiRequests() = default;

        // special request to render the CPU profiler window in a non-standard way
        // e.g not through ImGuiUpdateListenerBus::OnImGuiUpdate
        virtual void ShowCpuProfilerWindow(bool& keepDrawing) = 0;

#if defined (CARBONATED)
        virtual void AddExternalProfilerTimingData(const ProfilerExternalTimingData&) {}
#endif
    };

    using ProfilerImGuiInterface = AZ::Interface<ProfilerImGuiRequests>;
} // namespace Profiler
