#if defined(CARBONATED) && !defined(_RELEASE)

#include "VulkanBmpWriter.h"

#include <AzFramework/IO/LocalFileIO.h>
#include <AzCore/std/containers/vector.h>
#include <cmath>
#include <algorithm>

namespace AZ
{
    namespace Vulkan
    {
        //-------------------------------------------------------------------------
        // Internal helpers
        //-------------------------------------------------------------------------

        uint16_t VulkanBmpWriter::ReadU16(const uint8_t* ptr)
        {
            return ptr[0] | (ptr[1] << 8);
        }

        int16_t VulkanBmpWriter::ReadS16(const uint8_t* ptr)
        {
            return static_cast<int16_t>(ptr[0] | (ptr[1] << 8));
        }

        float VulkanBmpWriter::HalfToFloat(uint16_t h)
        {
            const uint16_t h_exp = (h & 0x7C00u) >> 10;
            const uint16_t h_sig = (h & 0x03FFu);
            const uint16_t h_sign = (h & 0x8000u);

            uint32_t f_sign = uint32_t(h_sign) << 16;

            if (h_exp == 0)
            {
                if (h_sig == 0)
                {
                    return reinterpret_cast<float&>(f_sign);
                }

                float mant = float(h_sig) / 1024.0f;
                float val = std::ldexp(mant, -14);
                return h_sign ? -val : val;
            }
            else if (h_exp == 31)
            {
                uint32_t f = f_sign | 0x7F800000u | (h_sig << 13);
                return reinterpret_cast<float&>(f);
            }
            else
            {
                const uint32_t f_exp = (h_exp + 112) << 23;
                const uint32_t f_sig = uint32_t(h_sig) << 13;
                uint32_t f = f_sign | f_exp | f_sig;
                return reinterpret_cast<float&>(f);
            }
        }

        uint8_t VulkanBmpWriter::LinearToSRGB8(float x)
        {
            x = std::max(0.0f, std::min(1.0f, x));

            if (x <= 0.0031308f)
            {
                x = 12.92f * x;
            }
            else
            {
                x = 1.055f * std::pow(x, 1.f / 2.4f) - 0.055f;
            }

            return static_cast<uint8_t>(x * 255.0f + 0.5f);
        }

        //-------------------------------------------------------------------------
        // Converters to BGRA8
        //-------------------------------------------------------------------------

        void VulkanBmpWriter::ConvertR16G16B16A16ToBGRA8(uint8_t* dst, const uint8_t* src, uint32_t width)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                const uint16_t hR = ReadU16(src + x * 8 + 0);
                const uint16_t hG = ReadU16(src + x * 8 + 2);
                const uint16_t hB = ReadU16(src + x * 8 + 4);
                const uint16_t hA = ReadU16(src + x * 8 + 6);

                const float R = HalfToFloat(hR);
                const float G = HalfToFloat(hG);
                const float B = HalfToFloat(hB);
                const float A = HalfToFloat(hA);

