/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Debug/Profiler.h>
#include <AzCore/std/string/conversions.h>
#include <AzCore/std/string/string.h>
#include <AzFramework/Windowing/WindowBus.h>
#include <RHI/Device.h>
#include <RHI/Image.h>
#include <RHI/SwapChain.h>

namespace Platform
{
    CGRect GetScreenBounds(NativeWindowType* nativeWindow);
    CGFloat GetScreenScale();
    void AttachViewController(NativeWindowType* nativeWindow, NativeViewControllerType* viewController, RHIMetalView* metalView);
    void UnAttachViewController(NativeWindowType* nativeWindow, NativeViewControllerType* viewController);
    void PresentInternal(id <MTLCommandBuffer> mtlCommandBuffer, id<CAMetalDrawable> drawable, float syncInterval, float refreshRate);
    void ResizeInternal(RHIMetalView* metalView, CGSize viewSize);
    float GetRefreshRate();
    RHIMetalView* GetMetalView(NativeWindowType* nativeWindow);
}

namespace AZ
{
    namespace Metal
    {
        RHI::Ptr<SwapChain> SwapChain::Create()
        {
            return aznew SwapChain();
        }
        
        Device& SwapChain::GetDevice() const
        {
            return static_cast<Device&>(Base::GetDevice());
        }

        RHI::ResultCode SwapChain::InitInternal(RHI::Device& deviceBase, const RHI::SwapChainDescriptor& descriptor, RHI::SwapChainDimensions* nativeDimensions)
        {
            auto& device = static_cast<Device&>(deviceBase);
            m_mtlDevice = device.GetMtlDevice();
            m_nativeWindow = reinterpret_cast<NativeWindowType*>(descriptor.m_window.GetIndex());
            AZ_Assert(m_nativeWindow != nullptr, "No window created");
            
            const bool isDisplayAWindow = ([(id)descriptor.m_window.GetIndex() isKindOfClass:[NativeWindowType class]]) == YES;
            if (isDisplayAWindow)
            {
                              
                // Create the MetalView
                CGRect screenBounds = Platform::GetScreenBounds(m_nativeWindow);
                CGFloat screenScale = Platform::GetScreenScale();
                m_metalView = [[RHIMetalView alloc] initWithFrame: screenBounds
                                                           scale: screenScale
                                                          device: m_mtlDevice];
                
                [m_metalView retain];
                
                // Create the MetalViewController
                RHIMetalViewController* metalViewController = [RHIMetalViewController alloc];
                m_viewController = [metalViewController init];
                [m_viewController setView : m_metalView];
                [m_viewController retain];
                        
                Platform::AttachViewController(m_nativeWindow, m_viewController, m_metalView);
                
                m_drawableSize = CGSizeMake(descriptor.m_dimensions.m_imageWidth, descriptor.m_dimensions.m_imageHeight);
                m_metalView.metalLayer.drawableSize = m_drawableSize;
            }
            else
            {
                AddSubView();
            }

            m_drawables.resize(descriptor.m_dimensions.m_imageCount);

            if (nativeDimensions)
            {
                *nativeDimensions = descriptor.m_dimensions;
            }

            AzFramework::WindowRequestBus::EventResult(
                m_refreshRate, m_nativeWindow, &AzFramework::WindowRequestBus::Events::GetDisplayRefreshRate);

            return RHI::ResultCode::Success;
        }

        void SwapChain::AddSubView()
        {
            NativeViewType* superView = reinterpret_cast<NativeViewType*>(m_nativeWindow);
            
            CGFloat screenScale = Platform::GetScreenScale();
            CGRect screenBounds = [superView bounds];
            m_metalView = [[RHIMetalView alloc] initWithFrame: screenBounds
                                                       scale: screenScale
                                                      device: m_mtlDevice];
            
            [m_metalView retain];
            [superView addSubview: m_metalView];
        }
    
        void SwapChain::ShutdownInternal()
        {
            if (m_viewController)
            {
                NativeWindowType* nativeWindow = static_cast<NativeWindowType*>(m_metalView.window);
                Platform::UnAttachViewController(nativeWindow, m_viewController);
                [m_viewController release];
                m_viewController = nullptr;
            }
            
            // Destroy the RHIMetalView
            if (m_metalView)
            {
                [m_metalView removeFromSuperview];
                [m_metalView release];
                m_metalView = nullptr;
            }
        }

