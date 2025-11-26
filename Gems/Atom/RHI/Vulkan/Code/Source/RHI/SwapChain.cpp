/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include "Atom_RHI_Vulkan_Platform.h"
#include <Atom/RHI/PipelineStateDescriptor.h>
#include <Atom/RHI/RHISystemInterface.h>
#include <Atom/RHI/XRRenderingInterface.h>
#include <Atom/RHI.Reflect/ClearValue.h>
#include <Atom/RHI.Reflect/ImageScopeAttachmentDescriptor.h>
#include <Atom/RHI.Reflect/ImagePoolDescriptor.h>
#include <Atom/RHI.Reflect/Vulkan/XRVkDescriptors.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Console/ILogger.h>
#include <Atom/RHI.Reflect/Vulkan/Conversion.h>
#include <RHI/Device.h>
#include <RHI/Image.h>
#include <RHI/ImagePool.h>
#include <RHI/GraphicsPipeline.h>
#include <RHI/Queue.h>
#include <RHI/RenderPass.h>
#include <RHI/SwapChain.h>
#include <RHI/ReleaseContainer.h>
#include <Atom/RHI.Reflect/VkAllocator.h>

#if defined(CARBONATED) && defined(AZ_PLATFORM_ANDROID) && defined(CARBONATED_DESIRED_FPS) && defined(CARBONATED_USE_SWAPPY)
#include <AzCore/Android/JNI/Object.h>
#include <AzCore/Android/Utils.h>
#include <swappy/swappyVk.h>
#endif // CARBONATED && AZ_PLATFORM_ANDROID && CARBONATED_DESIRED_FPS && CARBONATED_USE_SWAPPY


#if defined(CARBONATED) && !defined(_RELEASE)
#include <AzFramework/IO/LocalFileIO.h>
#endif

namespace AZ
{
    namespace Vulkan
    {
#if defined(CARBONATED) && !defined(_RELEASE)
        bool WriteBMP(
            const char* filePath,
            uint32_t width,
            uint32_t height,
            const uint8_t* rgbaData, // ВАЖНО: RGBA, НЕ BGRA
            size_t rowPitch)
        {
            const uint32_t bytesPerPixel = 4;
            const uint32_t tightRowSize = width * bytesPerPixel;
            const uint32_t dataSize = tightRowSize * height;

// --- Плотные структуры без паддинга ---
#pragma pack(push, 1)
            struct BMPHeader
            {
                uint16_t bfType; // 'BM'
                uint32_t bfSize;
                uint16_t bfReserved1;
                uint16_t bfReserved2;
                uint32_t bfOffBits; // 54 = 14 + 40
            };

            struct BMPInfoHeader
            {
                uint32_t biSize; // 40
                int32_t biWidth;
                int32_t biHeight;
                uint16_t biPlanes; // 1
                uint16_t biBitCount; // 32
                uint32_t biCompression; // 0 = BI_RGB
                uint32_t biSizeImage; // размер данных
                int32_t biXPelsPerMeter; // 72 dpi ~ 2835
                int32_t biYPelsPerMeter;
                uint32_t biClrUsed;
                uint32_t biClrImportant;
            };
#pragma pack(pop)

            BMPHeader header{};
            BMPInfoHeader info{};

            header.bfType = 0x4D42; // 'BM'
            header.bfOffBits = sizeof(BMPHeader) + sizeof(BMPInfoHeader);
            header.bfSize = header.bfOffBits + dataSize;

            info.biSize = sizeof(BMPInfoHeader);
            info.biWidth = static_cast<int32_t>(width);
            info.biHeight = -static_cast<int32_t>(height); // top-down
            info.biPlanes = 1;
            info.biBitCount = 32; // B,G,R,X
            info.biCompression = 0; // BI_RGB
            info.biSizeImage = dataSize;
            info.biXPelsPerMeter = 2835;
            info.biYPelsPerMeter = 2835;
            info.biClrUsed = 0;
            info.biClrImportant = 0;

            // --- Перегоняем с учётом rowPitch и правильного порядка каналов ---
            AZStd::vector<uint8_t> tight(dataSize);

            for (uint32_t y = 0; y < height; ++y)
            {
                const uint8_t* src = rgbaData + y * rowPitch;
                uint8_t* dst = tight.data() + y * tightRowSize;

                for (uint32_t x = 0; x < width; ++x)
                {
                    const uint8_t R = src[x * 4 + 0];
                    const uint8_t G = src[x * 4 + 1];
                    const uint8_t B = src[x * 4 + 2];
                    // const uint8_t A = src[x*4 + 3]; // BMP игнорит

                    // BMP (32 bpp, BI_RGB) — порядок байт в файле: B, G, R, X
                    dst[x * 4 + 0] = B;
                    dst[x * 4 + 1] = G;
                    dst[x * 4 + 2] = R;
                    dst[x * 4 + 3] = 255; // альфа можно забить
                }
            }

            // --- Пишем файл ---
            AZ::IO::HandleType handle;
            AZ::IO::Result openRes =
                AZ::IO::LocalFileIO::GetInstance()->Open(filePath, AZ::IO::OpenMode::ModeWrite | AZ::IO::OpenMode::ModeBinary, handle);

            if (!openRes)
                return false;

            AZ::u64 written = 0;
            AZ::IO::LocalFileIO::GetInstance()->Write(handle, &header, sizeof(header), &written);
            AZ::IO::LocalFileIO::GetInstance()->Write(handle, &info, sizeof(info), &written);
            AZ::IO::LocalFileIO::GetInstance()->Write(handle, tight.data(), dataSize, &written);
            AZ::IO::LocalFileIO::GetInstance()->Close(handle);

            return true;
        }
#endif
        static bool IsDefaultSwapChainNeeded()
        {
            auto* xrSystem = RHI::RHISystemInterface::Get()->GetXRSystem();
            return !xrSystem || xrSystem->IsDefaultRenderPipelineNeeded();
        }