                dst[x * 4 + 0] = LinearToSRGB8(B);
                dst[x * 4 + 1] = LinearToSRGB8(G);
                dst[x * 4 + 2] = LinearToSRGB8(R);
                dst[x * 4 + 3] = static_cast<uint8_t>(std::clamp(A, 0.f, 1.f) * 255);
            }
        }

        void VulkanBmpWriter::ConvertR16G16SnormToBGRA8(uint8_t* dst, const uint8_t* src, uint32_t width)
        {
            auto ToSnorm = [](int16_t v) -> float
            {
                if (v <= -32768)
                {
                    return -1.0f;
                }
                return float(v) / 32767.0f;
            };

            for (uint32_t x = 0; x < width; ++x)
            {
                const int16_t r16 = ReadS16(src + x * 4 + 0);
                const int16_t g16 = ReadS16(src + x * 4 + 2);

                const float r = ToSnorm(r16);
                const float g = ToSnorm(g16);

                float gray = (r + g) * 0.25f + 0.5f; // (-1..1) -> (0..1)
                gray = std::clamp(gray, 0.0f, 1.0f);
                uint8_t v = static_cast<uint8_t>(gray * 255.0f + 0.5f);

                dst[x * 4 + 0] = v;
                dst[x * 4 + 1] = v;
                dst[x * 4 + 2] = v;
                dst[x * 4 + 3] = 255;
            }
        }

        void VulkanBmpWriter::ConvertD32FloatToBGRA8(uint8_t* dst, const uint8_t* src, uint32_t width)
        {
            const float* depth = reinterpret_cast<const float*>(src);

            for (uint32_t x = 0; x < width; ++x)
            {
                const float d = std::clamp(depth[x], 0.f, 1.f);
                const uint8_t v = static_cast<uint8_t>(d * 255.f + 0.5f);

                dst[x * 4 + 0] = v;
                dst[x * 4 + 1] = v;
                dst[x * 4 + 2] = v;
                dst[x * 4 + 3] = 255;
            }
        }

        void VulkanBmpWriter::FillSolidRedBGRA8(uint8_t* dst, uint32_t width)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                dst[x * 4 + 0] = 0;
                dst[x * 4 + 1] = 0;
                dst[x * 4 + 2] = 255;
                dst[x * 4 + 3] = 255;
            }
        }

        //-------------------------------------------------------------------------
        // Pipeline name cleanup
        //-------------------------------------------------------------------------

        AZStd::string VulkanBmpWriter::StripPipelinePrefix(const AZStd::string& name)
        {
            constexpr const char* Prefix = "Root.MobilePipeline_-10.";
            if (name.starts_with(Prefix))
            {
                return name.substr(strlen(Prefix));
            }
            return name;
        }

        //-------------------------------------------------------------------------
        // WriteBMP (single unified version)
        //-------------------------------------------------------------------------

        bool VulkanBmpWriter::WriteBMP(const char* filePath, uint32_t width, uint32_t height, const uint8_t* rgbaData, size_t rowPitch)
        {
            const uint32_t bytesPerPixel = 4;
            const uint32_t tightRowSize = width * bytesPerPixel;
            const uint32_t dataSize = tightRowSize * height;

#pragma pack(push, 1)
            struct BMPHeader
            {
                uint16_t bfType;
                uint32_t bfSize;
                uint16_t bfReserved1;
                uint16_t bfReserved2;
                uint32_t bfOffBits;
            };

            struct BMPInfoHeader
            {
                uint32_t biSize;
                int32_t biWidth;
                int32_t biHeight;
                uint16_t biPlanes;
                uint16_t biBitCount;
                uint32_t biCompression;
                uint32_t biSizeImage;
                int32_t biXPelsPerMeter;
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
            info.biHeight = -static_cast<int32_t>(height); // top-down BMP
            info.biPlanes = 1;
            info.biBitCount = 32;
            info.biCompression = 0;
            info.biSizeImage = dataSize;
            info.biXPelsPerMeter = 2835;
            info.biYPelsPerMeter = 2835;

            AZStd::vector<uint8_t> tight(dataSize);

            // Convert RGBA -> BGRA writing order used by BMP
            for (uint32_t y = 0; y < height; ++y)
            {
                const uint8_t* src = rgbaData + y * rowPitch;
                uint8_t* dst = tight.data() + y * tightRowSize;

                for (uint32_t x = 0; x < width; ++x)
                {
                    const uint8_t R = src[x * 4 + 0];
                    const uint8_t G = src[x * 4 + 1];
                    const uint8_t B = src[x * 4 + 2];

                    dst[x * 4 + 0] = B;
                    dst[x * 4 + 1] = G;
                    dst[x * 4 + 2] = R;
                    dst[x * 4 + 3] = 255;
                }
            }

            AZ::IO::HandleType handle;
            if (!AZ::IO::LocalFileIO::GetInstance()->Open(filePath, AZ::IO::OpenMode::ModeWrite | AZ::IO::OpenMode::ModeBinary, handle))
            {
                return false;
            }

            AZ::u64 written = 0;
            auto* fileIO = AZ::IO::LocalFileIO::GetInstance();
            fileIO->Write(handle, &header, sizeof(header), &written);
            fileIO->Write(handle, &info, sizeof(info), &written);
            fileIO->Write(handle, tight.data(), dataSize, &written);
            fileIO->Close(handle);

            return true;
        }

        //-------------------------------------------------------------------------
        // WriteAnyFormatBMP
        //-------------------------------------------------------------------------

        bool VulkanBmpWriter::WriteAnyFormatBMP(
            const char* filePath, uint32_t width, uint32_t height, const uint8_t* srcData, size_t rowPitch, VkFormat format)
        {
            const uint32_t bpp = 4;
            const uint32_t tightRowSize = width * bpp;
            AZStd::vector<uint8_t> tight(tightRowSize * height);

            for (uint32_t y = 0; y < height; ++y)
            {
                const uint8_t* src = srcData + rowPitch * y;
                uint8_t* dst = tight.data() + tightRowSize * y;

                switch (format)
                {
                case VK_FORMAT_R8G8B8A8_UNORM:
                case VK_FORMAT_B8G8R8A8_UNORM:
                    memcpy(dst, src, tightRowSize);
                    break;

                case VK_FORMAT_R16G16B16A16_SFLOAT:
                    ConvertR16G16B16A16ToBGRA8(dst, src, width);
                    break;

                case VK_FORMAT_R16G16_SNORM:
                    ConvertR16G16SnormToBGRA8(dst, src, width);
                    break;

                case VK_FORMAT_D32_SFLOAT:
                    ConvertD32FloatToBGRA8(dst, src, width);
                    break;

                default:
                    for (uint32_t y2 = 0; y2 < height; ++y2)
                    {
                        FillSolidRedBGRA8(tight.data() + tightRowSize * y2, width);
                    }

                    WriteBMP(filePath, width, height, tight.data(), tightRowSize);
                    AZ_Warning("BMP", false, "Unsupported VkFormat %u – writing solid red BMP", format);
                    return true;
                }
            }

            return WriteBMP(filePath, width, height, tight.data(), tightRowSize);
        }
    } // namespace Vulkan
} // namespace AZ

#endif // CARBONATED && !_RELEASE
