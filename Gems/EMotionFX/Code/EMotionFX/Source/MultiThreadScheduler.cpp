/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// include the required headers
#include "MultiThreadScheduler.h"
#include "ActorManager.h"
#include "ActorInstance.h"
#include "AnimGraphInstance.h"
#if defined(CARBONATED)
#include "AnimGraph.h"
#include "Parameter/ValueParameter.h"
#endif
#include "Attachment.h"
#include "EMotionFXManager.h"
#include <EMotionFX/Source/Allocators.h>
#include <MCore/Source/LogManager.h>

#include <AzCore/Jobs/JobFunction.h>
#include <AzCore/Jobs/JobCompletion.h>
#include <AzCore/Jobs/JobManagerBus.h>
#include <AzCore/Jobs/JobContext.h>
#if defined(CARBONATED)
#include <AzCore/Time/ITime.h>
#include <AzCore/std/chrono/chrono.h>
#endif


namespace EMotionFX
{
#if defined(CARBONATED)
    bool MultiThreadScheduler::s_animGraphTickLogEnabled = false;
    bool MultiThreadScheduler::s_animGraphParamLogEnabled = false;
#endif    
    AZ_CLASS_ALLOCATOR_IMPL(MultiThreadScheduler, ActorUpdateAllocator)

    // constructor
    MultiThreadScheduler::MultiThreadScheduler()
        : ActorUpdateScheduler()
    {
        m_cleanTimer     = 0.0f; // time passed since last schedule cleanup, in seconds
        m_steps.reserve(1000);
    }


    // destructor
    MultiThreadScheduler::~MultiThreadScheduler()
    {
    }


    // create
    MultiThreadScheduler* MultiThreadScheduler::Create()
    {
        return aznew MultiThreadScheduler();
    }


    // clear the schedule
    void MultiThreadScheduler::Clear()
    {
        Lock();
        m_steps.clear();
        Unlock();
    }


    // add the actor instance dependencies to the schedule step
    void MultiThreadScheduler::AddDependenciesToStep(ActorInstance* instance, ScheduleStep* outStep)
    {
        MCORE_UNUSED(outStep);
        MCORE_UNUSED(instance);
    }


    // check if there is a matching dependency for a given actor
    bool MultiThreadScheduler::CheckIfHasMatchingDependency(ActorInstance* instance, ScheduleStep* step) const
    {
        MCORE_UNUSED(instance);
        MCORE_UNUSED(step);
        return false;
    }


    // log it, for debugging purposes
    void MultiThreadScheduler::Print()
    {
        // for all steps
        const size_t numSteps = m_steps.size();
        for (size_t i = 0; i < numSteps; ++i)
        {
            AZ_Printf("EMotionFX", "STEP %.3zu - %zu", i, m_steps[i].m_actorInstances.size());
        }

        AZ_Printf("EMotionFX", "---------");
    }


    void MultiThreadScheduler::RemoveEmptySteps()
    {
        // process all steps
        for (size_t s = 0; s < m_steps.size(); )
        {
            if (!m_steps[s].m_actorInstances.empty())
            {
                s++;
            }
            else
            {
                m_steps.erase(AZStd::next(begin(m_steps), s));
            }
        }
    }


