/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#include <RHI/ConstantDataAllocator.h>

#include <Atom/RHI/BufferPool.h>
#include <Atom/RHI.Reflect/Bits.h>
#include <AzCore/std/parallel/lock.h>
#include <RHI/Buffer.h>
#include <RHI/BufferMemoryView.h>
#include <RHI/BufferPool.h>
#include <RHI/Device.h>

#include <cstring>

namespace AZ
{
    namespace Vulkan
    {
        ConstantDataAllocator::Allocation::~Allocation()
        {
            Reset();
        }

        ConstantDataAllocator::Allocation::Allocation(Allocation&& rhs)
            : m_allocator(AZStd::move(rhs.m_allocator))
            , m_page(rhs.m_page)
            , m_sliceIndex(rhs.m_sliceIndex)
        {
            rhs.m_page = nullptr;
            rhs.m_sliceIndex = 0;
        }

        ConstantDataAllocator::Allocation& ConstantDataAllocator::Allocation::operator=(Allocation&& rhs)
        {
            if (this != &rhs)
            {
                Reset();
                m_allocator = AZStd::move(rhs.m_allocator);
                m_page = rhs.m_page;
                m_sliceIndex = rhs.m_sliceIndex;
                rhs.m_page = nullptr;
                rhs.m_sliceIndex = 0;
            }
            return *this;
        }

        bool ConstantDataAllocator::Allocation::IsValid() const
        {
            return m_allocator && m_page;
        }

        void ConstantDataAllocator::Allocation::Reset()
        {
            if (IsValid())
            {
                m_allocator->DeAllocate(*m_page, m_sliceIndex);
            }
            m_allocator.reset();
            m_page = nullptr;
            m_sliceIndex = 0;
        }

        void ConstantDataAllocator::Allocation::Write(AZStd::span<const uint8_t> data)
        {
            AZ_Assert(IsValid(), "Cannot write through an invalid constant-data allocation.");
            m_allocator->Write(*this, data);
        }

        const RHI::Ptr<Buffer>& ConstantDataAllocator::Allocation::GetBuffer() const
        {
            AZ_Assert(IsValid(), "Cannot get the buffer from an invalid constant-data allocation.");
            return m_page->m_buffer;
        }

        uint32_t ConstantDataAllocator::Allocation::GetByteOffset() const
        {
            AZ_Assert(IsValid(), "Cannot get the offset from an invalid constant-data allocation.");
            return m_sliceIndex * m_allocator->m_sliceStride;
        }

        uint32_t ConstantDataAllocator::Allocation::GetByteCount() const
        {
            AZ_Assert(IsValid(), "Cannot get the size from an invalid constant-data allocation.");
            return m_allocator->m_constantDataSize;
        }

        ConstantDataAllocator::Page::~Page()
        {
            if (m_mappedData && m_buffer)
            {
                m_buffer->GetBufferMemoryView()->Unmap(RHI::HostMemoryAccess::Write);
            }
        }

        RHI::ResultCode ConstantDataAllocator::Init(
            Device& device,
            RHI::Ptr<BufferPool> bufferPool,
            uint32_t constantDataSize,
            uint32_t slicesPerPage)
        {
            AZ_Assert(bufferPool, "Constant-data buffer pool is null.");
            AZ_Assert(constantDataSize, "Constant-data size must be non-zero.");
            AZ_Assert(slicesPerPage, "Constant-data page must contain at least one slice.");

            m_device = &device;
            m_bufferPool = AZStd::move(bufferPool);
            m_constantDataSize = constantDataSize;
            m_sliceStride = RHI::AlignUp(
                constantDataSize,
                device.GetLimits().m_minConstantBufferViewOffset);
            m_slicesPerPage = slicesPerPage;
            return RHI::ResultCode::Success;
        }

        ConstantDataAllocator::Allocation ConstantDataAllocator::Allocate()
        {
            AllocationList allocations = AllocateBatch(1);
            return allocations.empty() ? Allocation{} : AZStd::move(allocations.front());
        }

        ConstantDataAllocator::AllocationList ConstantDataAllocator::AllocateBatch(
            uint32_t count)
        {
            AZ_Assert(
                count <= RHI::Limits::Device::FrameCountMax,
                "Constant-data batch exceeds the maximum supported frame count.");
            AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
            AllocationList allocations;
            for (uint32_t allocationIndex = 0;
                 allocationIndex < count;
                 ++allocationIndex)
            {
                Allocation allocation = AllocateNoLock();
                if (!allocation.IsValid())
                {
                    for (Allocation& allocatedSlice : allocations)
                    {
                        DeAllocateNoLock(
                            *allocatedSlice.m_page,
                            allocatedSlice.m_sliceIndex);
                        allocatedSlice.m_allocator.reset();
                        allocatedSlice.m_page = nullptr;
                        allocatedSlice.m_sliceIndex = 0;
                    }
                    allocations.clear();
                    break;
                }
                allocations.emplace_back(AZStd::move(allocation));
            }
            return allocations;
        }