        RHI::Ptr<SwapChain> SwapChain::Create()
        {
            return aznew SwapChain();
        }

        VkSwapchainKHR SwapChain::GetNativeSwapChain() const
        {
            return m_nativeSwapChain;
        }

        const SwapChain::FrameContext& SwapChain::GetCurrentFrameContext() const
        {
             return m_currentFrameContext;
        }

        const WSISurface& SwapChain::GetSurface() const
        {
            return *m_surface;
        }

        const CommandQueue& SwapChain::GetPresentationQueue() const
        {
            return *m_presentationQueue;
        }

        void SwapChain::QueueBarrier(const VkPipelineStageFlags src, const VkPipelineStageFlags dst, const VkImageMemoryBarrier& imageBarrier)
        {
            m_swapChainBarrier.m_barrier = imageBarrier;
            m_swapChainBarrier.m_srcPipelineStages = src;
            m_swapChainBarrier.m_dstPipelineStages = dst;
            m_swapChainBarrier.m_isValid = true;
        }

        bool SwapChain::ProcessRecreation()
        {
            if (m_pendingRecreation)
            {
                VkSwapchainKHR oldSwapchain = m_nativeSwapChain;
                ShutdownImages();
                CreateSwapchain();
                // Destroy the old swapchain AFTER creating the new one (because it's used for building the new one)
                InvalidateNativeSwapChain(oldSwapchain);
                InitImages();

                m_pendingRecreation = false;
                return true;
            }
            return false;
        }

        void SwapChain::SetVerticalSyncIntervalInternal(uint32_t previousVsyncInterval)
        {
            if (GetDescriptor().m_verticalSyncInterval == 0 || previousVsyncInterval == 0)
            {
                // The presentation mode may change when transitioning to or from a vsynced presentation mode
                // In this case, the swapchain must be recreated.
                m_pendingRecreation = true;
            }
        }

#if defined(CARBONATED) && defined(CARBONATED_DESIRED_FPS)
        static int ImageNumber = 0;

        void SwapChain::SaveSetOfPresentImagesInternal()
        {
            m_currentImage = ++ImageNumber;
            m_currentPresentIndexToSave = (int)m_swapchainNativeImages.size();
            auto& device = static_cast<Device&>(GetDevice());
            device.StartWriteCLasBMP(m_currentImage);
        }

        void SwapChain::SetDesiredFPSInternal([[maybe_unused]] uint32_t desiredFPS)
        {
#if defined(AZ_PLATFORM_ANDROID) && defined(CARBONATED_USE_SWAPPY)
            if (m_refreshNs == 0 || desiredFPS <= 0)
            {
                AZ_Error("SwapChain", false, "Swappy not initialized or invalid FPS");
                return;
            }

            // 1. Target frame time for desired FPS
            uint64_t targetNs = static_cast<uint64_t>(1000000000ull / desiredFPS);

            // 2. How many vsync intervals does this correspond to?
            uint64_t interval = (targetNs + m_refreshNs / 2) / m_refreshNs;
            if (interval < 1)
            {
                interval = 1;
            }

            // 3. Real target we give to Swappy
            uint64_t swappyTargetNs = interval * m_refreshNs;

            // Set SwapIntervalNS
            auto& device = static_cast<Device&>(GetDevice());
            SwappyVk_setSwapIntervalNS(device.GetNativeDevice(), m_nativeSwapChain, swappyTargetNs);
#endif // AZ_PLATFORM_ANDROID && CARBONATED_USE_SWAPPY
        }
#endif // CARBONATED && CARBONATED_DESIRED_FPS

        void SwapChain::SetNameInternal([[maybe_unused]] const AZStd::string_view& name)
        {
            // On some GPUs, like the Adreno 740, setting the name of the swapchain causes a crash, so we don't do it.
        }

        RHI::ResultCode SwapChain::InitInternal(RHI::Device& baseDevice, const RHI::SwapChainDescriptor& descriptor, RHI::SwapChainDimensions* nativeDimensions)
        {
            RHI::ResultCode result = RHI::ResultCode::Success;
            RHI::DeviceObject::Init(baseDevice);

            auto& device = static_cast<Device&>(GetDevice());
            m_dimensions = descriptor.m_dimensions;

            if (descriptor.m_isXrSwapChain)
            {
                if (nativeDimensions)
                {
                    *nativeDimensions = m_dimensions;
                }
            }
            else
            {
                result = BuildSurface(descriptor);
                RETURN_RESULT_IF_UNSUCCESSFUL(result);

                auto& presentationQueue = device.GetCommandQueueContext().GetOrCreatePresentationCommandQueue(*this);
                m_presentationQueue = &presentationQueue;

                if (IsDefaultSwapChainNeeded())
                {
                    result = CreateSwapchain();
                    RETURN_RESULT_IF_UNSUCCESSFUL(result);
                }

                if (nativeDimensions)
                {
                    // Fill out the real swapchain dimensions to return
                    *nativeDimensions = m_dimensions;
                    nativeDimensions->m_imageFormat = ConvertFormat(m_surfaceFormat.format);
                }
            }

            SetName(GetName());
            return result;
        }