    // execute the schedule
    void MultiThreadScheduler::Execute(float timePassedInSeconds)
    {
        MCore::LockGuardRecursive guard(m_mutex);

        size_t numSteps = m_steps.size();
        if (numSteps == 0)
        {
            return;
        }

        // check if we need to cleanup the schedule
        m_cleanTimer += timePassedInSeconds;
        if (m_cleanTimer >= 1.0f)
        {
            m_cleanTimer = 0.0f;
            RemoveEmptySteps();
            numSteps = m_steps.size();
        }

        //-----------------------------------------------------------

        // propagate root actor instance visibility to their attachments
        const ActorManager& actorManager = GetActorManager();
        const size_t numRootActorInstances = actorManager.GetNumRootActorInstances();
        for (size_t i = 0; i < numRootActorInstances; ++i)
        {
            ActorInstance* rootInstance = actorManager.GetRootActorInstance(i);
            if (rootInstance->GetIsEnabled() == false)
            {
                continue;
            }

            rootInstance->RecursiveSetIsVisible(rootInstance->GetIsVisible());
        }

        // reset stats
        m_numUpdated.SetValue(0);
        m_numVisible.SetValue(0);
        m_numSampled.SetValue(0);

        int i = 0;
        for (const ScheduleStep& currentStep : m_steps)
        {
            i++;
            if (currentStep.m_actorInstances.empty())
            {
                continue;
            }

            // process the actor instances in the current step in parallel
            AZ::JobCompletion jobCompletion;
            for (ActorInstance* actorInstance : currentStep.m_actorInstances)
            {
                if (actorInstance->GetIsEnabled() == false)
                {
                    continue;
                }

                AZ::JobContext* jobContext = nullptr;
                //AZ_Info("aaa", "Create job for step %d, actor %s, time %lld", i - 1, actorInstance->GetEntity()->GetName().c_str(), AZ::GetRealElapsedTimeUs());
                AZ::Job* job = AZ::CreateJobFunction([this, timePassedInSeconds, actorInstance]()
                {
                    AZ_PROFILE_SCOPE(Animation, "MultiThreadScheduler::Execute::ActorInstanceUpdateJob");
                    /*AZ_Info("aaa", "Begin execute job actor %s, threads %d, time %lld",
                        actorInstance->GetEntity()->GetName().c_str(),
                        AZ::JobContext::GetGlobalContext()->GetJobManager().GetNumWorkerThreads(),
                        AZ::GetRealElapsedTimeUs());*/

                    const AZ::u32 threadIndex = AZ::JobContext::GetGlobalContext()->GetJobManager().GetWorkerThreadId();                    
                    actorInstance->SetThreadIndex(threadIndex);

                    const bool isVisible = actorInstance->GetIsVisible();
                    if (isVisible)
                    {
                        m_numVisible.Increment();
                    }

                    // check if we want to sample motions
                    bool sampleMotions = false;
                    actorInstance->SetMotionSamplingTimer(actorInstance->GetMotionSamplingTimer() + timePassedInSeconds);
                    if (actorInstance->GetMotionSamplingTimer() >= actorInstance->GetMotionSamplingRate())
                    {
                        sampleMotions = true;
                        actorInstance->SetMotionSamplingTimer(0.0f);

                        if (isVisible)
                        {
                            m_numSampled.Increment();
                        }
                    }

                    // update the actor instance
#if defined(CARBONATED)                    
                    if (MultiThreadScheduler::s_animGraphTickLogEnabled)
                    {
                        const auto now = AZStd::chrono::steady_clock::now();
                        const double nowSec = AZStd::chrono::duration_cast<AZStd::chrono::nanoseconds>(now.time_since_epoch()).count() / 1.0e9;
                        MCore::LogInfo("[AnimGraphTick] actor=%p thread=%u time=%.3fs sampleMotions=%d",
                            actorInstance, threadIndex, nowSec, (int)sampleMotions);
                        // uncomment the block below to see warnings instead of LogInfo
                        /*
                        AZ_Warning(
                            "EMotionFX",
                            false,
                            "[AnimGraphTick] actor=%p thread=%u time=%.3fs sampleMotions=%d",
                            actorInstance,
                            threadIndex,
                            nowSec,
                            static_cast<int>(sampleMotions));
                        */
                    }

                    if (MultiThreadScheduler::s_animGraphParamLogEnabled)
                    {
                        AnimGraphInstance* animGraphInstance = actorInstance->GetAnimGraphInstance();
                        if (animGraphInstance)
                        {
                            const AnimGraph* animGraph = animGraphInstance->GetAnimGraph();
                            const size_t numParams = animGraph->GetNumValueParameters();
                            AZStd::string paramLog;
                            paramLog.reserve(256);
                            for (size_t p = 0; p < numParams; ++p)
                            {
                                const ValueParameter* param = animGraph->FindValueParameter(p);
                                MCore::Attribute* attr = animGraphInstance->GetParameterValue(p);
                                AZStd::string valueStr;
                                if (attr)
                                    attr->ConvertToString(valueStr);
                                if (p > 0)
                                    paramLog += ", ";
                                paramLog += param->GetName();
                                paramLog += "=";
                                paramLog += valueStr;
                            }
                            const auto now = AZStd::chrono::steady_clock::now();
                            const double nowSec = AZStd::chrono::duration_cast<AZStd::chrono::nanoseconds>(now.time_since_epoch()).count() / 1.0e9;
                            MCore::LogInfo("[AnimGraphParams] actor=%p time=%.3fs [%s]", actorInstance, nowSec, paramLog.c_str());
                        }
                    }
#endif

                    actorInstance->UpdateTransformations(timePassedInSeconds, isVisible, sampleMotions);
                    //AZ_Info("aaa", "End execute job actor %s, time %lld", actorInstance->GetEntity()->GetName().c_str(), AZ::GetRealElapsedTimeUs());
                }, true, jobContext);

                job->SetDependent(&jobCompletion);               
                job->Start();

                m_numUpdated.Increment();
            }

            jobCompletion.StartAndWaitForCompletion();
            //AZ_Info("aaa", "All jobs for step %d are done at %lld", i - 1, AZ::GetRealElapsedTimeUs());
        } // for all steps
    }


    // find the next free spot in the schedule
    bool MultiThreadScheduler::FindNextFreeItem(ActorInstance* actorInstance, size_t startStep, size_t* outStepNr)
    {
        // try out all steps
        const size_t numSteps = m_steps.size();
        for (size_t s = startStep; s < numSteps; ++s)
        {
            // if there is a conflicting dependency, skip this step
            if (CheckIfHasMatchingDependency(actorInstance, &m_steps[s]))
            {
                continue;
            }

            // we found a free spot!
            *outStepNr = s;
            return true;
        }

        *outStepNr = numSteps;
        return false;
    }

