/* <editor-fold desc="MIT License">

Copyright(c) 2021 André Normann & Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgXchange/images.h>

#include <vsg/io/FileSystem.h>
#include <vsg/io/ObjectCache.h>

#include <cstring>

#if defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wunused-variable"
#    pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#    pragma GCC diagnostic ignored "-Wsign-compare"
#    pragma GCC diagnostic ignored "-Wunused-function"
#endif

#define TINYDDSLOADER_IMPLEMENTATION
#include "tinyddsloader.h"

#if defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif

namespace
{
    const std::unordered_map<tinyddsloader::DDSFile::DXGIFormat, VkFormat> kFormatMap{
        {tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_UNorm, VK_FORMAT_R8G8B8A8_UNORM},
        {tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_SNorm, VK_FORMAT_R8G8B8A8_SNORM},
        {tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_UNorm_SRGB, VK_FORMAT_R8G8B8A8_SRGB},
        {tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm, VK_FORMAT_BC7_UNORM_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm_SRGB, VK_FORMAT_BC7_SRGB_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC5_UNorm, VK_FORMAT_BC5_UNORM_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC5_SNorm, VK_FORMAT_BC5_SNORM_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC4_UNorm, VK_FORMAT_BC4_UNORM_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC4_SNorm, VK_FORMAT_BC4_SNORM_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm, VK_FORMAT_BC3_UNORM_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm_SRGB, VK_FORMAT_BC3_SRGB_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC2_UNorm, VK_FORMAT_BC2_UNORM_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC2_UNorm_SRGB, VK_FORMAT_BC2_SRGB_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm, VK_FORMAT_BC1_RGBA_UNORM_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm_SRGB, VK_FORMAT_BC1_RGBA_SRGB_BLOCK}};

    uint8_t* allocateAndCopyToContiguousBlock(tinyddsloader::DDSFile& ddsFile)
    {
        const auto numMipMaps = ddsFile.GetMipCount();
        const auto numArrays = ddsFile.GetArraySize();
        size_t totalSize = 0;
        for (uint32_t i = 0; i < numMipMaps; ++i)
        {
            for (uint32_t j = 0; j < numArrays; ++j)
            {
                const auto data = ddsFile.GetImageData(i, j);
                totalSize += data->m_memSlicePitch;
            }
        }

        if (totalSize == 0) return nullptr;

        auto raw = new uint8_t[totalSize];

        uint8_t* image_ptr = raw;
        for (uint32_t i = 0; i < numMipMaps; ++i)
        {
            for (uint32_t j = 0; j < numArrays; ++j)
            {
                const auto data = ddsFile.GetImageData(i, j);

                std::memcpy(image_ptr, data->m_mem, data->m_memSlicePitch);

                image_ptr += data->m_memSlicePitch;
            }
        }
        return raw;
    }

    int computeImageViewType(tinyddsloader::DDSFile& ddsFile)
    {
        switch (ddsFile.GetTextureDimension())
        {
        case tinyddsloader::DDSFile::TextureDimension::Texture1D: return VK_IMAGE_VIEW_TYPE_1D;
        case tinyddsloader::DDSFile::TextureDimension::Texture2D:
            if (ddsFile.GetArraySize() > 1)
            {
                return ddsFile.IsCubemap() ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            }
            else
            {
                return VK_IMAGE_VIEW_TYPE_2D;
            }
        case tinyddsloader::DDSFile::TextureDimension::Texture3D: return VK_IMAGE_VIEW_TYPE_3D;
        case tinyddsloader::DDSFile::TextureDimension::Unknown: return -1;
        }
        return -1;
    }

    vsg::ref_ptr<vsg::Data> readCompressed(tinyddsloader::DDSFile& ddsFile, VkFormat targetFormat)
    {
        const auto width = ddsFile.GetWidth();
        const auto height = ddsFile.GetHeight();
        const auto numMipMaps = ddsFile.GetMipCount();
        const auto numArrays = ddsFile.GetArraySize();

        auto raw = allocateAndCopyToContiguousBlock(ddsFile);

        vsg::ref_ptr<vsg::Data> vsg_data;

        vsg::Data::Layout layout;
        layout.format = targetFormat;
        layout.maxNumMipmaps = numMipMaps;
        layout.blockWidth = 4;
        layout.blockHeight = 4;
        layout.imageViewType = computeImageViewType(ddsFile);

        switch (targetFormat)
        {
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        case VK_FORMAT_BC4_SNORM_BLOCK:
        case VK_FORMAT_BC4_UNORM_BLOCK:
            if (numArrays > 1)
                vsg_data = vsg::block64Array3D::create(width / layout.blockWidth, height / layout.blockHeight, numArrays, reinterpret_cast<vsg::block64*>(raw), layout);
            else
                vsg_data = vsg::block64Array2D::create(width / layout.blockWidth, height / layout.blockHeight, reinterpret_cast<vsg::block64*>(raw), layout);
            break;
        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC5_UNORM_BLOCK:
        case VK_FORMAT_BC5_SNORM_BLOCK:
        case VK_FORMAT_BC7_UNORM_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:
            if (numArrays > 1)
                vsg_data = vsg::block128Array3D::create(width / layout.blockWidth, height / layout.blockHeight, numArrays, reinterpret_cast<vsg::block128*>(raw), layout);
            else
                vsg_data = vsg::block128Array2D::create(width / layout.blockWidth, height / layout.blockHeight, reinterpret_cast<vsg::block128*>(raw), layout);
            break;
        default:
            std::cerr << "dds::readCompressed() Format is not supported yet: " << (uint32_t)targetFormat << std::endl;
            break;
        }

        return vsg_data;
    }

    vsg::ref_ptr<vsg::Data> readDds(tinyddsloader::DDSFile& ddsFile)
    {
        const auto width = ddsFile.GetWidth();
        const auto height = ddsFile.GetHeight();
        const auto depth = ddsFile.GetDepth();
        const auto numMipMaps = ddsFile.GetMipCount();
        const auto format = ddsFile.GetFormat();
        const auto isCompressed = ddsFile.IsCompressed(format);
        const auto dim = ddsFile.GetTextureDimension();
        const auto numArrays = ddsFile.GetArraySize();

        // const auto valueCount = vsg::Data::computeValueCountIncludingMipmaps(width, height, depth, numMipMaps) * numArrays;
        // const auto bpp = tinyddsloader::DDSFile::GetBitsPerPixel(format);
        //std::cerr << "Fileinfo width: " << width << ", height: " << height << ", depth: " << depth << ", count: " << valueCount
        //          << ", format: " << (int)format << ", bpp: " << tinyddsloader::DDSFile::GetBitsPerPixel(format)
        //          << ", mipmaps: " << numMipMaps << ", arrays: " << numArrays
        //          << std::boolalpha << ", cubemap: " << isCubemap << ", compressed: " << isCompressed << std::endl;

        if (auto it = kFormatMap.find(format); it != kFormatMap.end())
        {
            if (isCompressed)
            {
                return readCompressed(ddsFile, it->second);
            }
            else
            {
                auto raw = allocateAndCopyToContiguousBlock(ddsFile);

                vsg::ref_ptr<vsg::Data> vsg_data;

                vsg::Data::Layout layout;
                layout.format = it->second;
                layout.maxNumMipmaps = numMipMaps;
                layout.imageViewType = computeImageViewType(ddsFile);

                switch (dim)
                {
                case tinyddsloader::DDSFile::TextureDimension::Texture1D:
                    vsg_data = vsg::ubvec4Array::create(width, reinterpret_cast<vsg::ubvec4*>(raw), layout);
                    break;
                case tinyddsloader::DDSFile::TextureDimension::Texture2D:
                    if (numArrays > 1)
                    {
                        vsg_data = vsg::ubvec4Array3D::create(width, height, numArrays, reinterpret_cast<vsg::ubvec4*>(raw), layout);
                    }
                    else
                    {
                        vsg_data = vsg::ubvec4Array2D::create(width, height, reinterpret_cast<vsg::ubvec4*>(raw), layout);
                    }
                    break;
                case tinyddsloader::DDSFile::TextureDimension::Texture3D:
                    vsg_data = vsg::ubvec4Array3D::create(width, height, depth, reinterpret_cast<vsg::ubvec4*>(raw), layout);
                    break;
                case tinyddsloader::DDSFile::TextureDimension::Unknown:
                    std::cerr << "dds::readDds() Num of dimnension (" << (uint32_t)dim << ")  is supported." << std::endl;
                    break;
                }

                //std::cout << "* Finish: " << valueCount * valueSize << ", " << vsg_data->dataSize() << std::endl;

                return vsg_data;
            }
        }
        else
        {
            std::cerr << "dds::readDds() Format is not supported yet: " << (uint32_t)format << std::endl;
        }

        return {};
    }
} // namespace

using namespace vsgXchange;

dds::dds() :
    _supportedExtensions{".dds"}
{
}

vsg::ref_ptr<vsg::Object> dds::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    if (const auto ext = vsg::lowerCaseFileExtension(filename); _supportedExtensions.count(ext) == 0)
        return {};

    vsg::Path filenameToUse = findFile(filename, options);
    if (filenameToUse.empty()) return {};

    tinyddsloader::DDSFile ddsFile;

    if (const auto result = ddsFile.Load(filenameToUse.c_str()); result == tinyddsloader::Success)
    {
        return readDds(ddsFile);
    }
    else
    {
        switch (result)
        {
        case tinyddsloader::ErrorNotSupported:
            std::cerr << "dds::read(" << filename << ") Error loading file: Feature not supported" << std::endl;
            break;
        default:
            std::cerr << "dds::read(" << filename << ") Error loading file: " << result << std::endl;
            break;
        }
    }

    return {};
}

vsg::ref_ptr<vsg::Object> dds::read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!options || _supportedExtensions.count(options->extensionHint) == 0)
        return {};

    tinyddsloader::DDSFile ddsFile;
    if (const auto result = ddsFile.Load(fin); result == tinyddsloader::Success)
    {
        return readDds(ddsFile);
    }
    else
    {
        switch (result)
        {
        case tinyddsloader::ErrorNotSupported:
            std::cerr << "dds::read(istream&) Error loading file: Feature not supported" << std::endl;
            break;
        default:
            std::cerr << "dds::read(istream&) Error loading file: " << result << std::endl;
            break;
        }
    }

    return {};
}

vsg::ref_ptr<vsg::Object> dds::read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!options || _supportedExtensions.count(options->extensionHint) == 0)
        return {};

    tinyddsloader::DDSFile ddsFile;
    if (const auto result = ddsFile.Load(ptr, size); result == tinyddsloader::Success)
    {
        return readDds(ddsFile);
    }
    else
    {
        switch (result)
        {
        case tinyddsloader::ErrorNotSupported:
            std::cerr << "dds::read(uint_8_t*, size_t) Error loading file: Feature not supported" << std::endl;
            break;
        default:
            std::cerr << "dds::readuint_8_t*, size_t) Error loading file: " << result << std::endl;
            break;
        }
    }

    return {};
}

bool dds::getFeatures(Features& features) const
{
    for(auto& ext : _supportedExtensions)
    {
        features.extensionFeatureMap[ext] = static_cast<vsg::ReaderWriter::FeatureMask>(vsg::ReaderWriter::READ_FILENAME | vsg::ReaderWriter::READ_ISTREAM | vsg::ReaderWriter::READ_MEMORY);
    }
    return true;
}
