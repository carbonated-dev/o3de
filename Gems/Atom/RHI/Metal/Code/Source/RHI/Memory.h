/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once
#include <Atom/RHI/Object.h>
#include <metal/metal.h>

#if defined(CARBONATED)
#include <AzCore/Memory/GPUAllocator.h>
#include <AzCore/Memory/MemoryMarker.h>
#endif

namespace AZ
{
    namespace Metal
    {
        enum class ResourceType
        {
            MtlUndefined = 0,
            MtlTextureType,
            MtlBufferType,            
        };
        
        struct MetalResourceDescriptor
        {
            void* m_resourcePtr = nullptr;
            ResourceType m_resourceType = ResourceType::MtlUndefined;
            bool m_isSwapChainResource = false;
        };
    
        class MetalResource final
            : public RHI::Object
        {
        public:
            
            AZ_CLASS_ALLOCATOR(MetalResource, AZ::SystemAllocator);
            AZ_RTTI(MetalResource, "{ED5953FB-6B4B-4A3B-9566-7561EC284687}", RHI::Object);

            static RHI::Ptr<MetalResource> Create(const MetalResourceDescriptor& metalResourceDescriptor)
            {
                RHI::Ptr<MetalResource> resource = aznew MetalResource();
                resource->m_resourcePtr = metalResourceDescriptor.m_resourcePtr;
                resource->m_resourceType = metalResourceDescriptor.m_resourceType;
                resource->m_isSwapChainResource = metalResourceDescriptor.m_isSwapChainResource;
#if defined(CARBONATED)
                {
                    AZ::GPUAllocator* allocator = static_cast<GPUAllocator*>(&AZ::AllocatorInstance<AZ::GPUAllocator>::Get());
                    switch(resource->m_resourceType)
                    {
                        case ResourceType::MtlTextureType:
                        {
                            MEMORY_TAG(MetalTexture);
                            id<MTLTexture> mtlTexture = resource->GetGpuAddress<id<MTLTexture>>();
                            const size_t size = CalculateTextureSize(mtlTexture);
                            allocator->Allocation(resource->m_resourcePtr, size);
                            break;
                        }
                        case ResourceType::MtlBufferType:
                        {
                            id<MTLBuffer> mtlBuffer = resource->GetGpuAddress<id<MTLBuffer>>();
                            const size_t size = mtlBuffer.allocatedSize;
                            if (size != 0x1000000)  // this size indicates a page in a paged allocator, then it is allocated as textures
                            {
                                MEMORY_TAG(MetalBuffer);
                                allocator->Allocation(resource->m_resourcePtr, size);
                            }
                            break;
                        }
                        default:
                        {
                            AZ_Assert(false, "Undefined metal Resource type");
                        }
                    }
                }
#endif
                return resource;
            }
            
            ~MetalResource()
            {
                switch(m_resourceType)
                {
                    case ResourceType::MtlTextureType:
                    {
                        id<MTLTexture> mtlTexture = GetGpuAddress<id<MTLTexture>>();
                        
                        //Swapchain textures are not created by the engine and are owned by the drivers hence we cant release them
                        if(mtlTexture && !m_isSwapChainResource)
                        {
#if defined(CARBONATED)
                            if (mtlTexture.retainCount == 1)
                            {
                                AZ::GPUAllocator* allocator = static_cast<GPUAllocator*>(&AZ::AllocatorInstance<AZ::GPUAllocator>::Get());
                                const size_t size = CalculateTextureSize(mtlTexture);
                                allocator->Deallocation(mtlTexture, size);
                            }
#endif
                            [mtlTexture release];
                            mtlTexture = nil;
                        }
                        break;
                    }
                    case ResourceType::MtlBufferType:
                    {
                        id<MTLBuffer> mtlBuffer = GetGpuAddress<id<MTLBuffer>>();
                        if (mtlBuffer)
                        {
#if defined(CARBONATED)
                            if (mtlBuffer.retainCount == 1)
                            {
                                const size_t size = mtlBuffer.allocatedSize;
                                if (size != 0x1000000)  // this size indicates a page in a paged allocator
                                {
                                    AZ::GPUAllocator* allocator = static_cast<GPUAllocator*>(&AZ::AllocatorInstance<AZ::GPUAllocator>::Get());
                                    allocator->Deallocation(mtlBuffer, size);
                                }
                            }
#endif
                            [mtlBuffer release];
                            mtlBuffer = nil;
                        }
                        break;
                    }
                    default:
                    {
                        AZ_Assert(false, "Undefined Resource type");
                    }
                }
                m_resourcePtr = nullptr;
            }
            
            ResourceType GetResourceType() const
            {
                return m_resourceType;
            }
            
            void* GetCpuAddress() const
            {
                AZ_Assert(m_resourcePtr, "m_resourcePtr can not be null");
                if(m_resourceType == ResourceType::MtlBufferType)
                {
                    id<MTLBuffer> mtlBuffer = static_cast<id<MTLBuffer>>(m_resourcePtr);
                    return mtlBuffer.contents;
                }
                AZ_Error("MetalResource", false, "Resource Type is incorrect");
                return nullptr;
            }            
            
            template <typename T>
            T GetGpuAddress() const
            {
                AZ_Assert(m_resourcePtr, "m_resourcePtr can not be null");
                return static_cast<T>(m_resourcePtr);
            }
            
            uint64_t GetHash() const
            {
                switch(m_resourceType)
                {
                    case ResourceType::MtlTextureType:
                    {
                        id<MTLTexture> mtlTexture = GetGpuAddress<id<MTLTexture>>();
                        return mtlTexture.hash;
                    }
                    case ResourceType::MtlBufferType:
                    {
                        id<MTLBuffer> mtlBuffer = GetGpuAddress<id<MTLBuffer>>();
                        return mtlBuffer.hash;
                    }
                    default:
                    {
                        AZ_Assert(false, "Undefined Resource type");
                        return 0;
                    }
                }
            }
            
            //! This function is setup for swapchain texture to override the native pointer as for metal we
            //! get the swapchain texture from the drivers at the end of every frame by requesting the nextdrawable from the CAMetalLayer.
            void OverrideResource(id<MTLTexture> mtlTexture)
            {
                AZ_Assert(m_isSwapChainResource, "Only the swapchain texture should be overriden due to the way swapchain works for metal");
                if(m_isSwapChainResource)
                {
                    m_resourcePtr = static_cast<void*>(mtlTexture);
                }
            }

        private:
            MetalResource() = default;
            void* m_resourcePtr = nullptr;
            ResourceType m_resourceType = ResourceType::MtlUndefined;
            bool m_isSwapChainResource = false;
#if defined(CARBONATED)
            static double GetBPP(MTLPixelFormat pixelFormat)
            {
                switch (pixelFormat)
                {
                    case MTLPixelFormatA8Unorm:
                    case MTLPixelFormatR8Unorm:
                    case MTLPixelFormatR8Unorm_sRGB:
                    case MTLPixelFormatR8Snorm:
                    case MTLPixelFormatR8Uint:
                    case MTLPixelFormatR8Sint:
                        return 8.00;

                    case MTLPixelFormatR16Unorm:
                    case MTLPixelFormatR16Uint:
                    case MTLPixelFormatR16Sint:
                    case MTLPixelFormatR16Float:
                    case MTLPixelFormatRG8Unorm:
                    case MTLPixelFormatRG8Unorm_sRGB:
                    case MTLPixelFormatRG8Snorm:
                    case MTLPixelFormatRG8Uint:
                    case MTLPixelFormatRG8Sint:
                        
                    case MTLPixelFormatB5G6R5Unorm:
                    case MTLPixelFormatA1BGR5Unorm:
                    case MTLPixelFormatABGR4Unorm:
                    case MTLPixelFormatBGR5A1Unorm:
                        return 16.00;
                        
                    case MTLPixelFormatR32Uint:
                    case MTLPixelFormatR32Sint:
                    case MTLPixelFormatR32Float:
                    case MTLPixelFormatRG16Unorm:
                    case MTLPixelFormatRG16Snorm:
                    case MTLPixelFormatRG16Uint:
                    case MTLPixelFormatRG16Sint:
                    case MTLPixelFormatRG16Float:
                    case MTLPixelFormatRGBA8Unorm:
                    case MTLPixelFormatRGBA8Unorm_sRGB:
                    case MTLPixelFormatRGBA8Snorm:
                    case MTLPixelFormatRGBA8Uint:
                    case MTLPixelFormatRGBA8Sint:
                    case MTLPixelFormatBGRA8Unorm:
                    case MTLPixelFormatBGRA8Unorm_sRGB:
                        
                    case MTLPixelFormatBGR10A2Unorm:
                    case MTLPixelFormatRGB10A2Unorm:
                    case MTLPixelFormatRGB10A2Uint:
                    case MTLPixelFormatRG11B10Float:
                    case MTLPixelFormatRGB9E5Float:
                        
                    case MTLPixelFormatDepth32Float:
                        return 32.00;

                    case MTLPixelFormatDepth32Float_Stencil8:
                        return 40.0;  // some devices allocate 64 bits, but I do not know which ones

                    case MTLPixelFormatRG32Uint:
                    case MTLPixelFormatRG32Sint:
                    case MTLPixelFormatRG32Float:
                    case MTLPixelFormatRGBA16Unorm:
                    case MTLPixelFormatRGBA16Snorm:
                    case MTLPixelFormatRGBA16Uint:
                    case MTLPixelFormatRGBA16Sint:
                    case MTLPixelFormatRGBA16Float:
                        return 64.00;

                    case MTLPixelFormatRGBA32Uint:
                    case MTLPixelFormatRGBA32Sint:
                    case MTLPixelFormatRGBA32Float:
                        return 128.00;
                        
                    case MTLPixelFormatASTC_4x4_sRGB:
                    case MTLPixelFormatASTC_4x4_LDR:
                    case MTLPixelFormatASTC_4x4_HDR:
                        return 8.00;
                    case MTLPixelFormatASTC_6x6_sRGB:
                    case MTLPixelFormatASTC_6x6_LDR:
                    case MTLPixelFormatASTC_6x6_HDR:
                        return 3.56;
                    case MTLPixelFormatASTC_8x8_sRGB:
                    case MTLPixelFormatASTC_8x8_LDR:
                    case MTLPixelFormatASTC_8x8_HDR:
                        return 2.00;
                }
                
                AZ_Error("metal", false, "Add bpp case for texture format %d", int(pixelFormat));
                return 32.0;
            }
            static size_t CalculateTextureSize(id<MTLTexture> mtlTexture)
            {
                const unsigned int width = (unsigned int)mtlTexture.width;
                const unsigned int height = (unsigned int)mtlTexture.height;
                const unsigned int depth = (unsigned int)mtlTexture.depth;
                const unsigned int mipmaps = (unsigned int)mtlTexture.mipmapLevelCount;
                const unsigned int bufferBytesPerRow = (unsigned int)mtlTexture.bufferBytesPerRow;
                const unsigned int arrayLength = (unsigned int)mtlTexture.arrayLength;
                
                const double bpp = GetBPP(mtlTexture.pixelFormat);
                
                size_t mip0 = size_t(double(width * height * depth) * bpp / 8.0) * arrayLength;
                
                if (bufferBytesPerRow != 0)
                {
                    mip0 = bufferBytesPerRow * height * arrayLength;
                }
                
                size_t sumSize = 0;
                size_t cur = mip0;
                for (int i = 0; i < mipmaps; i++)
                {
                    sumSize += cur;
                    cur /= 4;
                }
                /*
                AZ_Info("mts", "mtlTexture %d x %d, d%d, mips%d, bpp%.1f, format %d, %d, %d", width, height, depth, mipmaps, bpp, int(mtlTexture.pixelFormat), mip0, sumSize);
                if (width >= 1024 && height >= 1024 && bpp >= 30.0)
                {
                    AZ_Info("metal", "this is it");  // set breakpoint here to catch large textures
                }
                */
                return sumSize;
            }
#endif
        };
        
        using Memory = MetalResource;
    }
}