        ConstantDataAllocator::Allocation
        ConstantDataAllocator::AllocateNoLock()
        {
            if (m_availablePages.empty() && !CreatePage())
            {
                return {};
            }

            Page* page = m_availablePages.back();
            const uint32_t sliceIndex = page->m_freeSlices.back();
            page->m_freeSlices.pop_back();
            if (page->m_freeSlices.empty())
            {
                m_availablePages.pop_back();
            }

            ++m_allocationCount;
            ++m_activeAllocationCount;

            Allocation allocation;
            allocation.m_allocator = shared_from_this();
            allocation.m_page = page;
            allocation.m_sliceIndex = sliceIndex;
            return allocation;
        }

        ConstantDataAllocator::Statistics ConstantDataAllocator::GetStatistics() const
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
            Statistics statistics;
            statistics.m_allocationCount = m_allocationCount;
            statistics.m_pageCreateCount = m_pageCreateCount;
            statistics.m_activeAllocationCount = m_activeAllocationCount;
            statistics.m_pageCount = m_pages.size();
            statistics.m_availablePageCount = m_availablePages.size();
            return statistics;
        }

        ConstantDataAllocator::Page* ConstantDataAllocator::CreatePage()
        {
            AZStd::unique_ptr<Page> page = AZStd::make_unique<Page>();
            page->m_buffer = Buffer::Create();

            const uint64_t pageByteCount =
                static_cast<uint64_t>(m_sliceStride) * m_slicesPerPage;
            const RHI::BufferDescriptor bufferDescriptor(
                RHI::BufferBindFlags::Constant,
                pageByteCount);
            RHI::BufferInitRequest request(*page->m_buffer, bufferDescriptor);
            if (m_bufferPool->InitBuffer(request) != RHI::ResultCode::Success)
            {
                return nullptr;
            }

            BufferMemoryView* memoryView = page->m_buffer->GetBufferMemoryView();
            page->m_mappedData = static_cast<uint8_t*>(
                memoryView->Map(RHI::HostMemoryAccess::Write));
            if (!page->m_mappedData)
            {
                return nullptr;
            }

            page->m_freeSlices.reserve(m_slicesPerPage);
            for (uint32_t sliceIndex = 0; sliceIndex < m_slicesPerPage; ++sliceIndex)
            {
                page->m_freeSlices.push_back(m_slicesPerPage - sliceIndex - 1);
            }

            Page* pagePointer = page.get();
            m_pages.emplace_back(AZStd::move(page));
            m_availablePages.push_back(pagePointer);
            ++m_pageCreateCount;
            return pagePointer;
        }

        void ConstantDataAllocator::DeAllocate(Page& page, uint32_t sliceIndex)
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
            DeAllocateNoLock(page, sliceIndex);
        }

        void ConstantDataAllocator::DeAllocateNoLock(
            Page& page,
            uint32_t sliceIndex)
        {
            const bool wasFull = page.m_freeSlices.empty();
            page.m_freeSlices.push_back(sliceIndex);
            if (wasFull)
            {
                m_availablePages.push_back(&page);
            }
            AZ_Assert(m_activeAllocationCount > 0, "Constant-data allocation count underflow.");
            --m_activeAllocationCount;
        }

        void ConstantDataAllocator::Write(
            const Allocation& allocation,
            AZStd::span<const uint8_t> data)
        {
            AZ_Assert(
                data.size() <= m_constantDataSize,
                "Constant-data write of %zu bytes exceeds the %u-byte allocation.",
                data.size(),
                m_constantDataSize);

            AZStd::lock_guard<AZStd::mutex> lock(m_mutex);
            const uint32_t byteOffset = allocation.GetByteOffset();
            memcpy(
                allocation.m_page->m_mappedData + byteOffset,
                data.data(),
                data.size());

            BufferMemoryView* memoryView =
                allocation.m_page->m_buffer->GetBufferMemoryView();
            memoryView->GetAllocation()->Flush(
                memoryView->GetOffset() + byteOffset,
                data.size());
        }
    } // namespace Vulkan
} // namespace AZ
