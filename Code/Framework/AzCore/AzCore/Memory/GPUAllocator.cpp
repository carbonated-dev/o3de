#if defined(CARBONATED)
#include <AzCore/Memory/GPUAllocator.h>

#include <AzCore/Memory/AllocatorManager.h>
#include <AzCore/Memory/AllocationRecords.h>

#include <AzCore/std/functional.h>

#include <AzCore/Debug/MemoryProfiler.h>
#include <AzCore/Debug/Profiler.h>
#include <memory>

namespace AZ
{
    //////////////////////////////////////////////////////////////////////////
    AZ_TYPE_INFO_WITH_NAME_IMPL(GPUAllocator, "GPUAllocator", "{E9AA24D1-2558-49D4-8B2E-A83804D17AE2}");
    AZ_RTTI_NO_TYPE_INFO_IMPL(GPUAllocator, AllocatorBase);

    constexpr const char* ERROR_MESSAGE = "GPUAllocator is for tracking only, but not allocation";

    GPUAllocator::GPUAllocator()
    {
        Create();
        PostCreate();
    }

    GPUAllocator::~GPUAllocator()
    {
        PreDestroy();
        Destroy();
    }

    bool GPUAllocator::Create()
    {
        m_numAllocatedBytes = 0;
        return true;
    }

    AllocatorDebugConfig GPUAllocator::GetDebugConfig()
    {
        return AllocatorDebugConfig()
            .StackRecordLevels(O3DE_STACK_CAPTURE_DEPTH)
            .UsesMemoryGuards()
            .MarksUnallocatedMemory()
            .ExcludeFromDebugging(false);
    }

    void GPUAllocator::Allocation(pointer ptr, size_type byteSize)
    {
#if defined(AZ_ENABLE_TRACING)
        AZ_MEMORY_PROFILE(ProfileAllocation(ptr, byteSize, 1, 1));
        m_numAllocatedBytes += byteSize;
#endif
    }
    void GPUAllocator::Deallocation(pointer ptr, size_type byteSize)
    {
#if defined(AZ_ENABLE_TRACING)
        AZ_MEMORY_PROFILE(ProfileDeallocation(ptr, byteSize, 1, nullptr));
        m_numAllocatedBytes -= byteSize;
#endif
    }

    AllocateAddress GPUAllocator::allocate(size_type byteSize, [[maybe_unused]] size_type alignment)
    {
        AZ_Assert(false, ERROR_MESSAGE);
        return AllocateAddress(nullptr, byteSize);
        /*
        AllocateAddress address = alignment;
#if defined(AZ_ENABLE_TRACING)
        alignment = 1;
        AZ_MEMORY_PROFILE(ProfileAllocation(address, byteSize, alignment, 1));
        m_numAllocatedBytes += byteSize;
#endif
        return address;
        */
    }

    auto GPUAllocator::deallocate([[maybe_unused]] pointer ptr, size_type byteSize, [[maybe_unused]] size_type alignment) -> size_type
    {
        AZ_Assert(false, ERROR_MESSAGE);
        /*
#if defined(AZ_ENABLE_TRACING)
        if (ptr)
        {
            AZ_MEMORY_PROFILE(ProfileDeallocation(ptr, byteSize, alignment, nullptr));
            m_numAllocatedBytes -= byteSize;
        }
#endif
        */
        return byteSize;
    }

    AllocateAddress GPUAllocator::reallocate(pointer ptr, size_type newSize, [[maybe_unused]] size_type newAlignment)
    {
        AZ_Assert(false, ERROR_MESSAGE);
        return AllocateAddress(ptr, newSize);
        /*
        AllocateAddress newAddress = ptr;
#if defined(AZ_ENABLE_TRACING)
        AZ_MEMORY_PROFILE(ProfileReallocationBegin(ptr));
        AZ_MEMORY_PROFILE(ProfileReallocationEnd(ptr, newAddress, allocatedSize, newAlignment));
#endif
        return newAddress;
        */
    }

    auto GPUAllocator::get_allocated_size([[maybe_unused]] pointer ptr, [[maybe_unused]] align_type alignment) const -> size_type
    {
        AZ_Assert(false, ERROR_MESSAGE);
        return 0;
    }

} // namespace AZ

#endif // CARBONATED