        RHI::ResultCode SwapChain::InitImageInternal(const InitImageRequest& request)
        {
            Name name(AZStd::string::format("SwapChainImage_%d", request.m_imageIndex));
            Image& image = static_cast<Image&>(*request.m_image);
            
            //For metal we can only request swapchain texture right before writing into it which is handled by the appropriate scope itself.
            RHI::ImageDescriptor imgDescriptor = image.GetDescriptor();
            image.SetDescriptor(imgDescriptor);
            image.SetName(name);
            image.m_isSwapChainImage = true;            

            return RHI::ResultCode::Success;
        }

        void SwapChain::ShutdownResourceInternal(RHI::DeviceResource& resourceBase)
        {
            Image& image = static_cast<Image&>(resourceBase);
            image.m_memoryView = {};
        }

    
        void SwapChain::LogDrawable(const char* name, id<MTLTexture> readTexture)
        {
            AZ_Info("rrr", "LogDrawable %s", name);
            
            MTLPixelFormat pixelFormat = readTexture.pixelFormat;
            switch (pixelFormat)
            {
                case MTLPixelFormatBGRA8Unorm:
                    break;
                default:
                    AZ_Info("rrr", "  unsupported format %d", (uint32_t)pixelFormat);
                    return;
            }
            
            int bytesPerPixel = 4;
            int bytesPerRow   = readTexture.width * bytesPerPixel;
            int bytesPerImage = readTexture.height * bytesPerRow;
            
            id<MTLBuffer> readBuffer;
            //readBuffer = [readTexture.device newBufferWithLength:bytesPerImage options:MTLResourceStorageModeShared];
            readBuffer = [GetDevice().GetMtlDevice() newBufferWithLength:bytesPerImage options:MTLResourceStorageModeShared];
            if (!readBuffer)
            {
                AZ_Info("rrr", "  cannot create buffer");
                return;
            }
            
            static id<MTLDevice> sDevice;
            static id<MTLCommandQueue> sCommandQueue;
            static bool sCreated = false;
            
            if (!sCreated)
            {
                sCreated = true;
                sDevice = MTLCreateSystemDefaultDevice();
                sCommandQueue = [sDevice newCommandQueue];
            }
            
            id<MTLCommandBuffer> commandBuffer = [sCommandQueue commandBuffer];
            
            id <MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
            MTLOrigin readOrigin = MTLOriginMake(0, 0, 0);
            MTLSize readSize = MTLSizeMake(readTexture.width, readTexture.height, 1);
            
            {
                uint32_t* p = (uint32_t*)readBuffer.contents;
                memset(p, 0xff, readSize.height * readSize.width);
            }
            
            [blitEncoder pushDebugGroup:@"CopyDrawableTexture"];
            [blitEncoder copyFromTexture:readTexture
                             sourceSlice:0
                             sourceLevel:0
                            sourceOrigin:readOrigin
                              sourceSize:readSize
                                toBuffer:readBuffer
                       destinationOffset:0
                  destinationBytesPerRow:bytesPerRow
                destinationBytesPerImage:bytesPerImage];
            
            [blitEncoder endEncoding];
            
            [commandBuffer commit];
            [commandBuffer waitUntilCompleted];
            
            uint32_t* pixels = (uint32_t*)readBuffer.contents;
            
            uint32_t* row = pixels;
            for (int y = 0;  y < 2 /*readSize.height*/;  y++)
            {
                AZ_Info("rrr", "  row %d: %x %x %x %x", y, row[0], row[1], row[2], row[3]);
                /*
                 for (int x = 0;  x < readSize.width;  x++)
                 {
                 uint32_t pixel = row[x];
                 AZ_Info("rrr", "%d,%d %x\n", x, y, pixel);
                 }
                 */
                row += readSize.width;
            }
            
            [readBuffer release];
        }
    
        uint32_t SwapChain::PresentInternal()
        {
            const uint32_t currentImageIndex = GetCurrentImageIndex();
            
            AZ_Info("rrr", "SwapChain::PresentInternal %d of %d", currentImageIndex, GetImageCount());
            
            //if (currentImageIndex == 2)
            //static int counter = 0;
            //if (++counter % 10 == 0)
            {
                id<MTLTexture> readTexture = m_drawables[currentImageIndex].texture;
                LogDrawable("SwapChain::PresentInternal", readTexture);
            }
            
            //Preset the drawable
            Platform::PresentInternal(
                                          m_mtlCommandBuffer,
                                          m_drawables[currentImageIndex], GetDescriptor().m_verticalSyncInterval,
                                          m_refreshRate);
            
            [m_drawables[currentImageIndex] release];
            m_drawables[currentImageIndex] = nil;
            
#if defined(CARBONATED)
            return (currentImageIndex+ 1) % GetImageCount();
#else
            return (GetCurrentImageIndex() + 1) % GetImageCount();
#endif
        }

