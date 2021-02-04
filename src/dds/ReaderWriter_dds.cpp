#include "ReaderWriter_dds.h"

#include <vsg/io/FileSystem.h>
#include <vsg/io/ObjectCache.h>

#include <cstring>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#define TINYDDSLOADER_IMPLEMENTATION
#include "tinyddsloader.h"

#if defined(__GNUC__)
#pragma GCC diagnostic pop
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
        {tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm_SRGB, VK_FORMAT_BC1_RGBA_SRGB_BLOCK}
    };

    vsg::ref_ptr<vsg::Data> readCompressed(tinyddsloader::DDSFile& ddsFile, VkFormat targetFormat)
    {
        const auto width = ddsFile.GetWidth();
        const auto height = ddsFile.GetHeight();
        const auto depth = ddsFile.GetDepth();
        const auto numMipMaps = ddsFile.GetMipCount();
        const auto isCubemap = ddsFile.IsCubemap();
        const auto numArrays = ddsFile.GetArraySize();

        vsg::ref_ptr<vsg::Data> vsg_data;
        using image_t = std::vector<uint8_t>;
        std::vector<image_t> images(numMipMaps * numArrays);
        size_t offset = 0;

        for (uint32_t i = 0; i < numMipMaps; ++i)
        {
            for (uint32_t j = 0; j < numArrays; ++j)
            {
                const auto data = ddsFile.GetImageData(i, j);
                auto& face = images[numArrays * i + j];

                face.reserve(data->m_memSlicePitch);
                face.assign((uint8_t*)data->m_mem, (uint8_t*)data->m_mem + data->m_memSlicePitch);

                //std::cout << "Compressed: Face " << j << ", Level " << i
                //          << ", faceLodSize = " << face.size() << ", offset = " << offset << ", memPitch = " << data->m_memPitch << ", memSlice = " << data->m_memSlicePitch << std::endl;

                offset += data->m_memSlicePitch;
            }
        }

        //std::cout << "Total size = " << offset << std::endl;

        auto raw = new uint8_t[offset];
        offset = 0;

        for (auto& image : images)
        {
            std::memcpy(raw + offset, image.data(), image.size());
            offset += image.size();
        }

        vsg::Data::Layout layout;
        layout.format = targetFormat;
        layout.maxNumMipmaps = numMipMaps;
        layout.blockWidth = 4;
        layout.blockHeight = 4;

        switch (targetFormat)
        {
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        case VK_FORMAT_BC4_SNORM_BLOCK:
        case VK_FORMAT_BC4_UNORM_BLOCK:
            if (isCubemap && numArrays == 6)
                vsg_data = vsg::block64ArrayCube::create(width / layout.blockWidth, height / layout.blockHeight, reinterpret_cast<vsg::block64*>(raw), layout);
            else
                vsg_data = vsg::block64Array2D::create(width / layout.blockWidth, height / layout.blockHeight, reinterpret_cast<vsg::block64*>(raw), layout);
            break;
            break;
        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC5_UNORM_BLOCK:
        case VK_FORMAT_BC5_SNORM_BLOCK:
        case VK_FORMAT_BC7_UNORM_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:
            if (isCubemap && numArrays == 6)
                vsg_data = vsg::block128ArrayCube::create(width / layout.blockWidth, height / layout.blockHeight, reinterpret_cast<vsg::block128*>(raw), layout);
            else
                vsg_data = vsg::block128Array2D::create(width / layout.blockWidth, height / layout.blockHeight, reinterpret_cast<vsg::block128*>(raw), layout);
            break;
        }

        return vsg_data;
    }

    vsg::ref_ptr<vsg::Data> readDds(tinyddsloader::DDSFile &ddsFile)
    {
        const auto width = ddsFile.GetWidth();
        const auto height = ddsFile.GetHeight();
        const auto depth = ddsFile.GetDepth();
        const auto numMipMaps = ddsFile.GetMipCount();
        const auto isCubemap = ddsFile.IsCubemap();
        const auto format = ddsFile.GetFormat();
        const auto isCompressed = ddsFile.IsCompressed(format);
        const auto dim = ddsFile.GetTextureDimension();
        const auto numArrays = ddsFile.GetArraySize();
        const auto valueCount = vsg::Data::computeValueCountIncludingMipmaps(width, height, depth, numMipMaps) * numArrays;
        const auto bpp = tinyddsloader::DDSFile::GetBitsPerPixel(format);

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
                using image_t = std::vector<uint8_t>;
                size_t offset = 0;
                vsg::ref_ptr<vsg::Data> vsg_data;
                std::vector<image_t> images(numMipMaps * numArrays);

                for (uint32_t i = 0; i < numMipMaps; ++i)
                {
                    for (uint32_t j = 0; j < numArrays; ++j)
                    {
                        const auto data = ddsFile.GetImageData(i, j);
                        auto& face = images[numArrays * i + j];

                        face.reserve(data->m_memSlicePitch);
                        face.assign((uint8_t*)data->m_mem, (uint8_t*)data->m_mem + data->m_memSlicePitch);

                        //std::cout << "Uncompressed: Face " << j << ", Level " << i 
                        //          << ", faceLodSize = " << faceLodSize << ", offset = " << p - raw << std::endl;

                        offset += data->m_memSlicePitch;
                    }
                }

                auto raw = new uint8_t[offset];
                offset = 0;

                for (auto& image : images)
                {
                    std::memcpy(raw + offset, image.data(), image.size());
                    offset += image.size();
                }

                vsg::Data::Layout layout;
                layout.format = it->second;
                layout.maxNumMipmaps = numMipMaps;

                switch (dim)
                {
                case tinyddsloader::DDSFile::TextureDimension::Texture1D:
                    vsg_data = vsg::ubvec4Array::create(width, reinterpret_cast<vsg::ubvec4*>(raw), layout);
                    break;
                case tinyddsloader::DDSFile::TextureDimension::Texture2D:
                    if (isCubemap && numArrays == 6)
                        vsg_data = vsg::ubvec4ArrayCube::create(width, height, reinterpret_cast<vsg::ubvec4*>(raw), layout);
                    else
                        vsg_data = vsg::ubvec4Array2D::create(width, height, reinterpret_cast<vsg::ubvec4*>(raw), layout);
                    break;
                case tinyddsloader::DDSFile::TextureDimension::Texture3D:
                    vsg_data = vsg::ubvec4Array3D::create(width, height, depth, reinterpret_cast<vsg::ubvec4*>(raw), layout);
                    break;
                }

                //std::cout << "* Finish: " << valueCount * valueSize << ", " << vsg_data->dataSize() << std::endl;

                return vsg_data;
            }
        }
        else
        {
            std::cerr << "Format is not supported yet: " << (uint32_t)format << std::endl;
        }

        return {};
    }
}

