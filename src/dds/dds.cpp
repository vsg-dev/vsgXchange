/* <editor-fold desc="MIT License">

Copyright(c) 2021 André Normann & Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgXchange/images.h>

#include <vsg/core/Array.h>
#include <vsg/core/MipmapLayout.h>
#include <vsg/io/FileSystem.h>
#include <vsg/io/stream.h>
#include <vsg/state/ImageInfo.h>
#include <vsg/utils/CommandLine.h>
#include <vsg/utils/CoordinateSpace.h>

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
        {tinyddsloader::DDSFile::DXGIFormat::B8G8R8A8_UNorm, VK_FORMAT_B8G8R8A8_UNORM},
        {tinyddsloader::DDSFile::DXGIFormat::B8G8R8A8_UNorm_SRGB, VK_FORMAT_B8G8R8A8_SRGB},
        {tinyddsloader::DDSFile::DXGIFormat::R8G8_UNorm, VK_FORMAT_R8G8_UNORM},
        {tinyddsloader::DDSFile::DXGIFormat::R8G8_SNorm, VK_FORMAT_R8G8_SNORM},
        {tinyddsloader::DDSFile::DXGIFormat::R8G8_UInt, VK_FORMAT_R8G8_UINT},
        {tinyddsloader::DDSFile::DXGIFormat::R8G8_SInt, VK_FORMAT_R8G8_SINT},
        {tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm, VK_FORMAT_BC7_UNORM_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm_SRGB, VK_FORMAT_BC7_SRGB_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC6H_UF16, VK_FORMAT_BC6H_UFLOAT_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC6H_SF16, VK_FORMAT_BC6H_SFLOAT_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC5_UNorm, VK_FORMAT_BC5_UNORM_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC5_SNorm, VK_FORMAT_BC5_SNORM_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC4_UNorm, VK_FORMAT_BC4_UNORM_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC4_SNorm, VK_FORMAT_BC4_SNORM_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm, VK_FORMAT_BC3_UNORM_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm_SRGB, VK_FORMAT_BC3_SRGB_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC2_UNorm, VK_FORMAT_BC2_UNORM_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC2_UNorm_SRGB, VK_FORMAT_BC2_SRGB_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm, VK_FORMAT_BC1_RGBA_UNORM_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm_SRGB, VK_FORMAT_BC1_RGBA_SRGB_BLOCK},
        {tinyddsloader::DDSFile::DXGIFormat::R16G16B16A16_Float, VK_FORMAT_R16G16B16A16_SFLOAT},
        {tinyddsloader::DDSFile::DXGIFormat::R32G32B32A32_Float, VK_FORMAT_R32G32B32A32_SFLOAT}};

    std::pair<uint8_t*, vsg::ref_ptr<vsg::MipmapLayout>> allocateAndCopyToContiguousBlock(tinyddsloader::DDSFile& ddsFile, const vsg::Data::Properties& layout)
    {
        const auto numMipMaps = layout.mipLevels;
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

        if (totalSize == 0) return {nullptr, nullptr};

        auto raw = new uint8_t[totalSize];

        auto mipmapLayout = vsg::MipmapLayout::create(numMipMaps);
        auto mipmapItr = mipmapLayout->begin();

        uint32_t offset = 0;
        uint8_t* image_ptr = raw;
        for (uint32_t i = 0; i < numMipMaps; ++i)
        {
            for (uint32_t j = 0; j < numArrays; ++j)
            {
                const auto data = ddsFile.GetImageData(i, j);

                if (j == 0)
                {
                    (*mipmapItr++).set(data->m_width, data->m_height, data->m_depth, offset);
                }

                std::memcpy(image_ptr, data->m_mem, data->m_memSlicePitch);

                offset += static_cast<uint32_t>(data->m_memSlicePitch);
                image_ptr += data->m_memSlicePitch;
            }
        }

        return {raw, mipmapLayout};
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

    static void process_image_format(vsg::ref_ptr<const vsg::Options> options, VkFormat& format)
    {
        if (!options) return;

        vsg::CoordinateSpace coordinateSpace;
        if (options->getValue(vsgXchange::dds::image_format, coordinateSpace))
        {
            if (coordinateSpace == vsg::CoordinateSpace::sRGB)
                format = vsg::uNorm_to_sRGB(format);
            else if (coordinateSpace == vsg::CoordinateSpace::LINEAR)
                format = vsg::sRGB_to_uNorm(format);
        }
    }

    vsg::ref_ptr<vsg::Data> readCompressed(tinyddsloader::DDSFile& ddsFile, VkFormat targetFormat)
    {
        const auto width = ddsFile.GetWidth();
        const auto height = ddsFile.GetHeight();
        const auto numArrays = ddsFile.GetArraySize();

        auto formatTraits = vsg::getFormatTraits(targetFormat);

        uint32_t widthInBlocks = (width + formatTraits.blockWidth - 1) / formatTraits.blockWidth;
        uint32_t heightInBlocks = (height + formatTraits.blockHeight - 1) / formatTraits.blockHeight;

        uint32_t numMipMaps = ddsFile.GetMipCount();

        vsg::Data::Properties layout;
        layout.format = targetFormat;
        layout.mipLevels = numMipMaps;
        layout.blockWidth = formatTraits.blockWidth;
        layout.blockHeight = formatTraits.blockHeight;
        layout.blockDepth = formatTraits.blockDepth;
        layout.imageViewType = computeImageViewType(ddsFile);

        auto [raw, mipmapLayout] = allocateAndCopyToContiguousBlock(ddsFile, layout);

        vsg::ref_ptr<vsg::Data> vsg_data;

        switch (targetFormat)
        {
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        case VK_FORMAT_BC4_SNORM_BLOCK:
        case VK_FORMAT_BC4_UNORM_BLOCK:
            if (numArrays > 1)
                vsg_data = vsg::block64Array3D::create(widthInBlocks, heightInBlocks, numArrays, reinterpret_cast<vsg::block64*>(raw), layout, mipmapLayout);
            else
                vsg_data = vsg::block64Array2D::create(widthInBlocks, heightInBlocks, reinterpret_cast<vsg::block64*>(raw), layout, mipmapLayout);
            break;
        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC5_UNORM_BLOCK:
        case VK_FORMAT_BC5_SNORM_BLOCK:
        case VK_FORMAT_BC6H_UFLOAT_BLOCK:
        case VK_FORMAT_BC6H_SFLOAT_BLOCK:
        case VK_FORMAT_BC7_UNORM_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:
            if (numArrays > 1)
                vsg_data = vsg::block128Array3D::create(widthInBlocks, heightInBlocks, numArrays, reinterpret_cast<vsg::block128*>(raw), layout, mipmapLayout);
            else
                vsg_data = vsg::block128Array2D::create(widthInBlocks, heightInBlocks, reinterpret_cast<vsg::block128*>(raw), layout, mipmapLayout);
            break;
        default:
            std::cerr << "dds::readCompressed() Format is not supported yet: " << (uint32_t)targetFormat << std::endl;
            break;
        }
#if 0
        if (vsg_data && mipmapLayout)
        {
            vsg_data->setObject("mipmapLayout", mipmapLayout);
        }
#endif

        return vsg_data;
    }

    vsg::ref_ptr<vsg::Data> readDds(tinyddsloader::DDSFile& ddsFile, vsg::ref_ptr<const vsg::Options> options)
    {
        const auto width = ddsFile.GetWidth();
        const auto height = ddsFile.GetHeight();
        const auto depth = ddsFile.GetDepth();
        const auto numMipMaps = ddsFile.GetMipCount();
        const auto format = ddsFile.GetFormat();
        const auto isCompressed = ddsFile.IsCompressed(format);
        const auto dim = ddsFile.GetTextureDimension();
        const auto numArrays = ddsFile.GetArraySize();

        // const auto bpp = tinyddsloader::DDSFile::GetBitsPerPixel(format);
        //std::cerr << "Fileinfo width: " << width << ", height: " << height << ", depth: " << depth << ", count: " << valueCount
        //          << ", format: " << (int)format << ", bpp: " << tinyddsloader::DDSFile::GetBitsPerPixel(format)
        //          << ", mipmaps: " << numMipMaps << ", arrays: " << numArrays
        //          << std::boolalpha << ", cubemap: " << isCubemap << ", compressed: " << isCompressed << std::endl;

        if (auto it = kFormatMap.find(format); it != kFormatMap.end())
        {
            vsg::ref_ptr<vsg::Data> vsg_data;
            if (isCompressed)
            {
                vsg_data = readCompressed(ddsFile, it->second);
            }
            else
            {
                vsg::Data::Properties layout;
                layout.format = it->second;
                layout.mipLevels = numMipMaps;
                layout.imageViewType = computeImageViewType(ddsFile);

                auto [raw, mipmapLayout] = allocateAndCopyToContiguousBlock(ddsFile, layout);

                switch (dim)
                {
                case tinyddsloader::DDSFile::TextureDimension::Texture1D:
                    switch (layout.format)
                    {
                    case VK_FORMAT_R32G32B32A32_SFLOAT:
                        vsg_data = vsg::vec4Array::create(width, reinterpret_cast<vsg::vec4*>(raw), layout, mipmapLayout);
                        break;
                    case VK_FORMAT_R16G16B16A16_SFLOAT:
                        vsg_data = vsg::usvec4Array::create(width, reinterpret_cast<vsg::usvec4*>(raw), layout, mipmapLayout);
                        break;
                    default:
                        vsg_data = vsg::ubvec4Array::create(width, reinterpret_cast<vsg::ubvec4*>(raw), layout, mipmapLayout);
                        break;
                    }
                    break;
                case tinyddsloader::DDSFile::TextureDimension::Texture2D:
                    if (numArrays > 1)
                    {
                        switch (layout.format)
                        {
                        case VK_FORMAT_R32G32B32A32_SFLOAT:
                            vsg_data = vsg::vec4Array3D::create(width, height, numArrays, reinterpret_cast<vsg::vec4*>(raw), layout, mipmapLayout);
                            break;
                        case VK_FORMAT_R16G16B16A16_SFLOAT:
                            vsg_data = vsg::usvec4Array3D::create(width, height, numArrays, reinterpret_cast<vsg::usvec4*>(raw), layout, mipmapLayout);
                            break;
                        default:
                            vsg_data = vsg::ubvec4Array3D::create(width, height, numArrays, reinterpret_cast<vsg::ubvec4*>(raw), layout, mipmapLayout);
                            break;
                        }
                    }
                    else
                    {
                        switch (layout.format)
                        {
                        case VK_FORMAT_R32G32B32A32_SFLOAT:
                            vsg_data = vsg::vec4Array2D::create(width, height, reinterpret_cast<vsg::vec4*>(raw), layout, mipmapLayout);
                            break;
                        case VK_FORMAT_R16G16B16A16_SFLOAT:
                            vsg_data = vsg::usvec4Array2D::create(width, height, reinterpret_cast<vsg::usvec4*>(raw), layout, mipmapLayout);
                            break;
                        default:
                            vsg_data = vsg::ubvec4Array2D::create(width, height, reinterpret_cast<vsg::ubvec4*>(raw), layout, mipmapLayout);
                            break;
                        }
                    }
                    break;
                case tinyddsloader::DDSFile::TextureDimension::Texture3D:
                    switch (layout.format)
                    {
                    case VK_FORMAT_R32G32B32A32_SFLOAT:
                        vsg_data = vsg::vec4Array3D::create(width, height, depth, reinterpret_cast<vsg::vec4*>(raw), layout, mipmapLayout);
                        break;
                    case VK_FORMAT_R16G16B16A16_SFLOAT:
                        vsg_data = vsg::usvec4Array3D::create(width, height, depth, reinterpret_cast<vsg::usvec4*>(raw), layout, mipmapLayout);
                        break;
                    default:
                        vsg_data = vsg::ubvec4Array3D::create(width, height, depth, reinterpret_cast<vsg::ubvec4*>(raw), layout, mipmapLayout);
                        break;
                    }
                    break;
                case tinyddsloader::DDSFile::TextureDimension::Unknown:
                    std::cerr << "dds::readDds() Num of dimension (" << (uint32_t)dim << ")  not supported." << std::endl;
                    break;
                }
#if 0
                if (vsg_data && mipmapLayout)
                {
                    vsg_data->setObject("mipmapLayout", mipmapLayout);
                }
#endif
            }

            if (options && vsg_data) process_image_format(options, vsg_data->properties.format);

            return vsg_data;
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
    if (!vsg::compatibleExtension(filename, options, _supportedExtensions)) return {};

    vsg::Path filenameToUse = findFile(filename, options);
    if (!filenameToUse) return {};

    tinyddsloader::DDSFile ddsFile;

    std::ifstream ifs(filenameToUse, std::ios_base::binary);
    if (const auto result = ddsFile.Load(ifs); result == tinyddsloader::Success)
    {
        return readDds(ddsFile, options);
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
    if (!vsg::compatibleExtension(options, _supportedExtensions)) return {};

    tinyddsloader::DDSFile ddsFile;
    if (const auto result = ddsFile.Load(fin); result == tinyddsloader::Success)
    {
        return readDds(ddsFile, options);
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
    if (!vsg::compatibleExtension(options, _supportedExtensions)) return {};

    tinyddsloader::DDSFile ddsFile;
    if (const auto result = ddsFile.Load(ptr, size); result == tinyddsloader::Success)
    {
        return readDds(ddsFile, options);
    }
    else
    {
        switch (result)
        {
        case tinyddsloader::ErrorNotSupported:
            std::cerr << "dds::read(uint_8_t*, size_t) Error loading file: Feature not supported" << std::endl;
            break;
        default:
            std::cerr << "dds::read(uint_8_t*, size_t) Error loading file: " << result << std::endl;
            break;
        }
    }

    return {};
}

bool dds::getFeatures(Features& features) const
{
    for (auto& ext : _supportedExtensions)
    {
        features.extensionFeatureMap[ext] = static_cast<vsg::ReaderWriter::FeatureMask>(vsg::ReaderWriter::READ_FILENAME | vsg::ReaderWriter::READ_ISTREAM | vsg::ReaderWriter::READ_MEMORY);
    }

    features.optionNameTypeMap[dds::image_format] = vsg::type_name<vsg::CoordinateSpace>();

    return true;
}

bool dds::readOptions(vsg::Options& options, vsg::CommandLine& arguments) const
{
    bool result = arguments.readAndAssign<vsg::CoordinateSpace>(dds::image_format, &options);
    return result;
}
