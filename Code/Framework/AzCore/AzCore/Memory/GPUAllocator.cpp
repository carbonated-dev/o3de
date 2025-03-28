//#if defined(CARBONATED)
#include <AzCore/Memory/SystemAllocator.h>

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

    AllocatorDebugConfig SystemAllocator::GetDebugConfig()
    {
        return AllocatorDebugConfig()
            .StackRecordLevels(O3DE_STACK_CAPTURE_DEPTH)
            .UsesMemoryGuards()
            .MarksUnallocatedMemory()
            .ExcludeFromDebugging(false);
    }

    AllocateAddress GPUAllocator::allocate(size_type byteSize, size_type alignment)
    {
        AllocateAddress address = alignment;
#if defined(AZ_ENABLE_TRACING)
        alignment = 1;
        AZ_MEMORY_PROFILE(ProfileAllocation(address, byteSize, alignment, 1));
        m_numAllocatedBytes += byteSize;
#endif
        return address;
    }

    auto GPUAllocator::deallocate(pointer ptr, size_type byteSize, [[maybe_unused]] size_type alignment) -> size_type
    {
#if defined(AZ_ENABLE_TRACING)
        if (ptr)
        {
            AZ_MEMORY_PROFILE(ProfileDeallocation(ptr, byteSize, alignment, nullptr));
            m_numAllocatedBytes -= byteSize;
        }
#endif
        return byteSize;
    }

    AllocateAddress SystemAllocator::reallocate(pointer ptr, size_type newSize, size_type newAlignment)
    {
        AllocateAddress newAddress = ptr;
#if defined(AZ_ENABLE_TRACING)
        AZ_MEMORY_PROFILE(ProfileReallocationBegin(ptr));
        AZ_MEMORY_PROFILE(ProfileReallocationEnd(ptr, newAddress, allocatedSize, newAlignment));
#endif
        return newAddress;
    }

    auto SystemAllocator::get_allocated_size(pointer ptr, align_type alignment) const -> size_type
    {
        return m_numAllocatedBytes;
    }

} // namespace AZ

//#endif // CARBONATED