using namespace vsgXchange;

ReaderWriter_dds::ReaderWriter_dds() :
    _supportedExtensions{"dds"}
{
}

vsg::ref_ptr<vsg::Object> ReaderWriter_dds::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> /*options*/) const
{
    if (!vsg::fileExists(filename))
        return {};

    if (const auto ext = vsg::fileExtension(filename); _supportedExtensions.count(ext) == 0)
        return {};

    tinyddsloader::DDSFile ddsFile;

    if (const auto result = ddsFile.Load(filename.c_str()); result == tinyddsloader::Success)
    {
        return readDds(ddsFile);
    }
    else
    {
        switch (result)
        {
        case tinyddsloader::ErrorNotSupported:
            std::cerr << "Error loading file: Feature not supported" << std::endl;
            break;
        default:
            std::cerr << "Error loading file: " << result << std::endl;
            break;
        }
    }

    return {};
}

vsg::ref_ptr<vsg::Object> ReaderWriter_dds::read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const
{
    if (_supportedExtensions.count(options->extensionHint) == 0)
        return {};

    std::vector<uint8_t> buffer(1 << 16, 0); // 64kB
    std::vector<uint8_t> input;

    while (!fin.eof())
    {
        fin.read((char*)&buffer[0], buffer.size());
        const auto bytes_readed = fin.gcount();
        input.insert(input.end(), buffer.begin(), buffer.end());
    }

    tinyddsloader::DDSFile ddsFile;

    if (const auto result = ddsFile.Load(std::move(input)); result == tinyddsloader::Success)
    {
        return readDds(ddsFile);
    }
    else
    {
        switch (result)
        {
        case tinyddsloader::ErrorNotSupported:
            std::cerr << "Error loading file: Feature not supported" << std::endl;
            break;
        default:
            std::cerr << "Error loading file: " << result << std::endl;
            break;
        }
    }

    return {};
}
