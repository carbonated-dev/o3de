/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <tests/assetmanager/MockAssetProcessorManager.h>

namespace UnitTests
{
#if defined(CARBONATED) // Fix Warnings C4100 treated in VS17.14.x as errors.
    void MockAssetProcessorManager::AssessAddedFile([[maybe_unused]] QString filePath)
#else
    void MockAssetProcessorManager::AssessAddedFile(QString filePath)
#endif // defined(CARBONATED)
    {
        m_events[TestEvents::Added].Signal();
    }

#if defined(CARBONATED) // Fix Warnings C4100 treated in VS17.14.x as errors.
    void MockAssetProcessorManager::AssessModifiedFile([[maybe_unused]] QString filePath)
#else
    void MockAssetProcessorManager::AssessModifiedFile(QString filePath)
#endif // defined(CARBONATED)
    {
        m_events[TestEvents::Modified].Signal();
    }

#if defined(CARBONATED) // Fix Warnings C4100 treated in VS17.14.x as errors.
    void MockAssetProcessorManager::AssessDeletedFile([[maybe_unused]] QString filePath)
#else
    void MockAssetProcessorManager::AssessDeletedFile(QString filePath)
#endif // defined(CARBONATED)
    {
        m_events[TestEvents::Deleted].Signal();
    }
}
