/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#include <Atom/RHI/Buffer.h>
#include <Atom/RHI.Reflect/Base.h>
#include <Atom/RHI.Reflect/Limits.h>
#include <AzCore/std/containers/fixed_vector.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/parallel/mutex.h>
#include <AzCore/std/smart_ptr/enable_shared_from_this.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace AZ
{
    namespace Vulkan
    {
        class Buffer;
        class BufferPool;
        class Device;

        //! Suballocates persistently mapped constant-buffer pages for descriptor sets.
        class ConstantDataAllocator final
            : public AZStd::enable_shared_from_this<ConstantDataAllocator>
        {
        private:
            struct Page;

        public:
            struct Statistics
            {
                uint64_t m_allocationCount = 0;
                uint64_t m_pageCreateCount = 0;
                size_t m_activeAllocationCount = 0;
                size_t m_pageCount = 0;
                size_t m_availablePageCount = 0;
            };

            class Allocation final
            {
            public:
                Allocation() = default;
                ~Allocation();

                Allocation(const Allocation&) = delete;
                Allocation& operator=(const Allocation&) = delete;

                Allocation(Allocation&& rhs);
                Allocation& operator=(Allocation&& rhs);

                bool IsValid() const;
                void Reset();
                void Write(AZStd::span<const uint8_t> data);

                const RHI::Ptr<Buffer>& GetBuffer() const;
                uint32_t GetByteOffset() const;
                uint32_t GetByteCount() const;

            private:
                friend class ConstantDataAllocator;

                AZStd::shared_ptr<ConstantDataAllocator> m_allocator;
                Page* m_page = nullptr;
                uint32_t m_sliceIndex = 0;
            };

            using AllocationList =
                AZStd::fixed_vector<Allocation, RHI::Limits::Device::FrameCountMax>;

            RHI::ResultCode Init(
                Device& device,
                RHI::Ptr<BufferPool> bufferPool,
                uint32_t constantDataSize,
                uint32_t slicesPerPage);

            Allocation Allocate();
            AllocationList AllocateBatch(uint32_t count);
            Statistics GetStatistics() const;

        private:
            struct Page
            {
                ~Page();

                RHI::Ptr<Buffer> m_buffer;
                uint8_t* m_mappedData = nullptr;
                AZStd::vector<uint32_t> m_freeSlices;
            };

            Page* CreatePage();
            Allocation AllocateNoLock();
            void DeAllocate(Page& page, uint32_t sliceIndex);
            void DeAllocateNoLock(Page& page, uint32_t sliceIndex);
            void Write(const Allocation& allocation, AZStd::span<const uint8_t> data);

            Device* m_device = nullptr;
            RHI::Ptr<BufferPool> m_bufferPool;
            uint32_t m_constantDataSize = 0;
            uint32_t m_sliceStride = 0;
            uint32_t m_slicesPerPage = 0;
            AZStd::vector<AZStd::unique_ptr<Page>> m_pages;
            AZStd::vector<Page*> m_availablePages;

            mutable AZStd::mutex m_mutex;
            uint64_t m_allocationCount = 0;
            uint64_t m_pageCreateCount = 0;
            size_t m_activeAllocationCount = 0;
        };
    } // namespace Vulkan
} // namespace AZ
