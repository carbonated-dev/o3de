/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#if defined(CARBONATED) && defined(CARBONATED_LUT_TEXTURE)

#include <AzCore/Debug/Trace.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/string/fixed_string.h>
#include <AzCore/std/containers/fixed_vector.h>
#include <AzCore/std/string/conversions.h>
#include <AzCore/StringFunc/StringFunc.h>

#include <ImageLoader/ImageLoaders.h>
#include <Atom/ImageProcessing/ImageObject.h>
#include <Processing/PixelFormatInfo.h>

#include <QString>

namespace ImageProcessingAtom
{
    namespace CUBELoader
    {

        bool IsExtensionSupported(const char* extension)
        {
            const QString ext = QString(extension).toLower();
            return ext == "cube";
        }

        static uint16_t ConvertFloatToHalf(const float value)
        {
            uint32_t result = 0;

            uint32_t iValue = ((uint32_t*)(&value))[0];
            const uint32_t sign = (iValue & 0x80000000U) >> 16U;

            iValue = iValue & 0x7FFFFFFFU;

            if (iValue > 0x47FFEFFFU)
            {
                result = 0x7FFFU;
            }
            else
            {
                if (iValue < 0x38800000U)
                {
                    const uint32_t shift = 113U - (iValue >> 23U);
                    iValue = (0x800000U | (iValue & 0x7FFFFFU)) >> shift;
                }
                else
                {
                    iValue += 0xC8000000U;
                }

                result = ((iValue + 0x0FFFU + ((iValue >> 13U) & 1U)) >> 13U) & 0x7FFFU;
            }
            return static_cast<uint16_t>(result | sign);
        }

