/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include <RHI/DescriptorPoolState.h>

namespace AZ
{
    namespace Vulkan
    {
        namespace Internal
        {
            DescriptorPool* DescriptorPoolState::GetCurrent() const
            {
                return m_currentPool;
            }

            DescriptorPool* DescriptorPoolState::TakeAvailable()
            {
                if (m_availablePools.empty())
                {
                    return nullptr;
                }

                auto availablePool = m_availablePools.begin();
                DescriptorPool* pool = *availablePool;
                m_availablePools.erase(availablePool);
                m_currentPool = pool;
                return pool;
            }

            void DescriptorPoolState::SetCurrent(DescriptorPool* pool)
            {
                if (!pool)
                {
                    return;
                }

                if (m_currentPool && m_currentPool != pool)
                {
                    m_availablePools.insert(m_currentPool);
                }

                m_allPools.insert(pool);
                m_availablePools.erase(pool);
                m_fullPools.erase(pool);
                m_currentPool = pool;
            }

            void DescriptorPoolState::MarkAvailable(DescriptorPool* pool)
            {
                if (!pool)
                {
                    return;
                }

                m_allPools.insert(pool);
                m_fullPools.erase(pool);
                if (m_currentPool != pool)
                {
                    m_availablePools.insert(pool);
                }
            }

            void DescriptorPoolState::MarkFull(DescriptorPool* pool)
            {
                if (!pool)
                {
                    return;
                }

                m_allPools.insert(pool);
                m_availablePools.erase(pool);
                if (m_currentPool == pool)
                {
                    m_currentPool = nullptr;
                }
                m_fullPools.insert(pool);
            }

            void DescriptorPoolState::MarkPendingCollect(DescriptorPool* pool)
            {
                if (pool)
                {
                    m_pendingCollectPools.insert(pool);
                }
            }

            void DescriptorPoolState::ClearPendingCollect(DescriptorPool* pool)
            {
                m_pendingCollectPools.erase(pool);
            }

            void DescriptorPoolState::Remove(DescriptorPool* pool)
            {
                if (m_currentPool == pool)
                {
                    m_currentPool = nullptr;
                }
                m_allPools.erase(pool);
                m_availablePools.erase(pool);
                m_fullPools.erase(pool);
                m_pendingCollectPools.erase(pool);
            }

            void DescriptorPoolState::Reset()
            {
                m_currentPool = nullptr;
                m_allPools.clear();
                m_availablePools.clear();
                m_fullPools.clear();
                m_pendingCollectPools.clear();
            }

            const AZStd::unordered_set<DescriptorPool*>& DescriptorPoolState::GetAllPools() const
            {
                return m_allPools;
            }

            const AZStd::unordered_set<DescriptorPool*>& DescriptorPoolState::GetPendingCollectPools() const
            {
                return m_pendingCollectPools;
            }

            DescriptorPoolState::Counts DescriptorPoolState::GetCounts() const
            {
                Counts counts;
                counts.m_total = m_allPools.size();
                counts.m_available = m_availablePools.size();
                counts.m_full = m_fullPools.size();
                counts.m_pendingCollect = m_pendingCollectPools.size();
                counts.m_hasCurrent = m_currentPool != nullptr;
                return counts;
            }
        } // namespace Internal
    } // namespace Vulkan
} // namespace AZ