    bool MultiThreadScheduler::HasActorInstanceInSteps(const ActorInstance* actorInstance) const
    {
        const size_t numSteps = m_steps.size();
        for (size_t s = 0; s < numSteps; ++s)
        {
            const ScheduleStep& step = m_steps[s];
            if (AZStd::find(step.m_actorInstances.begin(), step.m_actorInstances.end(), actorInstance) != step.m_actorInstances.end())
            {
                return true;
            }
        }

        return false;
    }

    void MultiThreadScheduler::RecursiveInsertActorInstance(ActorInstance* instance, size_t startStep)
    {
        AZ_Info("aaa", "MultiThreadScheduler::RecursiveInsertActorInstance %s", instance->GetEntity()->GetName().c_str());
        MCore::LockGuardRecursive guard(m_mutex);
        AZ_Assert(!HasActorInstanceInSteps(instance), "Expected the actor instance not being part of another step already.");

        // find the first free location that doesn't conflict
        size_t outStep = startStep;
        if (!FindNextFreeItem(instance, startStep, &outStep))
        {
            m_steps.reserve(10);
            m_steps.emplace_back();
            outStep = m_steps.size() - 1;
        }

        AZ_Info("aaa", "  selected step %d", outStep);

        // pre-allocate step size
        if (m_steps[outStep].m_actorInstances.size() % 10 == 0)
        {
            m_steps[outStep].m_actorInstances.reserve(m_steps[outStep].m_actorInstances.size() + 10);
        }

        if (m_steps[outStep].m_dependencies.size() % 5 == 0)
        {
            m_steps[outStep].m_dependencies.reserve(m_steps[outStep].m_dependencies.size() + 5);
        }

        // add the actor instance and its dependencies
        m_steps[ outStep ].m_actorInstances.reserve(GetEMotionFX().GetNumThreads());
        m_steps[ outStep ].m_actorInstances.emplace_back(instance);
        AddDependenciesToStep(instance, &m_steps[outStep]);

        // recursively add all attachments too
        const size_t numAttachments = instance->GetNumAttachments();
        AZ_Info("aaa", "  add %d attachments to the next step %d", numAttachments, outStep + 1);
        for (size_t i = 0; i < numAttachments; ++i)
        {
            ActorInstance* attachment = instance->GetAttachment(i)->GetAttachmentActorInstance();
            if (attachment)
            {
                RecursiveInsertActorInstance(attachment, outStep + 1);
            }
        }
    }


    // remove the actor instance from the schedule (excluding attachments)
    size_t MultiThreadScheduler::RemoveActorInstance(ActorInstance* actorInstance, size_t startStep)
    {
        MCore::LockGuardRecursive guard(m_mutex);

        // for all scheduler steps, starting from the specified start step number
        const size_t numSteps = m_steps.size();
        for (size_t s = startStep; s < numSteps; ++s)
        {
            ScheduleStep& step = m_steps[s];

            // Remove all occurrences of the actor instance.
            const size_t numActorInstancesPreRemove = step.m_actorInstances.size();
            step.m_actorInstances.erase(AZStd::remove(step.m_actorInstances.begin(), step.m_actorInstances.end(), actorInstance), step.m_actorInstances.end());

            // try to see if there is anything to remove in this step
            // and if so, reconstruct the dependencies of this step
            if (step.m_actorInstances.size() < numActorInstancesPreRemove)
            {
                // clear the dependencies (but don't delete the memory)
                step.m_dependencies.clear();

                // calculate the new dependencies for this step
                for (ActorInstance* stepActorInstance : step.m_actorInstances)
                {
                    AddDependenciesToStep(stepActorInstance, &step);
                }

                // assume that there is only one of the same actor instance in the whole schedule
                RemoveEmptySteps();
                return s;
            }
        }

        return 0;
    }


    // remove the actor instance (including all of its attachments)
    void MultiThreadScheduler::RecursiveRemoveActorInstance(ActorInstance* actorInstance, size_t startStep)
    {
        MCore::LockGuardRecursive guard(m_mutex);

        // remove the actual actor instance
        const size_t step = RemoveActorInstance(actorInstance, startStep);

        // recursively remove all attachments as well
        const size_t numAttachments = actorInstance->GetNumAttachments();
        for (size_t i = 0; i < numAttachments; ++i)
        {
            ActorInstance* attachment = actorInstance->GetAttachment(i)->GetAttachmentActorInstance();
            if (attachment)
            {
                RecursiveRemoveActorInstance(attachment, step);
            }
        }
    }


    void MultiThreadScheduler::Lock()
    {
        m_mutex.Lock();
    }


    void MultiThreadScheduler::Unlock()
    {
        m_mutex.Unlock();
    }
}   // namespace EMotionFX
