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
            AZ_Info("ddd", "LogDrawable %s", name);
            
            MTLPixelFormat pixelFormat = readTexture.pixelFormat;
            switch (pixelFormat)
            {
                case MTLPixelFormatBGRA8Unorm:
                    break;
                default:
                    AZ_Info("ddd", "  unsupported format %d", (uint32_t)pixelFormat);
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
                AZ_Info("ddd", "  cannot create buffer");
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
            
            [blitEncoder release];
            
            uint32_t* pixels = (uint32_t*)readBuffer.contents;
            
            uint32_t* row = pixels;
            for (int y = 0;  y < 2 /*readSize.height*/;  y++)
            {
                AZ_Info("ddd", "  row %d: %x %x %x %x", y, row[0], row[1], row[2], row[3]);
                /*
                 for (int x = 0;  x < readSize.width;  x++)
                 {
                 uint32_t pixel = row[x];
                 AZ_Info("ddd", "%d,%d %x\n", x, y, pixel);
                 }
                 */
                row += readSize.width;
            }
            
            [readBuffer release];
        }

        void SwapChain::AffectDrawable(const char* name, id<MTLTexture> writeTexture)
        {
        AZ_Info("ddd", "AffectDrawable %s", name);
        
        MTLPixelFormat pixelFormat = writeTexture.pixelFormat;
        switch (pixelFormat)
        {
            case MTLPixelFormatBGRA8Unorm:
                break;
            default:
                AZ_Info("ddd", "  unsupported format %d", (uint32_t)pixelFormat);
                return;
        }
        
        size_t sourceWidth = writeTexture.width ;
        size_t sourceHeight = writeTexture.height;
        size_t resultWidth = sourceWidth;
        size_t resultHeight = sourceHeight;
        
        static id<MTLDevice> sDevice;
        static id<MTLCommandQueue> sCommandQueue;
        static bool sCreated = false;
        
        static const float sVerticesR180[4 * 4] =
        {
            -1.0f, -1.0f,  1.0f, 0.0f,
             1.0f, -1.0f,  0.0f, 0.0f,
            -1.0f,  1.0f,  1.0f, 1.0f,
             1.0f,  1.0f,  0.0f, 1.0f
        };
        const float* vertexData = sVerticesR180;
        
        if (!sCreated)
        {
            sCreated = true;
            sDevice = MTLCreateSystemDefaultDevice();
            sCommandQueue = [sDevice newCommandQueue];
        }

        @autoreleasepool
        {
            id<MTLBuffer> vertexBuffer = [sDevice newBufferWithBytes:vertexData length:sizeof(sVerticesR180) options:MTLResourceCPUCacheModeDefaultCache];
            vertexBuffer.label = @"TestVertices";

            MTLTextureDescriptor *textureDescriptor = [[MTLTextureDescriptor alloc] init];
            textureDescriptor.textureType = MTLTextureType2D;
            textureDescriptor.width = resultWidth;
            textureDescriptor.height = resultHeight;
            textureDescriptor.depth = 1;
            textureDescriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
            textureDescriptor.storageMode = MTLStorageModeShared;
            //textureDescriptor.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
            textureDescriptor.usage = MTLTextureUsageShaderRead;
            id<MTLTexture> sourceTexture = [sDevice newTextureWithDescriptor:textureDescriptor];
            [textureDescriptor release];
            if (!sourceTexture)
            {
                AZ_Error("ddd", false, "Texture creation error");
                return;
            }

            MTLRenderPassDescriptor *renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
            MTLRenderPassColorAttachmentDescriptor* colorAttachment = renderPassDescriptor.colorAttachments[0];
            colorAttachment.texture = writeTexture;
            colorAttachment.loadAction = MTLLoadActionClear;
            colorAttachment.storeAction = MTLStoreActionStore;
            colorAttachment.clearColor = MTLClearColorMake(1.0f, 0.0f, 0.0f, 1.0f);

            id<MTLCommandBuffer> commandBuffer = [sCommandQueue commandBuffer];
            commandBuffer.label = @"TestCommand";
            
            // ***************************
            // render source texture

            id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
            renderEncoder.label = @"TestRenderEncoder";

            MTLDepthStencilDescriptor *depthStencilDesc = [[MTLDepthStencilDescriptor alloc] init];
            depthStencilDesc.depthCompareFunction = MTLCompareFunctionAlways;
            depthStencilDesc.depthWriteEnabled = NO;
            id<MTLDepthStencilState> depthStencilState = [sDevice newDepthStencilStateWithDescriptor:depthStencilDesc];

            [renderEncoder setDepthStencilState:depthStencilState];
            
            [renderEncoder pushDebugGroup:@"AffectTexture"];
            [renderEncoder setViewport:(MTLViewport){0.0, 0.0, (double)resultWidth, (double)resultHeight, 0.0, 1.0 }];
        

            //id<MTLLibrary> defaultLibrary = [sDevice newDefaultLibrary];
        
            NSString* libraryShaders = @"\n\
#include <metal_stdlib>\n\
using namespace metal;\n\
\n\
typedef struct\n\
{\n\
    packed_float2 position;\n\
    packed_float2 texcoord;\n\
} Vertex;\n\
\n\
typedef struct\n\
{\n\
    float4 position [[position]];\n\
    float2 texcoord;\n\
} Varyings;\n\
\n\
vertex Varyings TestVertexProgram(constant Vertex* verticies [[ buffer(0) ]], unsigned int vid [[ vertex_id ]])\n\
{\n\
    Varyings out;\n\
    constant Vertex& v = verticies[vid];\n\
    out.position = float4(v.position.x, v.position.y, 0.0, 1.0);\n\
    out.texcoord = v.texcoord;\n\
    return out;\n\
}\n\
\n\
fragment half4 TestFragmentProgram(Varyings in [[ stage_in ]], texture2d<float, access::sample> texture [[ texture(0) ]])\n\
{\n\
    constexpr sampler s(address::clamp_to_edge, filter::linear);\n\
    float3 color = float3(texture.sample(s, in.texcoord).rgb);\n\
    color = float3(1.0, 0.0, 0.0);\n\
    return half4(half3(color), 1.0);\n\
}";
            NSError* error = nil;
            id<MTLLibrary> defaultLibrary = [sDevice newLibraryWithSource:libraryShaders options:nil error:&error];
            if (error)
            {
                AZ_Error("ddd", false, "Cannot ctreate test library (might be shader error)");
                return;
            }

            id<MTLFunction> fragmentProgram = [defaultLibrary newFunctionWithName:@"TestFragmentProgram"];
            id<MTLFunction> vertexProgram = [defaultLibrary newFunctionWithName:@"TestVertexProgram"];
        
            MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
            pipelineStateDescriptor.label = @"TestPipeline";
            pipelineStateDescriptor.rasterSampleCount = 1;
            [pipelineStateDescriptor setVertexFunction:vertexProgram];
            [pipelineStateDescriptor setFragmentFunction:fragmentProgram];
            pipelineStateDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
            pipelineStateDescriptor.depthAttachmentPixelFormat = MTLPixelFormatInvalid;
            pipelineStateDescriptor.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
        
            id<MTLRenderPipelineState> pipelineState = [sDevice newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
            if (!pipelineState)
            {
                AZ_Error("ddd", false, "Cannot create pipeline state, error %@", error);
                return;
            }
            [pipelineStateDescriptor release];
        
            [renderEncoder setRenderPipelineState:pipelineState];
        
            [renderEncoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
            [renderEncoder setFragmentTexture:sourceTexture atIndex:0];
            [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:1];
            [renderEncoder popDebugGroup];
            
            [renderEncoder endEncoding];
            
            __block bool commandSuccess = true;
            [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer)
             {
                if (buffer.status == MTLCommandBufferStatusError)
                {
                    [[maybe_unused]] const char * cbLabel = [ buffer.label UTF8String ];
                    [[maybe_unused]] const int errorCode = static_cast<int>(buffer.error.code);
                    AZ_Error("ddd", false, "Command buffer %s failed to execute: %d", cbLabel, errorCode);
                    commandSuccess = false;
                }
            }];
            
            [commandBuffer commit];
            [commandBuffer waitUntilCompleted];

            [renderEncoder release];
            [vertexBuffer release];
            [vertexProgram release];
            [fragmentProgram release];
            [defaultLibrary release];

            AZ_Info("ddd", "Painted drawable red at %f", double(clock_gettime_nsec_np(CLOCK_UPTIME_RAW)) / 1000000000.0);

            if (!commandSuccess)
            {
                AZ_Error("ddd", false, "Command buffer failed");
                return;
            }
        }
}

        void SwapChain::AffectDrawable(id<MTLTexture> writeTexture)
        {
            AZ_Info("ddd", "AffectDrawable");
            
            MTLPixelFormat pixelFormat = writeTexture.pixelFormat;
            switch (pixelFormat)
            {
                case MTLPixelFormatBGRA8Unorm:
                    break;
                default:
                    AZ_Info("ddd", "  unsupported format %d", (uint32_t)pixelFormat);
                    return;
            }
            
            size_t sourceWidth = writeTexture.width ;
            size_t sourceHeight = writeTexture.height;
            size_t resultWidth = sourceWidth;
            size_t resultHeight = sourceHeight;
            
            static id<MTLDevice> sDevice;
            static bool sCreated = false;
            
            if (!sCreated)
            {
                sCreated = true;
                sDevice = MTLCreateSystemDefaultDevice();
            }

            static const float sVerticesR180[4 * 4] =
            {
                -1.0f, -1.0f,  1.0f, 0.0f,
                 1.0f, -1.0f,  0.0f, 0.0f,
                -1.0f,  1.0f,  1.0f, 1.0f,
                 1.0f,  1.0f,  0.0f, 1.0f
            };
            const float* vertexData = sVerticesR180;
            
            @autoreleasepool
            {
                id<MTLBuffer> vertexBuffer = [sDevice newBufferWithBytes:vertexData length:sizeof(sVerticesR180) options:MTLResourceCPUCacheModeDefaultCache];
                vertexBuffer.label = @"TestVertices";

                MTLTextureDescriptor *textureDescriptor = [[MTLTextureDescriptor alloc] init];
                textureDescriptor.textureType = MTLTextureType2D;
                textureDescriptor.width = resultWidth;
                textureDescriptor.height = resultHeight;
                textureDescriptor.depth = 1;
                textureDescriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
                textureDescriptor.storageMode = MTLStorageModeShared;
                //textureDescriptor.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
                textureDescriptor.usage = MTLTextureUsageShaderRead;
                id<MTLTexture> sourceTexture = [sDevice newTextureWithDescriptor:textureDescriptor];
                [textureDescriptor release];
                if (!sourceTexture)
                {
                    AZ_Error("ddd", false, "Texture creation error");
                    return;
                }

                MTLRenderPassDescriptor *renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
                MTLRenderPassColorAttachmentDescriptor* colorAttachment = renderPassDescriptor.colorAttachments[0];
                colorAttachment.texture = writeTexture;
                colorAttachment.loadAction = MTLLoadActionClear;
                colorAttachment.storeAction = MTLStoreActionStore;
                colorAttachment.clearColor = MTLClearColorMake(1.0f, 0.0f, 0.0f, 1.0f);

                // ***************************
                // render source texture

                id<MTLRenderCommandEncoder> renderEncoder = [m_mtlCommandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
                renderEncoder.label = @"TestRenderEncoder";

                MTLDepthStencilDescriptor *depthStencilDesc = [[MTLDepthStencilDescriptor alloc] init];
                depthStencilDesc.depthCompareFunction = MTLCompareFunctionAlways;
                depthStencilDesc.depthWriteEnabled = NO;
                id<MTLDepthStencilState> depthStencilState = [sDevice newDepthStencilStateWithDescriptor:depthStencilDesc];

                [renderEncoder setDepthStencilState:depthStencilState];
                
                [renderEncoder pushDebugGroup:@"AffectTexture"];
                [renderEncoder setViewport:(MTLViewport){0.0, 0.0, (double)resultWidth, (double)resultHeight, 0.0, 1.0 }];
            

                //id<MTLLibrary> defaultLibrary = [sDevice newDefaultLibrary];
            
                NSString* libraryShaders = @"\n\
    #include <metal_stdlib>\n\
    using namespace metal;\n\
    \n\
    typedef struct\n\
    {\n\
        packed_float2 position;\n\
        packed_float2 texcoord;\n\
    } Vertex;\n\
    \n\
    typedef struct\n\
    {\n\
        float4 position [[position]];\n\
        float2 texcoord;\n\
    } Varyings;\n\
    \n\
    vertex Varyings TestVertexProgram(constant Vertex* verticies [[ buffer(0) ]], unsigned int vid [[ vertex_id ]])\n\
    {\n\
        Varyings out;\n\
        constant Vertex& v = verticies[vid];\n\
        out.position = float4(v.position.x, v.position.y, 0.0, 1.0);\n\
        out.texcoord = v.texcoord;\n\
        return out;\n\
    }\n\
    \n\
    fragment half4 TestFragmentProgram(Varyings in [[ stage_in ]], texture2d<float, access::sample> texture [[ texture(0) ]])\n\
    {\n\
        constexpr sampler s(address::clamp_to_edge, filter::linear);\n\
        float3 color = float3(texture.sample(s, in.texcoord).rgb);\n\
        color = float3(1.0, 0.0, 0.0);\n\
        return half4(half3(color), 1.0);\n\
    }";
                NSError* error = nil;
                id<MTLLibrary> defaultLibrary = [sDevice newLibraryWithSource:libraryShaders options:nil error:&error];
                if (error)
                {
                    AZ_Error("ddd", false, "Cannot ctreate test library (might be shader error)");
                    return;
                }

                id<MTLFunction> fragmentProgram = [defaultLibrary newFunctionWithName:@"TestFragmentProgram"];
                id<MTLFunction> vertexProgram = [defaultLibrary newFunctionWithName:@"TestVertexProgram"];
            
                MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
                pipelineStateDescriptor.label = @"TestPipeline";
                pipelineStateDescriptor.rasterSampleCount = 1;
                [pipelineStateDescriptor setVertexFunction:vertexProgram];
                [pipelineStateDescriptor setFragmentFunction:fragmentProgram];
                pipelineStateDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
                pipelineStateDescriptor.depthAttachmentPixelFormat = MTLPixelFormatInvalid;
                pipelineStateDescriptor.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
            
                id<MTLRenderPipelineState> pipelineState = [sDevice newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
                if (!pipelineState)
                {
                    AZ_Error("ddd", false, "Cannot create pipeline state, error %@", error);
                    return;
                }
                [pipelineStateDescriptor release];
            
                [renderEncoder setRenderPipelineState:pipelineState];

                [renderEncoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
                [renderEncoder setFragmentTexture:sourceTexture atIndex:0];
                [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:1];
                [renderEncoder popDebugGroup];
                
                [renderEncoder endEncoding];
                
                [renderEncoder release];
                //[pipelineState release];
                //[vertexBuffer release];
                //[vertexProgram release];
                //[fragmentProgram release];
                //[defaultLibrary release];

                AZ_Info("ddd", "Added paint drawable red");
            }
        }

        uint32_t SwapChain::PresentInternal()
        {
            const uint32_t currentImageIndex = GetCurrentImageIndex();
            
            {
                id<MTLTexture> readTexture = m_drawables[currentImageIndex].texture;
                const char * label = [ m_mtlCommandBuffer.label UTF8String ];
                AZ_Info("ddd", "SwapChain::PresentInternal %d of %d, texture %p, buffer %p (%s), at %f",
                        currentImageIndex, GetImageCount(),
                        readTexture, m_mtlCommandBuffer, label,
                        double(clock_gettime_nsec_np(CLOCK_UPTIME_RAW)) / 1000000000.0);

                //AffectDrawable(readTexture);

                //const AZStd::string s = AZStd::string::format("SwapChain::PresentInternal %d, texture %p, at %f", currentImageIndex, readTexture);
                //LogDrawable(s.c_str(), readTexture);
                //AffectDrawable(s.c_str(), readTexture);
                //LogDrawable(s.c_str(), readTexture);
            }

            //Preset the drawable
            Platform::PresentInternal(
                                          m_mtlCommandBuffer,
                                          m_drawables[currentImageIndex], GetDescriptor().m_verticalSyncInterval,
                                          m_refreshRate);
            
#if defined(CARBONATED)
            {
                AZStd::lock_guard<AZStd::mutex> lock(m_drawablesMutex);
                m_storedDrawables.push_back(StoredDrawable(m_drawables[currentImageIndex] , m_mtlCommandBuffer));
            }
#else
            [m_drawables[currentImageIndex] release];
#endif
            m_drawables[currentImageIndex] = nil;
            
#if defined(CARBONATED)
            return (currentImageIndex+ 1) % GetImageCount();
#else
            return (GetCurrentImageIndex() + 1) % GetImageCount();
#endif
        }

#if defined(CARBONATED)
        void SwapChain::ReleaseDrawable(id <MTLCommandBuffer>   mtlCommandBuffer)
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_drawablesMutex);
            AZ_Info("sss", "SwapChain::ReleaseDrawable for buffer %p (has %d) at %f",
                    mtlCommandBuffer, m_storedDrawables.size(), double(clock_gettime_nsec_np(CLOCK_UPTIME_RAW)) / 1000000000.0);
            for (auto it = m_storedDrawables.begin(); it != m_storedDrawables.end(); it++)
                if (mtlCommandBuffer == it->m_mtlCommandBuffer)
                {
                    AZ_Info("sss", "  release texture %p", it->m_drawable.texture);
                    [it->m_drawable release];
                    m_storedDrawables.erase(it);
                    break;
                }
        }
#endif

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
                AZ_Info("ddd", "SwapChain::RequestDrawable already has drawable %d", currentImageIndex);
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
                    AZ_Info("ddd", "SwapChain::RequestDrawable got new drawable %d (metal count %d), texture %p, capture enabled %d, at %f",
                            currentImageIndex, maximumDrawableCount, mtlDrawableTexture, isFrameCaptureEnabled,
                            double(clock_gettime_nsec_np(CLOCK_UPTIME_RAW)) / 1000000000.0);
                    //LogDrawable("SwapChain::RequestDrawable", mtlDrawableTexture);
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
