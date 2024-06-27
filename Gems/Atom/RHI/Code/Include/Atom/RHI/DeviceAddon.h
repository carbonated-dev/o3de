// CARBONATED specific classes to track GPU data, used in Device.h
#pragma once

namespace AZ::RHI
{

    struct TimeInterval
    {
        double m_begin;
        double m_end;
        
        TimeInterval() {}
        TimeInterval(double begin, double end) : m_begin(begin), m_end(end) {}
        /*
        bool operator <(const TimeInterval& t)
        {
            return m_end < t.m_begin;
        }
        bool operator >(const TimeInterval& t)
        {
            return m_begin > t.m_end;
        }
        */
        bool IsOverlap(const TimeInterval& t)
        {
            if (m_begin >= t.m_begin)
            {
                return m_begin <= t.m_end;
            }
            return t.m_begin <= m_end;
        }
        
        void Merge(const TimeInterval& t)
        {
            AZ_Assert(IsOverlap(t), "Cannot combine non-overlapping intervals");
            if (m_begin > t.m_begin)
            {
                m_begin = t.m_begin;
            }
            if (t.m_end > m_end)
            {
                m_end = t.m_end;
            }
        }
    };

    struct FrameCommands
    {
        unsigned int m_frameNumber;
        double m_sumTime;
        AZStd::vector<const void*> m_commands;
        AZStd::vector<TimeInterval> m_intervals;
        
        FrameCommands() : m_frameNumber(0), m_sumTime(0.0)
        {
            m_commands.reserve(6);
            m_intervals.reserve(4);
        }
        
        void RegisterCommandBuffer(const void* commandBuffer)
        {
            m_commands.push_back(commandBuffer);
        }
        void RegisterInterval(double begin, double end)
        {
            m_sumTime += end - begin;
            bool consumed = false;
            for (auto it = m_intervals.begin(); it != m_intervals.end(); it++)
            {
                if (end < it->m_begin)
                {
                    m_intervals.insert(it, TimeInterval(begin, end));
                    consumed = true;
                    break;
                }
                else if (begin > it->m_end)
                {
                    continue;
                }
                it->Merge(TimeInterval(begin, end));
                consumed = true;
                for (;;)
                {
                    auto itNext = it + 1;
                    if (itNext == m_intervals.end())
                    {
                        break;
                    }
                    if (!it->IsOverlap(*itNext))
                    {
                        break;
                    }
                    it->Merge(*itNext);
                    m_intervals.erase(itNext);
                }
                break;
            }
            if (!consumed)
            {
                m_intervals.push_back(TimeInterval(begin, end));
            }
            
            ValidateIntervals();
        }
        void LogIntervals()
        {
            int i = 0;
            for (auto it = m_intervals.begin(); it != m_intervals.end(); it++, i++)
            {
                AZ_Info("GPUtime", "%d %f %f", i, it->m_begin, it->m_end);
            }
        }
        void ValidateIntervals()
        {
            for (auto it = m_intervals.begin(); it != m_intervals.end(); it++)
            {
                auto itNext = it + 1;
                if (it->m_begin >= it->m_end)
                {
                    LogIntervals();
                    AZ_Error("GPUtime", false, "bad interval %f %f", it->m_begin, it->m_end);
                }
                if (itNext != m_intervals.end())
                {
                    if (it->m_end > itNext->m_begin)
                    {
                        LogIntervals();
                        AZ_Error("GPUtime", false, "unordered intervals %f..%f  %f..%f", it->m_begin, it->m_end, itNext->m_begin, itNext->m_end);
                    }
                }
            }
        }
        
        void Init(int frameNumber)
        {
            m_frameNumber = frameNumber;
            m_sumTime = 0.0;
            m_commands.clear();
            m_intervals.clear();
        }
        
        double CalculateTime()
        {
            double result = 0.0;
            for (const TimeInterval& ti : m_intervals)
            {
                result += ti.m_end - ti.m_begin;
            }
            return result;
        }
    };

}