        RHI::ResultCode SwapChain::ResizeInternal(const RHI::SwapChainDimensions& dimensions, RHI::SwapChainDimensions* nativeDimensions)
        {
            if(m_metalView)
            {
                //Cache the new dimensions. We update the layer right before requesting the drawable.
                m_drawableSize = CGSizeMake(dimensions.m_imageWidth, dimensions.m_imageHeight);
            }
            else
            {
                if ([(id)m_nativeWindow isKindOfClass:[NativeWindowType class]])
                {
                    // Cache the window's view in order to get drawables
                    m_metalView = Platform::GetMetalView(m_nativeWindow);
                }
                else
                {
                    AddSubView();
                }
            }
            return RHI::ResultCode::Success;
        }
        
        void SwapChain::SetCommandBuffer(id <MTLCommandBuffer> mtlCommandBuffer)
        {
            m_mtlCommandBuffer = mtlCommandBuffer;
        }
    
        id<MTLTexture> SwapChain::RequestDrawable(bool isFrameCaptureEnabled)
        {
            AZ_PROFILE_SCOPE(RHI, "SwapChain::RequestDrawable");
            AZStd::lock_guard<AZStd::mutex> lock(m_drawablesMutex);
            m_metalView.metalLayer.framebufferOnly = !isFrameCaptureEnabled;
            const uint32_t currentImageIndex = GetCurrentImageIndex();
            if(m_drawables[currentImageIndex])
            {
                AZ_Info("rrr", "SwapChain::RequestDrawable already has drawable %d", currentImageIndex);
                //We already have a drawable for this frame. Lets return that
                //This can happen if a pass comes after Swapchain and wants to write to the swapchain texture
                return m_drawables[currentImageIndex].texture;
            }
            else
            {
                //Resize the layer if the dimensions dont align.
                if(m_drawableSize.width != m_metalView.metalLayer.drawableSize.width ||
                   m_drawableSize.height != m_metalView.metalLayer.drawableSize.height)
                {
                    Platform::ResizeInternal(m_metalView, m_drawableSize);
                }
                m_drawables[currentImageIndex] = [m_metalView.metalLayer nextDrawable];
                AZ_Assert(m_drawables[currentImageIndex], "Drawable can not be null");
                
                //Need this to make sure the drawable is alive for Present call
                [m_drawables[currentImageIndex] retain];
                
                id<MTLTexture> mtlDrawableTexture =  m_drawables[currentImageIndex].texture;
                if(isFrameCaptureEnabled)
                {
                    //If the swapchainimage's m_memoryView does not exist create one and if it already exists override the
                    //native texture pointer with the one received from the driver (i.e nextDrawable call).
                    Image* swapChainImage = static_cast<Image*>(GetCurrentImage());
                    if( swapChainImage->GetMemoryView().GetMemory())
                    {
                        swapChainImage->GetMemoryView().GetMemory()->OverrideResource(mtlDrawableTexture);
                    }
                    else
                    {
                        RHI::ImageDescriptor imgDescriptor = swapChainImage->GetDescriptor();
                        imgDescriptor.m_size.m_width = static_cast<uint32_t>(mtlDrawableTexture.width);
                        imgDescriptor.m_size.m_height = static_cast<uint32_t>(mtlDrawableTexture.height);
                        swapChainImage->SetDescriptor(imgDescriptor);
                        
                        RHI::Ptr<MetalResource> resc = MetalResource::Create(MetalResourceDescriptor{mtlDrawableTexture, ResourceType::MtlTextureType, swapChainImage->m_isSwapChainImage});
                        swapChainImage->m_memoryView = MemoryView(resc, 0, mtlDrawableTexture.allocatedSize, 0);
                    }
                }
                
                {
                    const int maximumDrawableCount = [m_metalView.metalLayer maximumDrawableCount];
                    AZ_Info("rrr", "SwapChain::RequestDrawable got new drawable %d (metal count %d)", currentImageIndex, maximumDrawableCount);
                    LogDrawable("SwapChain::RequestDrawable", mtlDrawableTexture);
                }

                return mtlDrawableTexture;
            }
        }

#if defined(CARBONATED) && defined(CARBONATED_DESIRED_FPS)
        void SwapChain::SetDesiredFPSInternal([[maybe_unused]] uint32_t desiredFPS)
        {
           // TODO. MAD-18568
        }
#endif
    }
}
