/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzTest/AzTest.h>
#include <RHI/DescriptorPoolState.h>

AZ_UNIT_TEST_HOOK(DEFAULT_UNIT_TEST_ENV);

namespace AZ
{
    namespace Vulkan
    {
        namespace Internal
        {
            class DescriptorPoolStateTests
                : public ::testing::Test
            {
            protected:
                static DescriptorPool* MakePool(uintptr_t value)
                {
                    return reinterpret_cast<DescriptorPool*>(value);
                }
            };

            TEST_F(DescriptorPoolStateTests, CurrentPoolBecomesFullAndAvailableAgain)
            {
                DescriptorPoolState state;
                DescriptorPool* pool = MakePool(1);

                state.SetCurrent(pool);
                EXPECT_EQ(state.GetCurrent(), pool);

                state.MarkFull(pool);
                EXPECT_EQ(state.GetCurrent(), nullptr);
                EXPECT_EQ(state.GetCounts().m_full, 1);

                state.MarkPendingCollect(pool);
                EXPECT_EQ(state.GetCounts().m_pendingCollect, 1);

                state.ClearPendingCollect(pool);
                state.MarkAvailable(pool);
                EXPECT_EQ(state.TakeAvailable(), pool);
                EXPECT_EQ(state.GetCurrent(), pool);
            }

            TEST_F(DescriptorPoolStateTests, ReplacingCurrentPoolPreservesItsAvailableCapacity)
            {
                DescriptorPoolState state;
                DescriptorPool* firstPool = MakePool(1);
                DescriptorPool* secondPool = MakePool(2);

                state.SetCurrent(firstPool);
                state.SetCurrent(secondPool);

                EXPECT_EQ(state.GetCurrent(), secondPool);
                EXPECT_EQ(state.GetCounts().m_available, 1);
                EXPECT_EQ(state.TakeAvailable(), firstPool);
            }

            TEST_F(DescriptorPoolStateTests, RemovingPoolClearsEveryState)
            {
                DescriptorPoolState state;
                DescriptorPool* pool = MakePool(1);

                state.SetCurrent(pool);
                state.MarkPendingCollect(pool);
                state.Remove(pool);

                const DescriptorPoolState::Counts counts = state.GetCounts();
                EXPECT_EQ(counts.m_total, 0);
                EXPECT_EQ(counts.m_pendingCollect, 0);
                EXPECT_FALSE(counts.m_hasCurrent);
            }

            TEST_F(DescriptorPoolStateTests, ThousandsOfFullPoolsAreNotCandidatesForAllocation)
            {
                DescriptorPoolState state;
                constexpr uintptr_t PoolCount = 5000;
                for (uintptr_t index = 1; index <= PoolCount; ++index)
                {
                    state.MarkFull(MakePool(index));
                }

                EXPECT_EQ(state.GetCounts().m_full, PoolCount);
                EXPECT_EQ(state.GetCurrent(), nullptr);
                EXPECT_EQ(state.TakeAvailable(), nullptr);
            }
        } // namespace Internal
    } // namespace Vulkan
} // namespace AZ
