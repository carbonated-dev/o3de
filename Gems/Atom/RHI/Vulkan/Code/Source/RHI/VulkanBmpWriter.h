/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#if defined(CARBONATED) && !defined(_RELEASE)

#include <cstdint>
#include <AzCore/std/string/string.h>
#include <RHI/Device.h>

namespace AZ
{
    namespace Vulkan
    {
        //! Utility class for writing Vulkan images into BMP files
        //! Supports multiple Vulkan formats and converts them to BGRA8.
        class VulkanBmpWriter
        {
        public:
            //! Writes raw RGBA (not BGRA!) image buffer into BMP file.
            //! Used by SwapChain screenshot and WriteAnyFormatBMP().
            static bool WriteBMP(const char* filePath, uint32_t width, uint32_t height, const uint8_t* rgbaData, size_t rowPitch);

            //! Converts any of the supported Vulkan formats into BGRA8
            //! and writes them to BMP using WriteBMP().
            static bool WriteAnyFormatBMP(
                const char* filePath, uint32_t width, uint32_t height, const uint8_t* srcData, size_t rowPitch, VkFormat format);

            //! Used by CommandList to remove long pipeline prefixes
            //! from debug capture filenames.
            static AZStd::string StripPipelinePrefix(const AZStd::string& name);

        private:
            static uint16_t ReadU16(const uint8_t* ptr);
            static int16_t ReadS16(const uint8_t* ptr);
            static float HalfToFloat(uint16_t h);
            static uint8_t LinearToSRGB8(float x);

            static void ConvertR16G16B16A16ToBGRA8(uint8_t* dstRow, const uint8_t* srcRow, uint32_t width);
            static void ConvertR16G16SnormToBGRA8(uint8_t* dstRow, const uint8_t* srcRow, uint32_t width);
            static void ConvertD32FloatToBGRA8(uint8_t* dstRow, const uint8_t* srcRow, uint32_t width);

            static void FillSolidRedBGRA8(uint8_t* dstRow, uint32_t width);
        };
    } // namespace Vulkan
} // namespace AZ

#endif // CARBONATED && !_RELEASE