        void SwapChain::ShutdownInternal()
        {
            //Nothing to clean as all the native objects for xr swapchain is handles by xr modules
            if (GetDescriptor().m_isXrSwapChain)
            {
                return;
            }

            InvalidateNativeSwapChain(m_nativeSwapChain);
            m_nativeSwapChain = VK_NULL_HANDLE;
            InvalidateSurface();
            m_presentationQueue = nullptr;

            m_swapchainNativeImages.clear();
            m_currentFrameContext = {};
        }

        RHI::ResultCode SwapChain::InitImageInternal(const RHI::SwapChain::InitImageRequest& request)
        {
            auto& device = static_cast<Device&>(GetDevice());
            Image* image = static_cast<Image*>(request.m_image);
            RHI::ImageDescriptor imageDesc = request.m_descriptor;
            RHI::ResultCode result = RHI::ResultCode::Success;

            // XR swapchains will retrieve the native swapchain image from xr system where as non-xr
            // swapchains will use the images created internally (i.e RHI::Vulkan)
            if (GetDescriptor().m_isXrSwapChain)
            {
                XRSwapChainDescriptor xrSwapChainDescriptor;
                xrSwapChainDescriptor.m_inputData.m_swapChainIndex = GetDescriptor().m_xrSwapChainIndex;
                xrSwapChainDescriptor.m_inputData.m_swapChainImageIndex = request.m_imageIndex;

                result = GetXRSystem()->GetSwapChainImage(&xrSwapChainDescriptor);
                AZ_Assert(result == RHI::ResultCode::Success, "Xr Session creation was not successful");

                result = image->Init(device, xrSwapChainDescriptor.m_outputData.m_nativeImage, imageDesc);
            }
            else
            {
                if (IsDefaultSwapChainNeeded())
                {
                    imageDesc.m_format = ConvertFormat(m_surfaceFormat.format);
                    result = image->Init(device, m_swapchainNativeImages[request.m_imageIndex], imageDesc);
                }
            }

            if (result != RHI::ResultCode::Success)
            {
                AZ_Assert(false, "Failed to initialize swapchain image %d", request.m_imageIndex);
                return result;
            }

            Name name(AZStd::string::format("SwapChainImage_%d", request.m_imageIndex));
            image->SetName(name);

            return result;
        }

        RHI::ResultCode SwapChain::ResizeInternal(const RHI::SwapChainDimensions& dimensions, RHI::SwapChainDimensions* nativeDimensions)
        {
            VkSwapchainKHR oldSwapchain = m_nativeSwapChain;
            auto& device = static_cast<Device&>(GetDevice());
            m_dimensions = dimensions;

            auto& presentationQueue = device.GetCommandQueueContext().GetOrCreatePresentationCommandQueue(*this);
            m_presentationQueue = &presentationQueue;

            CreateSwapchain();

            if (nativeDimensions)
            {
                *nativeDimensions = m_dimensions;
                // [ATOM-4840] This is a workaround when the windows is minimized (0x0 size).
                // Add proper support to handle this case.
                nativeDimensions->m_imageHeight = AZStd::max(m_dimensions.m_imageHeight, 1u);
                nativeDimensions->m_imageWidth = AZStd::max(m_dimensions.m_imageWidth, 1u);

                nativeDimensions->m_imageFormat = ConvertFormat(m_surfaceFormat.format);
            }

            // Destroy the old swapchain AFTER creating the new one (because it's used for building the new one)
            InvalidateNativeSwapChain(oldSwapchain);

            return RHI::ResultCode::Success;
        }

