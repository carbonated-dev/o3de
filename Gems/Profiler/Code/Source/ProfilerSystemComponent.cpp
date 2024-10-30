/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <ProfilerSystemComponent.h>

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <AzCore/Serialization/Json/JsonSerializationSettings.h>
#include <AzCore/Serialization/Json/JsonUtils.h>
#include <AzCore/Serialization/SerializeContext.h>

#if defined(CARBONATED) && (defined(AZ_PLATFORM_IOS) || defined(AZ_PLATFORM_ANDROID))
#include <AzCore/std/time.h>
#endif

namespace Profiler
{
    static constexpr AZ::Crc32 profilerServiceCrc = AZ_CRC_CE("ProfilerService");

    struct DelayedFunction
    {
        using func_type = AZStd::function<void()>;

        DelayedFunction(int framesToDelay, func_type&& function)
            : m_function(AZStd::move(function))
            , m_framesLeft(framesToDelay)
        {
        }

        void Run()
        {
            if (--m_framesLeft <= 0)
            {
                m_function();
            }
            else
            {
                AZ::SystemTickBus::QueueFunction(
                    [](DelayedFunction&& delayedFunc)
                    {
                        delayedFunc.Run();
                    },
                    AZStd::move(*this)
                );
            }
        }

        func_type m_function;
        int m_framesLeft{ 0 };
    };

#if defined(CARBONATED) && (defined(AZ_PLATFORM_IOS) || defined(AZ_PLATFORM_ANDROID))
    // the original serializer crahes the app because out of memory
    // this one produces a pretty badly formated result, but it is super fast, consumes no memory, saves space on disk
    AZ::Outcome<void, AZStd::string> PlainSave(const AZStd::string& filePath, const AZStd::ring_buffer<TimeRegionMap>& data)
    {
        AZ::IO::FileIOStream outputFileStream;
        if (!outputFileStream.Open(filePath.c_str(), AZ::IO::OpenMode::ModeWrite | AZ::IO::OpenMode::ModeCreatePath | AZ::IO::OpenMode::ModeText))
        {
            return AZ::Failure(AZStd::string::format("Error opening file '%s' for writing", filePath.c_str()));
        }
        const char* prefix = "{\"Type\":\"JsonSerialization\",\"Version\":1,\"ClassName\":\"CpuProfilingStatisticsSerializer\","
                             "\"ClassData\":{\"cpuProfilingStatisticsSerializerEntries\":[\n";
        outputFileStream.Write(strlen(prefix), prefix);

        bool isFirst = true;
        for (const auto& timeRegionMap : data)
        {
            for (const auto& [threadId, regionMap] : timeRegionMap)
            {
                const auto threadIdHash = AZStd::hash<AZStd::thread_id>{}(threadId);
                for (const auto& [regionName, regionVec] : regionMap)
                {
                    for (const auto& region : regionVec)
                    {
                        char buf[1024];  // function names can be pretty long, it overflows 256
                        constexpr const char* format = ",{\"groupName\":\"%s\","
                                             "\"regionName\":\"%s\","
                                             "\"stackDepth\":%u,"
                                             "\"startTick\":%lli,"
                                             "\"endTick\":%lli,"
                                             "\"threadId\":%zu}\n";
                        const char* regionGroupName = region.m_groupRegionName.m_groupName;
                        const char* regionRegionName = region.m_groupRegionName.m_regionName.GetCStr();
                        const int n = snprintf(buf, sizeof(buf), format,
                                              regionGroupName,
                                              regionRegionName,
                                              region.m_stackDepth,
                                              region.m_startTick,
                                              region.m_endTick,
                                              threadIdHash);
                        if (isFirst)
                        {
                            outputFileStream.Write(n - 1, buf + 1);
                            isFirst = false;
                        }
                        else
                        {
                            outputFileStream.Write(n, buf);
                        }
                }
                }
            }
        }
        constexpr const char* postfixFormat = "],\"timeTicksPerSecond\":%llu}}";
        char buf[64];
        const int n = snprintf(buf, sizeof(buf), postfixFormat, AZStd::GetTimeTicksPerSecond());
        outputFileStream.Write(n, buf);

        return AZ::Success();
    }
#endif

    bool SerializeCpuProfilingData(const AZStd::ring_buffer<TimeRegionMap>& data, AZStd::string outputFilePath, bool wasEnabled)
    {
        AZ_TracePrintf("ProfilerSystemComponent", "Beginning serialization of %zu frames of profiling data\n", data.size());
        AZ::JsonSerializerSettings serializationSettings;
        serializationSettings.m_keepDefaults = true;

#if defined(CARBONATED) && (defined(AZ_PLATFORM_IOS) || defined(AZ_PLATFORM_ANDROID))
        const auto saveResult = PlainSave(outputFilePath, data);
#else
        CpuProfilingStatisticsSerializer serializer(data);

        const auto saveResult = AZ::JsonSerializationUtils::SaveObjectToFile(&serializer,
            outputFilePath, (CpuProfilingStatisticsSerializer*)nullptr, &serializationSettings);
#endif

        AZStd::string captureInfo = outputFilePath;
        if (!saveResult.IsSuccess())
        {
            captureInfo = AZStd::string::format("Failed to save Cpu Profiling Statistics data to file '%s'. Error: %s",
                outputFilePath.c_str(),
                saveResult.GetError().c_str());
            AZ_Warning("ProfilerSystemComponent", false, captureInfo.c_str());
        }
        else
        {
            AZ_Printf("ProfilerSystemComponent", "Cpu profiling statistics was saved to file [%s]\n", outputFilePath.c_str());
        }

        // Disable the profiler again
        if (!wasEnabled)
        {
            AZ::Debug::ProfilerSystemInterface::Get()->SetActive(false);
        }

        // Notify listeners that the profiler capture has finished.
        AZ::Debug::ProfilerNotificationBus::Broadcast(&AZ::Debug::ProfilerNotificationBus::Events::OnCaptureFinished,
            saveResult.IsSuccess(),
            captureInfo);

        return saveResult.IsSuccess();
    }

