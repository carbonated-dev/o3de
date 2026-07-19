/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <AzCore/std/containers/unordered_set.h>

namespace AZ
{
    namespace Vulkan
    {
        class DescriptorPool;

        namespace Internal
        {
            //! Tracks descriptor pools by allocation state so the allocation path never needs to
            //! search pools that are already known to be full.
            class DescriptorPoolState final
            {
            public:
                struct Counts
                {
                    size_t m_total = 0;
                    size_t m_available = 0;
                    size_t m_full = 0;
                    size_t m_pendingCollect = 0;
                    bool m_hasCurrent = false;
                };

                DescriptorPool* GetCurrent() const;
                DescriptorPool* TakeAvailable();

                void SetCurrent(DescriptorPool* pool);
                void MarkAvailable(DescriptorPool* pool);
                void MarkFull(DescriptorPool* pool);
                void MarkPendingCollect(DescriptorPool* pool);
                void ClearPendingCollect(DescriptorPool* pool);
                void Remove(DescriptorPool* pool);
                void Reset();

                const AZStd::unordered_set<DescriptorPool*>& GetAllPools() const;
                const AZStd::unordered_set<DescriptorPool*>& GetPendingCollectPools() const;
                Counts GetCounts() const;

            private:
                DescriptorPool* m_currentPool = nullptr;
                AZStd::unordered_set<DescriptorPool*> m_allPools;
                AZStd::unordered_set<DescriptorPool*> m_availablePools;
                AZStd::unordered_set<DescriptorPool*> m_fullPools;
                AZStd::unordered_set<DescriptorPool*> m_pendingCollectPools;
            };
        } // namespace Internal
    } // namespace Vulkan
} // namespace AZ