        uint32_t SwapChain::PresentInternal()
        {
            // No need to present a xr swapchain
            if (GetDescriptor().m_isXrSwapChain)
            {
                return 0;
            }

            auto& device = static_cast<Device&>(GetDevice());

            const uint32_t imageIndex = GetCurrentImageIndex();

            auto presentCommand = [this, imageIndex, presentSemaphore = m_currentFrameContext.m_presentableSemaphore, &device](void* queue)
            {
                Queue* vulkanQueue = static_cast<Queue*>(queue);
                VkSemaphore waitSemaphore = presentSemaphore->GetNativeSemaphore();
                if (m_swapChainBarrier.m_isValid)
                {
                    // The presentation and graphic queue belong to different families so
                    // we need to add an ownership transfer to the presentation queue.
                    auto commandList = device.AcquireCommandList(vulkanQueue->GetId().m_familyIndex);
                    commandList->BeginCommandBuffer();
                    device.GetContext().CmdPipelineBarrier(
                        commandList->GetNativeCommandBuffer(),
                        m_swapChainBarrier.m_srcPipelineStages,
                        m_swapChainBarrier.m_dstPipelineStages,
                        VK_DEPENDENCY_BY_REGION_BIT,
                        0,
                        nullptr,
                        0,
                        nullptr,
                        1,
                        &m_swapChainBarrier.m_barrier);
                    commandList->EndCommandBuffer();

                    // This semaphore will be signaled once the transfer has completed.
                    auto transferSemaphore = device.GetSwapChainSemaphoreAllocator().Allocate();
                    // We wait until the swapchain image has finished being rendered to initialize the
                    // ownership transfer.
                    vulkanQueue->SubmitCommandBuffers(
                        AZStd::vector<RHI::Ptr<CommandList>>{ commandList },
                        AZStd::vector<Semaphore::WaitSemaphore>{ AZStd::make_pair(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, presentSemaphore) },
                        AZStd::vector<RHI::Ptr<Semaphore>>{ transferSemaphore },
                        {},
                        nullptr);

                    // The presentation engine must wait until the ownership transfer has completed.
                    waitSemaphore = transferSemaphore->GetNativeSemaphore();
                    transferSemaphore->SignalEvent();
                    // This will not deallocate immediately. It has a collect latency.
                    device.GetSwapChainSemaphoreAllocator().DeAllocate(transferSemaphore);
                    m_swapChainBarrier.m_isValid = false;
                }

// ---------------- DEBUG SCREENSHOT ----------------
                if (m_currentPresentIndexToSave > 0)
                {
                    VkDevice vkDevice = device.GetNativeDevice();
                    VkImage srcImage = m_swapchainNativeImages[imageIndex];
                    uint32_t width = m_dimensions.m_imageWidth;
                    uint32_t height = m_dimensions.m_imageHeight;

                    // Создаём staging image (linear, host-visible)
                    VkImageCreateInfo imgInfo{};
                    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                    imgInfo.imageType = VK_IMAGE_TYPE_2D;
                    imgInfo.format = m_surfaceFormat.format; // VK_FORMAT_B8G8R8A8_UNORM
                    imgInfo.extent = { width, height, 1 };
                    imgInfo.mipLevels = 1;
                    imgInfo.arrayLayers = 1;
                    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                    imgInfo.tiling = VK_IMAGE_TILING_LINEAR;
                    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

                    VkImage dstImage = VK_NULL_HANDLE;
                    device.GetContext().CreateImage(vkDevice, &imgInfo, VkSystemAllocator::Get(), &dstImage);

                    VkMemoryRequirements memReq{};
                    device.GetContext().GetImageMemoryRequirements(vkDevice, dstImage, &memReq);

                    // --- выбор host-visible памяти ---
                    uint32_t memoryTypeIndex = 0;
                    {
                        VkPhysicalDeviceMemoryProperties memProps;
                        device.GetContext().GetPhysicalDeviceMemoryProperties(
                            static_cast<const PhysicalDevice&>(device.GetPhysicalDevice()).GetNativePhysicalDevice(), &memProps);

                        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
                        {
                            if ((memReq.memoryTypeBits & (1 << i)) &&
                                (memProps.memoryTypes[i].propertyFlags &
                                 (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)))
                            {
                                memoryTypeIndex = i;
                                break;
                            }
                        }
                    }

                    VkMemoryAllocateInfo allocInfo{};
                    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                    allocInfo.allocationSize = memReq.size;
                    allocInfo.memoryTypeIndex = memoryTypeIndex;

                    VkDeviceMemory dstMemory = VK_NULL_HANDLE;
                    device.GetContext().AllocateMemory(vkDevice, &allocInfo, VkSystemAllocator::Get(), &dstMemory);
                    device.GetContext().BindImageMemory(vkDevice, dstImage, dstMemory, 0);

                    // Командный буфер
                    auto cmdList = device.AcquireCommandList(vulkanQueue->GetId().m_familyIndex);
                    cmdList->BeginCommandBuffer();
                    VkCommandBuffer cmd = cmdList->GetNativeCommandBuffer();

                    // Барьер: PRESENT_SRC → TRANSFER_SRC
                    VkImageMemoryBarrier b1{};
                    b1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    b1.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                    b1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    b1.image = srcImage;
                    b1.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                    b1.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
                    b1.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

                    device.GetContext().CmdPipelineBarrier(
                        cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b1);

                    // Барьер: dstImage → TRANSFER_DST
                    VkImageMemoryBarrier b2{};
                    b2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    b2.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                    b2.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    b2.image = dstImage;
                    b2.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                    b2.srcAccessMask = 0;
                    b2.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                    device.GetContext().CmdPipelineBarrier(
                        cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b2);

                    // Копирование
                    VkImageCopy region{};
                    region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
                    region.dstSubresource = region.srcSubresource;
                    region.extent = { width, height, 1 };

                    device.GetContext().CmdCopyImage(
                        cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

                    // dstImage → GENERAL (CPU readable)
                    VkImageMemoryBarrier b3{};
                    b3.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    b3.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    b3.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                    b3.image = dstImage;
                    b3.subresourceRange = b2.subresourceRange;
                    b3.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    b3.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

                    device.GetContext().CmdPipelineBarrier(
                        cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 0, nullptr, 1, &b3);

                    // srcImage ← обратно в PRESENT
                    VkImageMemoryBarrier b4 = b1;
                    b4.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    b4.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                    b4.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                    b4.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

                    device.GetContext().CmdPipelineBarrier(
                        cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &b4);

                    cmdList->EndCommandBuffer();

                    // Скачиваем
                    VkFenceCreateInfo fi{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
                    VkFence fence;
                    device.GetContext().CreateFence(vkDevice, &fi, VkSystemAllocator::Get(), &fence);

                    VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
                    VkCommandBuffer nativeCmd = cmd;
                    si.commandBufferCount = 1;
                    si.pCommandBuffers = &nativeCmd;

                    device.GetContext().QueueSubmit(vulkanQueue->GetNativeQueue(), 1, &si, fence);
                    device.GetContext().WaitForFences(vkDevice, 1, &fence, VK_TRUE, UINT64_MAX);
                    device.GetContext().DestroyFence(vkDevice, fence, VkSystemAllocator::Get());

                    // Мапим и пишем BMP
                    void* mapped = nullptr;
                    device.GetContext().MapMemory(vkDevice, dstMemory, 0, VK_WHOLE_SIZE, 0, &mapped);

                    VkImageSubresource sub{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
                    VkSubresourceLayout layout{};
                    device.GetContext().GetImageSubresourceLayout(vkDevice, dstImage, &sub, &layout);

                    uint8_t* pixelData = reinterpret_cast<uint8_t*>(mapped) + layout.offset;

                    AZStd::string path = AZStd::string::format("@user@/BMPs/swapchain_%i_%u.bmp", m_currentImage, imageIndex);

                    WriteBMP(path.c_str(), width, height, pixelData, layout.rowPitch);

                    device.GetContext().UnmapMemory(vkDevice, dstMemory);

                    device.GetContext().DestroyImage(vkDevice, dstImage, VkSystemAllocator::Get());
                    device.GetContext().FreeMemory(vkDevice, dstMemory, VkSystemAllocator::Get());
                    m_currentPresentIndexToSave--;

                    if (!m_currentPresentIndexToSave)
                    {
                        device.StopWriteCLasBMP();
                    }
                }
                // ---------------- END DEBUG SCREENSHOT ----------------

                VkPresentInfoKHR info{};
                info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                info.pNext = nullptr;
                info.waitSemaphoreCount = 1;
                info.pWaitSemaphores = &waitSemaphore;
                info.swapchainCount = 1;
                info.pSwapchains = &m_nativeSwapChain;
                info.pImageIndices = &imageIndex;
                info.pResults = nullptr;

#if defined(CARBONATED) && defined(AZ_PLATFORM_ANDROID) && defined(CARBONATED_DESIRED_FPS) && defined(CARBONATED_USE_SWAPPY)
                const VkResult result = SwappyVk_queuePresent(vulkanQueue->GetNativeQueue(), &info);
#else
                const VkResult result = device.GetContext().QueuePresentKHR(vulkanQueue->GetNativeQueue(), &info);
#endif // CARBONATED && AZ_PLATFORM_ANDROID && CARBONATED_DESIRED_FPS && CARBONATED_USE_SWAPPY

                // Vulkan's definition of the two types of errors.
                // VK_ERROR_OUT_OF_DATE_KHR: "A surface has changed in such a way that it is no longer compatible with the swapchain,
                //     and further presentation requests using the swapchain will fail. Applications must query the new surface
                //     properties and recreate their swapchain if they wish to continue presenting to the surface."
                // VK_SUBOPTIMAL_KHR: "A swapchain no longer matches the surface properties exactly, but can still be used to
                //     present to the surface successfully."
                //
                // These result values may occur after resizing or some window operation. We should update the surface info and recreate the swapchain.
                // VK_SUBOPTIMAL_KHR is treated as success, but on non-mobile platforms we better update the surface info as well.
                if (result == VK_ERROR_OUT_OF_DATE_KHR)
                {
                    m_pendingRecreation = true;
                }
                else if (result == VK_SUBOPTIMAL_KHR)
                {
                    // On mobile platforms the swapchain won't be recreated on VK_SUBOPTIMAL_KHR.
                    // This is because on mobiles VK_SUBOPTIMAL_KHR is returned when the swapchain's "preTransform"
                    // doesn't match the rotation of the device and that means its render engine internally will
                    // perform the rotation and on certain devices that's not as optimal as being handled by O3DE.
                    // Handling the rotation ourselves is not trivial to achieve, because the viewport dimensions have
                    // to be flipped (which affects UI operations) and view/projection matrices of 3D and 2D systems
                    // have to be manipulated in higher level code, which is very intrusive.
#if AZ_TRAIT_ATOM_VULKAN_RECREATE_SWAPCHAIN_WHEN_SUBOPTIMAL
                    m_pendingRecreation = true;
#endif
                }
                else
                {
                    // Other errors are:
                    // VK_ERROR_OUT_OF_HOST_MEMORY
                    // VK_ERROR_OUT_OF_DEVICE_MEMORY
                    // VK_ERROR_DEVICE_LOST
                    // VK_ERROR_SURFACE_LOST_KHR
                    // VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT
                    AZ_Assert(result == VK_SUCCESS, "Unhandled error for swapchain presentation.");
                }
            };

            uint32_t acquiredImageIndex = GetCurrentImageIndex();
            RHI::ResultCode result = AcquireNewImage(&acquiredImageIndex);
            if (result == RHI::ResultCode::Fail)
            {
                m_pendingRecreation = true;
                return 0;
            }
            else
            {
                m_presentationQueue->QueueCommand(AZStd::move(presentCommand));
                return acquiredImageIndex;
            }
        }

        RHI::ResultCode SwapChain::BuildSurface(const RHI::SwapChainDescriptor& descriptor)
        {
            WSISurface::Descriptor surfaceDesc{};
            surfaceDesc.m_windowHandle = descriptor.m_window;
            RHI::Ptr<WSISurface> surface = WSISurface::Create();
            const RHI::ResultCode result = surface->Init(surfaceDesc);
            RETURN_RESULT_IF_UNSUCCESSFUL(result);
            m_surface = surface;

            return result;
        }

        bool SwapChain::ValidateSurfaceDimensions(const RHI::SwapChainDimensions& dimensions)
        {
            return (m_surfaceCapabilities.minImageExtent.width <= dimensions.m_imageWidth &&
                dimensions.m_imageWidth <= m_surfaceCapabilities.maxImageExtent.width &&
                m_surfaceCapabilities.minImageExtent.height <= dimensions.m_imageHeight &&
                dimensions.m_imageHeight <= m_surfaceCapabilities.maxImageExtent.height);
        }

        VkSurfaceFormatKHR SwapChain::GetSupportedSurfaceFormat(const RHI::Format rhiFormat) const
        {
            AZ_Assert(m_surface, "Surface has not been initialized.");
            auto& device = static_cast<Device&>(GetDevice());
            const auto& physicalDevice = static_cast<const PhysicalDevice&>(device.GetPhysicalDevice());
            uint32_t surfaceFormatCount = 0;
            AssertSuccess(device.GetContext().GetPhysicalDeviceSurfaceFormatsKHR(
                physicalDevice.GetNativePhysicalDevice(), m_surface->GetNativeSurface(), &surfaceFormatCount, nullptr));
            AZ_Assert(surfaceFormatCount > 0, "Surface support no format.");
            AZStd::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
            AssertSuccess(device.GetContext().GetPhysicalDeviceSurfaceFormatsKHR(
                physicalDevice.GetNativePhysicalDevice(), m_surface->GetNativeSurface(), &surfaceFormatCount, surfaceFormats.data()));

            const VkFormat format = ConvertFormat(rhiFormat);
            for (uint32_t index = 0; index < surfaceFormatCount; ++index)
            {
                if (surfaceFormats[index].format == format)
                {
                    return surfaceFormats[index];
                }
            }
            AZ_Warning("Vulkan", false, "Given format is not supported, so it uses a supported format.");
            return surfaceFormats[0];
        }

        VkPresentModeKHR SwapChain::GetSupportedPresentMode(uint32_t verticalSyncInterval) const
        {
            AZ_Assert(m_surface, "Surface has not been initialized.");

            if (verticalSyncInterval > 0)
            {
                // When a non-zero vsync interval is requested, the FIFO presentation mode (always available)
                // is usable without needing to query available presentation modes.
                return VK_PRESENT_MODE_FIFO_KHR;
            }

            auto& device = static_cast<Device&>(GetDevice());
            const auto& physicalDevice = static_cast<const PhysicalDevice&>(device.GetPhysicalDevice());

            uint32_t modeCount = 0;
            AssertSuccess(device.GetContext().GetPhysicalDeviceSurfacePresentModesKHR(
                physicalDevice.GetNativePhysicalDevice(), m_surface->GetNativeSurface(), &modeCount, nullptr));
            // At least VK_PRESENT_MODE_FIFO_KHR have to be supported.
            // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPresentModeKHR.html
            AZ_Assert(modeCount > 0, "no available present mode.");
            AZStd::vector<VkPresentModeKHR> supportedModes(modeCount);
            AssertSuccess(device.GetContext().GetPhysicalDeviceSurfacePresentModesKHR(
                physicalDevice.GetNativePhysicalDevice(), m_surface->GetNativeSurface(), &modeCount, supportedModes.data()));

            VkPresentModeKHR preferredModes[] = {VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR};
            for (VkPresentModeKHR preferredMode : preferredModes)
            {
                for (VkPresentModeKHR supportedMode : supportedModes)
                {
                    if (supportedMode == preferredMode)
                    {
                        return supportedMode;
                    }
                }
            }
            return supportedModes[0];
        }

        VkSurfaceCapabilitiesKHR SwapChain::GetSurfaceCapabilities()
        {
            AZ_Assert(m_surface, "Surface has not been initialized.");

            auto& device = static_cast<Device&>(GetDevice());
            const auto& physicalDevice = static_cast<const PhysicalDevice&>(device.GetPhysicalDevice());

            VkSurfaceCapabilitiesKHR surfaceCapabilities;
            VkResult vkResult = device.GetContext().GetPhysicalDeviceSurfaceCapabilitiesKHR(
                physicalDevice.GetNativePhysicalDevice(), m_surface->GetNativeSurface(), &surfaceCapabilities);
            AssertSuccess(vkResult);

            return surfaceCapabilities;
        }

        VkCompositeAlphaFlagBitsKHR SwapChain::GetSupportedCompositeAlpha() const
        {
            VkFlags supportedModesBits = m_surfaceCapabilities.supportedCompositeAlpha;
            VkCompositeAlphaFlagBitsKHR preferedModes[] = {
                VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
                VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR };

            for (VkCompositeAlphaFlagBitsKHR mode : preferedModes)
            {
                if (supportedModesBits & mode)
                {
                    return mode;
                }
            }

            AZ_Assert(false, "Could not find a supported composite alpha mode for the swapchain");
            return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        }

        RHI::ResultCode SwapChain::BuildNativeSwapChain(const RHI::SwapChainDimensions& dimensions)
        {
            AZ_Assert(m_surface, "Surface is null.");

            if (!ValidateSurfaceDimensions(dimensions))
            {
                AZ_Assert(false, "Swapchain dimensions are not supported.");
                return RHI::ResultCode::InvalidArgument;
            }

            auto& device = static_cast<Vulkan::Device&>(GetDevice());
            auto& queueContext = device.GetCommandQueueContext();
            const VkExtent2D extent = { dimensions.m_imageWidth, dimensions.m_imageHeight };

            // If the graphic queue is the same as the presentation queue, then we will always acquire
            // 1 image at the same time. If it's another queue, we will have 2 at the same time (while the other queue
            // presents the image)
            auto graphicQueueId = queueContext.GetCommandQueue(RHI::HardwareQueueClass::Graphics).GetId();
            auto presentationQueueId = m_presentationQueue->GetId();
            AZStd::vector<uint32_t> familyIndices{ graphicQueueId.m_familyIndex };
            uint32_t simultaneousAcquiredImages = 1;
            if (graphicQueueId != presentationQueueId)
            {
                simultaneousAcquiredImages = 2;
                if (presentationQueueId.m_familyIndex != graphicQueueId.m_familyIndex)
                {
                    familyIndices.push_back(presentationQueueId.m_familyIndex);
                }
            }

            VkSwapchainCreateInfoKHR createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            createInfo.pNext = nullptr;
            createInfo.flags = 0; // [GFX TODO][ATOM-512] find appropriate flags
            createInfo.surface = m_surface->GetNativeSurface();
            // When acquiring an image the number of images that the application has currently acquired (but not yet presented)
            // need to be less than or equal to the difference between the number of images in swapchain and the value of VkSurfaceCapabilitiesKHR::minImageCount
            createInfo.minImageCount = AZStd::max(dimensions.m_imageCount, simultaneousAcquiredImages + m_surfaceCapabilities.minImageCount);
            createInfo.imageFormat = m_surfaceFormat.format;
            createInfo.imageColorSpace = m_surfaceFormat.colorSpace;
            createInfo.imageExtent = extent;
            createInfo.imageArrayLayers = 1; // non-stereoscopic
            createInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            createInfo.imageUsage = RHI::FilterBits(createInfo.imageUsage, m_surfaceCapabilities.supportedUsageFlags);
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = aznumeric_cast<uint32_t>(familyIndices.size());
            createInfo.pQueueFamilyIndices = familyIndices.empty() ? nullptr : familyIndices.data();
            createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
            createInfo.compositeAlpha = m_compositeAlphaFlagBits;
            createInfo.presentMode = m_presentMode;
            createInfo.clipped = VK_FALSE;
            // Pass the current swapchain as the old one
            createInfo.oldSwapchain = m_nativeSwapChain;

            const VkResult result =
                device.GetContext().CreateSwapchainKHR(device.GetNativeDevice(), &createInfo, VkSystemAllocator::Get(), &m_nativeSwapChain);
            AssertSuccess(result);

            return ConvertResult(result);
        }

        RHI::ResultCode SwapChain::AcquireNewImage(uint32_t* acquiredImageIndex)
        {
            auto& device = static_cast<Device&>(GetDevice());
            auto& semaphoreAllocator = device.GetSwapChainSemaphoreAllocator();
            Semaphore* imageAvailableSemaphore = semaphoreAllocator.Allocate();
            VkResult vkResult = device.GetContext().AcquireNextImageKHR(
                device.GetNativeDevice(),
                m_nativeSwapChain,
                UINT64_MAX,
                imageAvailableSemaphore->GetNativeSemaphore(),
                VK_NULL_HANDLE,
                acquiredImageIndex);

            RHI::ResultCode result = ConvertResult(vkResult);
            RETURN_RESULT_IF_UNSUCCESSFUL(result);

            imageAvailableSemaphore->SignalEvent();
            if (m_currentFrameContext.m_imageAvailableSemaphore)
            {
                semaphoreAllocator.DeAllocate(m_currentFrameContext.m_imageAvailableSemaphore);
            }
            if (m_currentFrameContext.m_presentableSemaphore)
            {
                semaphoreAllocator.DeAllocate(m_currentFrameContext.m_presentableSemaphore);
            }
            m_currentFrameContext.m_imageAvailableSemaphore = imageAvailableSemaphore;
            m_currentFrameContext.m_presentableSemaphore = semaphoreAllocator.Allocate();

            return result;
        }

        void SwapChain::InvalidateSurface()
        {
            m_surface = nullptr;
        }

        void SwapChain::InvalidateNativeSwapChain(VkSwapchainKHR swapchain)
        {
            auto& device = static_cast<Device&>(GetDevice());
            auto presentCommand = [&device, swapchain]([[maybe_unused]] void* queue)
            {
                device.GetContext().DeviceWaitIdle(device.GetNativeDevice());
                if (swapchain != VK_NULL_HANDLE)
                {
#if defined(CARBONATED) && defined(AZ_PLATFORM_ANDROID) && defined(CARBONATED_DESIRED_FPS) && defined(CARBONATED_USE_SWAPPY)
                    SwappyVk_destroySwapchain(device.GetNativeDevice(), swapchain);
#endif // CARBONATED && AZ_PLATFORM_ANDROID && CARBONATED_DESIRED_FPS && CARBONATED_USE_SWAPPY
                    device.GetContext().DestroySwapchainKHR(device.GetNativeDevice(), swapchain, VkSystemAllocator::Get());
                }
            };

            m_presentationQueue->QueueCommand(AZStd::move(presentCommand));
            m_presentationQueue->FlushCommands();
        }

        RHI::ResultCode SwapChain::CreateSwapchain()
        {
            auto& device = static_cast<Device&>(GetDevice());

            m_surfaceCapabilities = GetSurfaceCapabilities();
            m_surfaceFormat = GetSupportedSurfaceFormat(m_dimensions.m_imageFormat);
            m_presentMode = GetSupportedPresentMode(GetDescriptor().m_verticalSyncInterval);
            m_compositeAlphaFlagBits = GetSupportedCompositeAlpha();

            if (!ValidateSurfaceDimensions(m_dimensions))
            {
                [[maybe_unused]] uint32_t oldHeight = m_dimensions.m_imageHeight;
                [[maybe_unused]] uint32_t oldWidth = m_dimensions.m_imageWidth;
                m_dimensions.m_imageHeight = AZStd::clamp(
                    m_dimensions.m_imageHeight,
                    m_surfaceCapabilities.minImageExtent.height,
                    m_surfaceCapabilities.maxImageExtent.height);
                m_dimensions.m_imageWidth = AZStd::clamp(
                    m_dimensions.m_imageWidth,
                    m_surfaceCapabilities.minImageExtent.width,
                    m_surfaceCapabilities.maxImageExtent.width);
                AZ_Info("Swapchain", "Resizing swapchain from (%u, %u) to (%u, %u).",
                    oldWidth, oldHeight, m_dimensions.m_imageWidth, m_dimensions.m_imageHeight);
            }

            RHI::ResultCode result = BuildNativeSwapChain(m_dimensions);
            RETURN_RESULT_IF_UNSUCCESSFUL(result);
            AZ_Info("Swapchain", "Swapchain created. Width: %u, Height: %u.\n", m_dimensions.m_imageWidth, m_dimensions.m_imageHeight);

            // Do not recycle the semaphore because they may not ever get signaled and since
            // we can't recycle Vulkan semaphores we just delete them.
            if (m_currentFrameContext.m_imageAvailableSemaphore)
            {
                m_currentFrameContext.m_imageAvailableSemaphore->SetRecycleValue(false);
            }
            if (m_currentFrameContext.m_presentableSemaphore)
            {
                m_currentFrameContext.m_presentableSemaphore->SetRecycleValue(false);
            }

            m_dimensions.m_imageCount = 0;
            VkResult vkResult =
                device.GetContext().GetSwapchainImagesKHR(device.GetNativeDevice(), m_nativeSwapChain, &m_dimensions.m_imageCount, nullptr);
            AssertSuccess(vkResult);
            RETURN_RESULT_IF_UNSUCCESSFUL(ConvertResult(vkResult));

            m_swapchainNativeImages.resize(m_dimensions.m_imageCount);

            // Retrieve the native images of the swapchain so they are
            // available when we init the images in InitImageInternal
            vkResult = device.GetContext().GetSwapchainImagesKHR(
                device.GetNativeDevice(), m_nativeSwapChain, &m_dimensions.m_imageCount, m_swapchainNativeImages.data());
            AssertSuccess(vkResult);
            RETURN_RESULT_IF_UNSUCCESSFUL(ConvertResult(vkResult));
            AZLOG_DEBUG("Obtained presentable images.\n");

            // Acquire the first image
            uint32_t imageIndex = 0;
            result = AcquireNewImage(&imageIndex);
            RETURN_RESULT_IF_UNSUCCESSFUL(result);
            AZLOG_DEBUG("Acquired the first image.\n");

#if defined(CARBONATED) && defined(AZ_PLATFORM_ANDROID) && defined(CARBONATED_DESIRED_FPS) && defined(CARBONATED_USE_SWAPPY)
            auto presentCommand = [this, &device](void* queue)
            {
                Queue* vulkanQueue = static_cast<Queue*>(queue);

                if (auto* androidEnv = AZ::Android::AndroidEnv::Get())
                {
                    JNIEnv* env = androidEnv->GetJniEnv();
                    jobject activity = androidEnv->GetActivityRef();

                    const AZ::Vulkan::PhysicalDevice& vulkanPhysicalDevice = static_cast<const PhysicalDevice&>(device.GetPhysicalDevice());

                    // Initializing Swappy for the current VkSwapchainKHR and get m_refreshNs
                    SwappyVk_initAndGetRefreshCycleDuration(
                        env,
                        activity,
                        vulkanPhysicalDevice.GetNativePhysicalDevice(),
                        device.GetNativeDevice(),
                        m_nativeSwapChain,
                        &m_refreshNs);

                    // Inform Swappy the window and presentation queue
                    SwappyVk_setWindow(device.GetNativeDevice(), m_nativeSwapChain, androidEnv->GetWindow());

                    SwappyVk_setQueueFamilyIndex(
                        device.GetNativeDevice(), vulkanQueue->GetNativeQueue(), m_presentationQueue->GetId().m_familyIndex);
                }
            };
            m_presentationQueue->QueueCommand(AZStd::move(presentCommand));
            m_presentationQueue->FlushCommands();
#endif // CARBONATED && AZ_PLATFORM_ANDROID && CARBONATED_DESIRED_FPS && CARBONATED_USE_SWAPPY
            return RHI::ResultCode::Success;
        }
    }
}
