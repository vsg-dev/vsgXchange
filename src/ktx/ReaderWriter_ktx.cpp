#include "ReaderWriter_ktx.h"

#include <vsg/state/DescriptorImage.h>

#include <ktx.h>
#include <ktxvulkan.h>

#include <algorithm>
#include <cstring>

namespace
{
    const std::unordered_set<VkFormat> kFormatSet{
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SNORM,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_BC7_UNORM_BLOCK,
        VK_FORMAT_BC7_SRGB_BLOCK,
        VK_FORMAT_BC5_UNORM_BLOCK,
        VK_FORMAT_BC5_SNORM_BLOCK,
        VK_FORMAT_BC4_UNORM_BLOCK,
        VK_FORMAT_BC4_SNORM_BLOCK,
        VK_FORMAT_BC3_UNORM_BLOCK,
        VK_FORMAT_BC3_SRGB_BLOCK,
        VK_FORMAT_BC2_UNORM_BLOCK,
        VK_FORMAT_BC2_SRGB_BLOCK,
        VK_FORMAT_BC1_RGB_UNORM_BLOCK,
        VK_FORMAT_BC1_RGB_SRGB_BLOCK,
        VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
        VK_FORMAT_BC1_RGBA_SRGB_BLOCK};

    vsg::ref_ptr<vsg::Data> readKtx(ktxTexture* texture)
    {
        vsg::ref_ptr<vsg::Data> data;
        const auto width = texture->baseWidth;
        const auto height = texture->baseHeight;
        const auto depth = texture->baseDepth;
        const auto numMipMaps = texture->numLevels;
        const auto numArrays = texture->numFaces;
        const auto textureData = ktxTexture_GetData(texture);
        //const auto textureSize = ktxTexture_GetDataSize(texture);
        const auto format = ktxTexture_GetVkFormat(texture);
        const auto valueSize = ktxTexture_GetElementSize(texture);

        if (kFormatSet.count(format) > 0)
        {
            const uint32_t blockSize = texture->isCompressed ? 4 : 1;
            size_t offset = 0;
            auto mipWidth = (width) / blockSize;
            auto mipHeight = (height) / blockSize;
            auto mipDepth = depth;

            using image_t = std::vector<uint8_t>;
            std::vector<image_t> images(numMipMaps * numArrays);

            for (uint32_t i = 0; i < numMipMaps; ++i)
            {
                const auto faceSize = std::max(mipWidth * mipHeight * mipDepth * valueSize, valueSize);

                for (uint32_t j = 0; j < numArrays; ++j)
                {
                    auto& face = images[numArrays * i + j];

                    if (ktx_size_t ktxOffset = 0; ktxTexture_GetImageOffset(texture, i, 0, j, &ktxOffset) == KTX_SUCCESS)
                    {
                        face.reserve(faceSize);
                        face.assign((uint8_t*)textureData + ktxOffset, (uint8_t*)textureData + ktxOffset + faceSize);
                    }

                    offset += faceSize;
                }

                if (mipWidth > 1) mipWidth /= 2;
                if (mipHeight > 1) mipHeight /= 2;
                if (mipDepth > 1) mipDepth /= 2;
            }

            auto raw = new uint8_t[offset];
            offset = 0;

            for (auto& image : images)
            {
                std::memcpy(raw + offset, image.data(), image.size());
                offset += image.size();
            }

            uint32_t numImages = depth * numArrays;

            vsg::Data::Layout layout;
            layout.format = format;
            layout.maxNumMipmaps = numMipMaps;

            if (depth > 1)
            {
                layout.imageViewType = VK_IMAGE_VIEW_TYPE_3D;
            }
            else if (numArrays > 1)
            {
                if (texture->isCubemap)
                {
                    layout.imageViewType = (numArrays > 6) ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
                }
                else
                {
                    layout.imageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                }
            }
            else
            {
                layout.imageViewType = VK_IMAGE_VIEW_TYPE_2D;
            }

            switch (format)
            {
            case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
            case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
            case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
            case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
            case VK_FORMAT_BC4_SNORM_BLOCK:
            case VK_FORMAT_BC4_UNORM_BLOCK:
                layout.blockWidth = blockSize;
                layout.blockHeight = blockSize;

                if (numImages>1)
                {
                    data = vsg::block64Array3D::create(width / blockSize, height / blockSize, numImages, reinterpret_cast<vsg::block64*>(raw), layout);
                }
                else
                {
                    data = vsg::block64Array2D::create(width / blockSize, height / blockSize, reinterpret_cast<vsg::block64*>(raw), layout);
                }
                break;

            case VK_FORMAT_BC2_UNORM_BLOCK:
            case VK_FORMAT_BC2_SRGB_BLOCK:
            case VK_FORMAT_BC3_UNORM_BLOCK:
            case VK_FORMAT_BC3_SRGB_BLOCK:
            case VK_FORMAT_BC5_UNORM_BLOCK:
            case VK_FORMAT_BC5_SNORM_BLOCK:
            case VK_FORMAT_BC7_UNORM_BLOCK:
            case VK_FORMAT_BC7_SRGB_BLOCK:
                layout.blockWidth = blockSize;
                layout.blockHeight = blockSize;

                if (numImages > 1)
                {
                    data = vsg::block128Array3D::create(width / blockSize, height / blockSize, numImages, reinterpret_cast<vsg::block128*>(raw), layout);
                }
                else
                {
                    data = vsg::block128Array2D::create(width / blockSize, height / blockSize, reinterpret_cast<vsg::block128*>(raw), layout);
                }
                break;

            default:
                if (numImages > 1)
                    data = vsg::ubvec4Array3D::create(width, height, numImages, reinterpret_cast<vsg::ubvec4*>(raw), layout);
                else
                    data = vsg::ubvec4Array2D::create(width, height, reinterpret_cast<vsg::ubvec4*>(raw), layout);
                break;
            }
        }

        ktxTexture_Destroy(texture);

        return data;
    }

} // namespace

using namespace vsgXchange;

ReaderWriter_ktx::ReaderWriter_ktx() :
    _supportedExtensions{"ktx", "ktx2"}
{
}

vsg::ref_ptr<vsg::Object> ReaderWriter_ktx::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    if (const auto ext = vsg::lowerCaseFileExtension(filename); _supportedExtensions.count(ext) == 0)
        return {};

    vsg::Path filenameToUse = findFile(filename, options);
    if (filenameToUse.empty()) return {};

    if (ktxTexture * texture{nullptr}; ktxTexture_CreateFromNamedFile(filenameToUse.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture) == KTX_SUCCESS)
        return readKtx(texture);

    return {};
}

vsg::ref_ptr<vsg::Object> ReaderWriter_ktx::read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const
{
    if (_supportedExtensions.count(options->extensionHint) == 0)
        return {};

    std::string buffer(1 << 16, 0); // 64kB
    std::string input;

    while (!fin.eof())
    {
        fin.read(&buffer[0], buffer.size());
        const auto bytes_readed = fin.gcount();
        input.append(&buffer[0], bytes_readed);
    }

    if (ktxTexture * texture{nullptr}; ktxTexture_CreateFromMemory((const ktx_uint8_t*)input.data(), input.size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture) == KTX_SUCCESS)
        return readKtx(texture);

    return {};
}