    void ProfilerSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<ProfilerSystemComponent, AZ::Component>()
                ->Version(0);

            if (AZ::EditContext* ec = serialize->GetEditContext())
            {
                ec->Class<ProfilerSystemComponent>("Profiler", "Provides a custom implementation of the AZ::Debug::Profiler interface for capturing performance data")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true);
            }
        }

        CpuProfilingStatisticsSerializer::Reflect(context);
    }

    void ProfilerSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(profilerServiceCrc);
    }

    void ProfilerSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(profilerServiceCrc);
    }

    void ProfilerSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
    }

    void ProfilerSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
    }

    ProfilerSystemComponent::ProfilerSystemComponent()
    {
        if (AZ::Debug::ProfilerSystemInterface::Get() == nullptr)
        {
            AZ::Debug::ProfilerSystemInterface::Register(this);
        }
    }

    ProfilerSystemComponent::~ProfilerSystemComponent()
    {
        if (AZ::Debug::ProfilerSystemInterface::Get() == this)
        {
            AZ::Debug::ProfilerSystemInterface::Unregister(this);
        }
    }

    void ProfilerSystemComponent::Activate()
    {
        m_cpuProfiler.Init();
    }

    void ProfilerSystemComponent::Deactivate()
    {
        m_cpuProfiler.Shutdown();

        // Block deactivation until the IO thread has finished serializing the CPU data
        if (m_cpuDataSerializationThread.joinable())
        {
            m_cpuDataSerializationThread.join();
        }
    }

    bool ProfilerSystemComponent::IsActive() const
    {
        return m_cpuProfiler.IsProfilerEnabled();
    }

    void ProfilerSystemComponent::SetActive(bool enabled)
    {
        m_cpuProfiler.SetProfilerEnabled(enabled);
    }

    bool ProfilerSystemComponent::CaptureFrame(const AZStd::string& outputFilePath)
    {
        bool expected = false;
        if (!m_cpuCaptureInProgress.compare_exchange_strong(expected, true))
        {
            return false;
        }

        // Start the cpu profiling
        bool wasEnabled = m_cpuProfiler.IsProfilerEnabled();
        if (!wasEnabled)
        {
            m_cpuProfiler.SetProfilerEnabled(true);
        }

        const int frameDelay = 5; // arbitrary number
        DelayedFunction delayedFunc(frameDelay,
            [this, outputFilePath, wasEnabled]()
            {
                // Blocking call for a single frame of data, avoid thread overhead
                AZStd::ring_buffer<TimeRegionMap> singleFrameData(1);
                singleFrameData.push_back(m_cpuProfiler.GetTimeRegionMap());
                SerializeCpuProfilingData(singleFrameData, outputFilePath, wasEnabled);
                m_cpuCaptureInProgress.store(false);
            }
        );
        delayedFunc.Run();

        return true;
    }

    bool ProfilerSystemComponent::StartCapture(AZStd::string outputFilePath)
    {
        m_captureFile = AZStd::move(outputFilePath);
        return m_cpuProfiler.BeginContinuousCapture();
    }

    bool ProfilerSystemComponent::EndCapture()
    {
        bool expected = false;
        if (!m_cpuDataSerializationInProgress.compare_exchange_strong(expected, true))
        {
            AZ_TracePrintf(
                "ProfilerSystemComponent",
                "Cannot end a continuous capture - another serialization is currently in progress\n");
            return false;
        }

        AZStd::ring_buffer<TimeRegionMap> captureResult;
        const bool captureEnded = m_cpuProfiler.EndContinuousCapture(captureResult);
        if (!captureEnded)
        {
            AZ_TracePrintf("ProfilerSystemComponent", "Could not end the continuous capture, is one in progress?\n");
            m_cpuDataSerializationInProgress.store(false);
            return false;
        }

        // cpuProfilingData could be 1GB+ once saved, so use an IO thread to write it to disk.
        auto threadIoFunction =
            [data = AZStd::move(captureResult), filePath = m_captureFile, &flag = m_cpuDataSerializationInProgress]()
            {
                SerializeCpuProfilingData(data, filePath, true);
                flag.store(false);
            };

        // If the thread object already exists (ex. we have already serialized data), join. This will not block since
        // m_cpuDataSerializationInProgress was false, meaning the IO thread has already completed execution.
        if (m_cpuDataSerializationThread.joinable())
        {
            m_cpuDataSerializationThread.join();
        }

        AZStd::thread_desc threadDesc;
        threadDesc.m_name = "ProfilerSystemComponent";
        auto thread = AZStd::thread(threadDesc, threadIoFunction);
        m_cpuDataSerializationThread = AZStd::move(thread);

        return true;
    }

    bool ProfilerSystemComponent::IsCaptureInProgress() const
    {
        return m_cpuProfiler.IsContinuousCaptureInProgress();
    }
} // namespace Profiler
