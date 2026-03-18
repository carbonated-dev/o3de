/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#ifndef GM_MEMORY_H
#define GM_MEMORY_H

#include <AzCore/base.h>
#if defined(AZ_COMPILER_MSVC) // todo fix it
#   pragma warning(push)
#   pragma warning(disable:4127)
#endif
#include <AzCore/Memory/IAllocator.h>
#include <AzCore/Memory/OSAllocator.h>
#include <AzCore/Memory/SystemAllocator.h>
#if defined(AZ_COMPILER_MSVC)
#   pragma warning(pop)
#endif

#include <GridMate/GridMateForTools.h>

#include <AzFramework/AzFrameworkAPI.h>

namespace GridMate
{
#ifndef GRIDMATE_FOR_TOOLS

    // Gruber patch begin // VMED -- added missed abstract methods GetName(), GetDescription(), added missed struct Descriptor
    class GridMateSystemAllocator
        : public AZ::SystemAllocator
    {
        friend class AZ::AllocatorInstance<GridMateSystemAllocator>;
    public:
        virtual const char* GetName() const = 0;
        virtual const char* GetDescription() const = 0;

        struct Descriptor
        {
            Descriptor()
                //: m_custom(0)
                : m_allocationRecords(true)
                , m_stackRecordLevels(5)
            {}
            //IAllocatorAllocate* m_custom;   ///< You can provide our own allocation scheme. If NULL a HeapScheme will be used with the provided Descriptor.

            struct Heap
            {
                Heap()
                    : m_pageSize(m_defaultPageSize)
                    , m_poolPageSize(m_defaultPoolPageSize)
                    , m_isPoolAllocations(true)
                    , m_numFixedMemoryBlocks(0)
                    //, m_subAllocator(nullptr)
                    , m_systemChunkSize(0)
                {}
                static const int        m_defaultPageSize = AZ_TRAIT_OS_DEFAULT_PAGE_SIZE;
                static const int        m_defaultPoolPageSize = 4 * 1024;
                static const int        m_memoryBlockAlignment = m_defaultPageSize;
                static const int        m_maxNumFixedBlocks = 3;
                unsigned int            m_pageSize;                                 ///< Page allocation size must be 1024 bytes aligned. (default m_defaultPageSize)
                unsigned int            m_poolPageSize;                             ///< Page size used to small memory allocations. Must be less or equal to m_pageSize and a multiple of it. (default m_defaultPoolPageSize)
                bool                    m_isPoolAllocations;                        ///< True (default) if we use pool for small allocations (< 256 bytes), otherwise false. IMPORTANT: Changing this to false will degrade performance!
                int                     m_numFixedMemoryBlocks;                     ///< Number of memory blocks to use.
                void* m_fixedMemoryBlocks[m_maxNumFixedBlocks];   ///< Pointers to provided memory blocks or NULL if you want the system to allocate them for you with the System Allocator.
                size_t                  m_fixedMemoryBlocksByteSize[m_maxNumFixedBlocks]; ///< Sizes of different memory blocks (MUST be multiple of m_pageSize), if m_memoryBlock is 0 the block will be allocated for you with the System Allocator.
                //IAllocatorAllocate* m_subAllocator;                             ///< Allocator that m_memoryBlocks memory was allocated from or should be allocated (if NULL).
                size_t                  m_systemChunkSize;                          ///< Size of chunk to request from the OS when more memory is needed (defaults to m_pageSize)
            }                           m_heap;
            bool                        m_allocationRecords;    ///< True if we want to track memory allocations, otherwise false.
            unsigned char               m_stackRecordLevels;    ///< If stack recording is enabled, how many stack levels to record.
        };

        //bool Create(const Descriptor& desc);
    };
    // Gruber patch end // VMED

    /**
    * GridMateAllocator is used by non-MP portions of GridMate
    */
    class AZF_API GridMateAllocator
// Gruber patch begin // VMED
        // was : public AZ::SystemAllocator
        : public GridMateSystemAllocator
// Gruber patch end // VMED
    {
        friend class AZ::AllocatorInstance<GridMateAllocator>;
    public:

        AZ_TYPE_INFO(GridMateAllocator, "{BB127E7A-E4EF-4480-8F17-0C10146D79E0}")
        AZ_RTTI_NO_TYPE_INFO_DECL();

        const char* GetName() const override { return "GridMate Allocator"; }
        const char* GetDescription() const override { return "GridMate fundamental generic memory allocator"; }
    };

    AZ_RTTI_NO_TYPE_INFO_IMPL_INLINE(GridMateAllocator);

    /**
    * GridMateAllocatorMP is used by MP portions of GridMate
    */
    class AZF_API GridMateAllocatorMP
// Gruber patch begin // VMED
        // was : public AZ::SystemAllocator
        : public GridMateSystemAllocator
// Gruber patch end // VMED
    {
        friend class AZ::AllocatorInstance<GridMateAllocatorMP>;
    public:

        AZ_TYPE_INFO(GridMateAllocatorMP, "{FABCBC6E-B3E5-4200-861E-A3EC22592678}")

        const char* GetName() const override { return "GridMate Multiplayer Allocator"; }
        const char* GetDescription() const override { return "GridMate Multiplayer data allocations (Session,Replica,Carrier)"; }

        // TODO: We have an aggressive memory policy in the Carrier. We have 2 ways to fix it.
        // Either keep a cap and sacrifice performance or create a carrier->GarbageCollection and call it from here
        //virtual void          GarbageCollect()                 { EBUS_EVENT(CarrierBus,GarbageCollect); m_allocator->GarbageCollect(); }
    };
#else
    using GridMateAllocator = AZ::OSAllocator;
    using GridMateAllocatorMP = AZ::OSAllocator;
#endif

    //! GridMate system container allocator.
    typedef AZ::AZStdAlloc<GridMateAllocator> GridMateStdAlloc;

    //! GridMate system container allocator.
    typedef AZ::AZStdAlloc<GridMateAllocatorMP> SysContAlloc;
}   // namespace GridMate


#define GM_CLASS_ALLOCATOR(_type)       AZ_CLASS_ALLOCATOR(_type, GridMate::GridMateAllocatorMP, 0)
#define GM_CLASS_ALLOCATOR_DECL         AZ_CLASS_ALLOCATOR_DECL
#define GM_CLASS_ALLOCATOR_IMPL(_type)  AZ_CLASS_ALLOCATOR_IMPL(_type, GridMate::GridMateAllocatorMP, 0)

#endif // GM_MEMORY_H