        IImageObject* LoadImageFromCube(const AZStd::string& filename)
        {
            AZ::IO::SystemFileStream cubeStream(filename.c_str(), AZ::IO::OpenMode::ModeRead);
            if (!cubeStream.IsOpen())
            {
                AZ_Warning("Image Processing", false, "%s: failed to open file %s", __FUNCTION__, filename.c_str());
                return nullptr;
            }

            size_t fileLength = cubeStream.GetLength();
            AZ_Error("ImageProcessing", fileLength > 0, "The %s is empty...", filename.c_str());

            typedef AZStd::vector<char> BufferType;
            BufferType buffer;
            buffer.resize(fileLength);

            size_t bytesRead = 0;
            while (fileLength > 0)
            {
                bytesRead = cubeStream.Read(fileLength, buffer.data() + bytesRead);
                fileLength -= bytesRead;
            }

            AZStd::string title;
            AZStd::optional<AZStd::array<float, 3>> domainMin;
            AZStd::optional<AZStd::array<float, 3>> domainMax;
            AZ::u32 lutSize = 0;
            AZStd::vector<AZ::Vector3> lutData;

            int lutColorCounter = 0;

            BufferType::iterator itr;
            for (itr = buffer.begin(); itr != buffer.end();)
            {
                if (AZStd::isspace(*itr, {}))
                {
                    ++itr;
                    continue;
                }
                auto lineStartIter = itr;
                auto lineEndIter = AZStd::find(itr, buffer.end(), '\n');
                if (lineEndIter != buffer.end())
                {
                    AZStd::string line{ lineStartIter, lineEndIter };
                    line = AZ::StringFunc::TrimWhiteSpace(line, true, true);
                    if (AZ::StringFunc::FirstCharacter(line.c_str()) != '#')
                    {
                        if (AZ::StringFunc::StartsWith(line, "title", false))
                        {
                            const size_t pos = line.find('"');
                            if (pos != AZStd::string::npos)
                            {
                                const size_t end_pos = line.find('"', pos + 1);
                                if (end_pos != AZStd::string::npos)
                                {
                                    title = line.substr(pos + 1, end_pos - pos - 1);
                                }
                            }
                        }
                        else if (AZ::StringFunc::StartsWith(line, "lut_3d_size", false))
                        {
                            AZStd::vector<AZStd::string> tokens;
                            AZ::StringFunc::Tokenize(line, tokens, " ");
                            if (tokens.size() >= 2)
                            {
                                lutSize = AZ::StringFunc::ToInt(tokens[1].c_str());
                                lutData.resize(lutSize * lutSize * lutSize);
                            }
                        }
                        else if (AZ::StringFunc::StartsWith(line, "domain_min", false))
                        {
                            AZStd::vector<AZStd::string> tokens;
                            AZ::StringFunc::Tokenize(line, tokens, " ");
                            if (tokens.size() >= 4)
                            {
                                domainMin = AZStd::array<float, 3>{ AZ::StringFunc::ToFloat(tokens[1].c_str()),
                                                                    AZ::StringFunc::ToFloat(tokens[2].c_str()),
                                                                    AZ::StringFunc::ToFloat(tokens[3].c_str()) };
                            }
                        }
                        else if (AZ::StringFunc::StartsWith(line, "domain_max", false))
                        {
                            AZStd::vector<AZStd::string> tokens;
                            AZ::StringFunc::Tokenize(line, tokens, " ");
                            if (tokens.size() >= 4)
                            {
                                domainMax = AZStd::array<float, 3>{ AZ::StringFunc::ToFloat(tokens[1].c_str()),
                                                                    AZ::StringFunc::ToFloat(tokens[2].c_str()),
                                                                    AZ::StringFunc::ToFloat(tokens[3].c_str()) };
                            }
                        }
                        else
                        {
                            AZStd::vector<AZStd::string> tokens;
                            AZ::StringFunc::Tokenize(line, tokens, " ");
                            if (tokens.size() >= 3)
                            {
                                float r = AZ::StringFunc::ToFloat(tokens[0].c_str());
                                float g = AZ::StringFunc::ToFloat(tokens[1].c_str());
                                float b = AZ::StringFunc::ToFloat(tokens[2].c_str());

                                if (domainMin && domainMax)
                                {
                                    const auto& min = *domainMin;
                                    const auto& max = *domainMax;
                                    r = (r - min[0]) / (max[0] - min[0]);
                                    g = (g - min[1]) / (max[1] - min[1]);
                                    b = (b - min[2]) / (max[2] - min[2]);
                                }

                                AZ_Assert(lutSize && !lutData.empty(), "The lutData array is empty: %d %d", lutSize, (int)lutData.empty());

                                const auto rIdx = lutColorCounter % lutSize;
                                const auto gIdx = (lutColorCounter / lutSize) % lutSize;
                                const auto bIdx = lutColorCounter / (lutSize * lutSize);
                                const auto idx = (bIdx * lutSize + gIdx) * lutSize + rIdx;
                                lutData[idx] = { r, g, b };

                                ++lutColorCounter;
                            }
                        }
                    }
                    itr = lineEndIter + 1;
                }
                else
                {
                    itr = buffer.end();
                }
            }
            cubeStream.Close();

            const EPixelFormat imagePixelFormat = ePixelFormat_R16G16B16A16F;
            IImageObject* pImage = IImageObject::CreateImage(lutSize, lutSize, lutSize, 1, imagePixelFormat);

            uint32_t dwPitch = 0;
            uint8_t* texBuffer = nullptr;
            pImage->GetImagePointer(0, texBuffer, dwPitch);

            uint16_t* texData = reinterpret_cast<uint16_t*>(texBuffer);

            for (int i = 0; i < lutData.size(); ++i)
            {
                const auto& color = lutData[i];
                texData[4 * i + 0] = ConvertFloatToHalf(color.GetX());
                texData[4 * i + 1] = ConvertFloatToHalf(color.GetY());
                texData[4 * i + 2] = ConvertFloatToHalf(color.GetZ());
                texData[4 * i + 3] = ConvertFloatToHalf(1.0f);
            }

            return pImage;
        }

    } // namespace CUBELoader
} // namespace ImageProcessingAtom

#endif

/* Example of creation Cube file
static void CreateIdentityLUT(const AZStd::string& filename, int lutSize)
{
    AZ::IO::SystemFileStream cubeStream(filename.c_str(), AZ::IO::OpenMode::ModeWrite);
    if (!cubeStream.IsOpen())
    {
        AZ_Warning("Image Processing", false, "%s: failed to open file %s", __FUNCTION__, filename.c_str());
        return;
    }

    const AZStd::string_view desc{ "#Identity LUT. RGB to RGB identity transform (no color transformation)\n" };
    const AZStd::string_view title{ "TITLE \"Identity LUT - RGB to RGB\"\n" };
    const AZStd::string lut3DSize = AZStd::string::format("LUT_3D_SIZE %d\n", lutSize);
    const AZStd::string_view startRGB{ "\n#R G B\n" };

    cubeStream.Write(desc.length(), desc.data());
    cubeStream.Write(title.length(), title.data());
    cubeStream.Write(lut3DSize.length(), lut3DSize.c_str());
    cubeStream.Write(startRGB.length(), startRGB.data());

    const float ooLutSize = 1.0f / (lutSize - 1);
    for (int b = 0; b < lutSize; ++b)
    {
        for (int g = 0; g < lutSize; ++g)
        {
            for (int r = 0; r < lutSize; ++r)
            {
                const AZStd::string rgbData = AZStd::string::format("%.4f %.4f %.4f\n", r * ooLutSize, g * ooLutSize, b * ooLutSize);
                cubeStream.Write(rgbData.length(), rgbData.c_str());
            }
        }
    }
    cubeStream.Close();
}*/
