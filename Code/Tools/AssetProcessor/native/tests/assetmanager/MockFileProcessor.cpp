/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "MockFileProcessor.h"

namespace UnitTests
{
#if defined(CARBONATED) // Fix Warnings C4100 treated in VS17.14.x as errors.
    void MockFileProcessor::AssessAddedFile([[maybe_unused]] QString fileName)
#else
     void MockFileProcessor::AssessAddedFile(QString fileName)
#endif // defined(CARBONATED)
    {
        m_events[TestEvents::Added].Signal();
    }

#if defined(CARBONATED) // Fix Warnings C4100 treated in VS17.14.x as errors.
    void MockFileProcessor::AssessDeletedFile([[maybe_unused]] QString fileName)
#else
     void MockFileProcessor::AssessDeletedFile(QString fileName)
#endif // defined(CARBONATED)
    {
        m_events[TestEvents::Deleted].Signal();
    }
}
