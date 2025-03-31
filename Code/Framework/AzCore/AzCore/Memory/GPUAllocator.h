#pragma once

//#if defined(CARBONATED)

#include <AzCore/Memory/AllocatorBase.h>
#include <AzCore/Memory/Memory.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace AZ
{
    /**
     * GPU allocator
     * The purpose of this allocator is to track GPU memory, it allocates nothing,
     * just tracks the GPU memory via AllocationRecords to find memory leaks
     * NEVER use it as a normal allocator (although it mimics the API), it is fake
     */
    class GPUAllocator
        : public AllocatorBase
    {
    public:
        AZ_TYPE_INFO_WITH_NAME_DECL(GPUAllocator);
        AZ_RTTI_NO_TYPE_INFO_DECL();

        GPUAllocator();
        ~GPUAllocator() override;

        bool Create();

        void Allocation(pointer ptr, size_type byteSize);
        void Deallocation(pointer ptr, size_type byteSize);

        //////////////////////////////////////////////////////////////////////////
        // IAllocator
        AllocatorDebugConfig GetDebugConfig() override;

        //////////////////////////////////////////////////////////////////////////
        // IAllocator

        AllocateAddress allocate(size_type byteSize, size_type alignment) override;
        size_type       deallocate(pointer ptr, size_type byteSize = 0, size_type alignment = 0) override;
        AllocateAddress reallocate(pointer ptr, size_type newSize, size_type newAlignment) override;
        size_type get_allocated_size(pointer ptr, size_type alignment) const override;
        void            GarbageCollect() override                 {}

        size_type       NumAllocatedBytes() const override       { return m_numAllocatedBytes; }

        //////////////////////////////////////////////////////////////////////////

    protected:
        //GPUAllocator(const GPUAllocator&);
        //GPUAllocator& operator=(const GPUAllocator&);
        AZStd::atomic<size_type> m_numAllocatedBytes = 0;
    };
}

//#endif // CARBONATED
