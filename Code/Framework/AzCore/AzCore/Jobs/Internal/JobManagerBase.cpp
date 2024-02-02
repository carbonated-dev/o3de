/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Debug/Profiler.h>
#include <AzCore/Jobs/Internal/JobManagerBase.h>
#include <AzCore/Jobs/Job.h>

using namespace AZ;
using namespace AZ::Internal;

void JobManagerBase::Process(Job* job)
{
#ifdef AZ_DEBUG_JOB_STATE
    AZ_Assert(job->m_state == Job::STATE_PENDING, "Job must be in pending state to be processed, current state %d", job->m_state);
    job->SetState(Job::STATE_PROCESSING);
#endif

    Job* dependent = job->GetDependent();
    bool isDelete = job->IsAutoDelete();

    const bool bDebug = (job->pDebugJob == job);

    AZ_PROFILE_INTERVAL_END(JobManagerDetailed, job);
    if (!job->IsCancelled())
    {
        AZ_PROFILE_SCOPE(AzCore, "AZ::JobManagerBase::Process Job");
        if (bDebug) job->bProcess = true;
        job->Process();
        if (bDebug)
        {
            AZ_Printf("assetdbg", "Job %d %d %d", job->bInsert, job->bPop, job->bProcess);
            printf("assetdbg Job %d %d %d\n", job->bInsert, job->bPop, job->bProcess);
        }
    }
    else
    {
        if (bDebug)
        {
            AZ_Printf("assetdbg", "Job cancelled");
            printf("assetdbg Job cancelled\n");
        }
    }

    if (isDelete)
    {
        delete job;
    }

    if (dependent)
    {
        dependent->DecrementDependentCount();
    }
}
