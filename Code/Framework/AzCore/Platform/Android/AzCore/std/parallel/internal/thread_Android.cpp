/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/std/parallel/thread.h>

#include <sched.h>
#include <errno.h>

#if defined(CARBONATED)
#include <unordered_map>
#include <mutex>
#include <string>
#endif

namespace AZStd
{
    namespace Platform
    {
        void NameCurrentThread(const char*)
        {
            // Threads are named in PostCreateThread on Android
        }

        void PreCreateSetThreadAffinity(int , pthread_attr_t&)
        {
            // Unimplemented on Android
        }

        void SetThreadPriority([[maybe_unused]] int priority, pthread_attr_t&)
        {
            // (not supported at v r10d)
        }

        void PostCreateThread(pthread_t tId, const char* name, int)
        {
            pthread_setname_np(tId, name);
        }

        uint8_t GetDefaultThreadPriority()
        {
            // pthread priority is an integer between >=1 and <=99 (although only range 1<=>32 is guaranteed)
            // Don't use a scheduling policy value (e.g. SCHED_OTHER or SCHED_FIFO) here.
            return 1;
        }

#if defined(CARBONATED)

        // Global table  tid -> thread name
        static std::unordered_map<pid_t, std::string> g_tidToName;
        static std::mutex g_tidToNameMutex;

        void RegisterThreadName(pid_t tid, const char* name)
        {
            std::lock_guard<std::mutex> lock(g_tidToNameMutex);
            g_tidToName[tid] = name;
        }

        void UnregisterThreadName(pid_t tid)
        {
            std::lock_guard<std::mutex> lock(g_tidToNameMutex);
            g_tidToName.erase(tid);
        }

        std::string GetThreadName(pid_t tid)
        {
            std::lock_guard<std::mutex> lock(g_tidToNameMutex);
            auto it = g_tidToName.find(tid);
            return it != g_tidToName.end() ? it->second : "";
        }
#endif
    }
}
