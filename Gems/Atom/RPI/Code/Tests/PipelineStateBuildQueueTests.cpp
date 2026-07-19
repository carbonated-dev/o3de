/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Atom/RPI.Public/PipelineStateBuildQueue.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzTest/AzTest.h>

namespace UnitTest
{
    TEST(PipelineStateBuildRequestTests, BuildRunsDeferredPreparation)
    {
        bool preparationCalled = false;
        auto request = AZStd::make_shared<AZ::RPI::PipelineStateBuildRequest>(
            [&preparationCalled](
                AZ::RPI::PipelineStateBuildItemList& buildItems)
            {
                preparationCalled = true;
                EXPECT_TRUE(buildItems.empty());
                return true;
            });

        request->Build();

        EXPECT_TRUE(preparationCalled);
        EXPECT_TRUE(request->IsSuccessful());
    }

    TEST(PipelineStateBuildRequestTests, CancelSkipsDeferredPreparation)
    {
        bool preparationCalled = false;
        auto request = AZStd::make_shared<AZ::RPI::PipelineStateBuildRequest>(
            [&preparationCalled](
                AZ::RPI::PipelineStateBuildItemList&)
            {
                preparationCalled = true;
                return true;
            });

        request->Cancel();
        request->Build();

        EXPECT_FALSE(preparationCalled);
        EXPECT_EQ(
            request->GetState(),
            AZ::RPI::PipelineStateBuildRequest::State::Cancelled);
    }
} // namespace UnitTest
