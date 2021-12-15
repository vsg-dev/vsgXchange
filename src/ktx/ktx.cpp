/* <editor-fold desc="MIT License">

Copyright(c) 2021 André Normann & Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgXchange/images.h>

#include <vsg/core/Exception.h>
#include <vsg/state/DescriptorImage.h>

#include <ktx.h>
#include <ktxvulkan.h>
#include <texture.h>
#include <vk_format.h>

#include <algorithm>
#include <cstring>
#include <iostream>

namespace
{

    template<typename T>
    vsg::ref_ptr<vsg::Data> createImage(uint32_t arrayDimensions, uint32_t width, uint32_t height, uint32_t depth, uint8_t* data, vsg::Data::Layout layout)
    {
        switch (arrayDimensions)
        {
        case 1: return vsg::Array<T>::create(width, reinterpret_cast<T*>(data), layout);
        case 2: return vsg::Array2D<T>::create(width, height, reinterpret_cast<T*>(data), layout);
        case 3: return vsg::Array3D<T>::create(width, height, depth, reinterpret_cast<T*>(data), layout);
        default: return {};
        }
    }

    vsg::ref_ptr<vsg::Data> readKtx(ktxTexture* texture, const vsg::Path& /*filename*/)
    {
        uint32_t width = texture->baseWidth;
        uint32_t height = texture->baseHeight;
        uint32_t depth = texture->baseDepth;
        const auto numMipMaps = texture->numLevels;
        const auto numLayers = texture->numLayers;
        const auto textureData = ktxTexture_GetData(texture);
        const auto format = ktxTexture_GetVkFormat(texture);

        ktxFormatSize formatSize;
        vkGetFormatSize(format, &formatSize);

        if (formatSize.blockSizeInBits != texture->_protected->_formatSize.blockSizeInBits)
        {
#if 0
            auto before_valueSize = ktxTexture_GetElementSize(texture);

            vkGetFormatSize( format, &(texture->_protected->_formatSize) );

            auto after_valueSize = ktxTexture_GetElementSize(texture);

            std::cout<<"ktx::read("<<filename<<") Fallback : format = "<<format<<", before_valueSize = "<<before_valueSize<<" after_valueSize = "<<after_valueSize<<std::endl;
#endif
            throw vsg::Exception{"Mismatched ktxFormatSize.blockSize and ktxTexture_GetElementSize(texture)."};
        }

        auto valueSize = ktxTexture_GetElementSize(texture);

        vsg::Data::Layout layout;
        layout.format = format;
        layout.blockWidth = texture->_protected->_formatSize.blockWidth;
        layout.blockHeight = texture->_protected->_formatSize.blockHeight;
        layout.blockDepth = texture->_protected->_formatSize.blockDepth;
        layout.maxNumMipmaps = numMipMaps;
        layout.origin = static_cast<uint8_t>(((texture->orientation.x == KTX_ORIENT_X_RIGHT) ? 0 : 1) |
                                             ((texture->orientation.y == KTX_ORIENT_Y_DOWN) ? 0 : 2) |
                                             ((texture->orientation.z == KTX_ORIENT_Z_OUT) ? 0 : 4));

        width /= layout.blockWidth;
        height /= layout.blockHeight;
        depth /= layout.blockDepth;

        // compute the textureSize.
        size_t textureSize = 0;
        {
            auto mipWidth = width;
            auto mipHeight = height;
            auto mipDepth = depth;

            for (uint32_t level = 0; level < numMipMaps; ++level)
            {
                const auto faceSize = std::max(mipWidth * mipHeight * mipDepth * valueSize, valueSize);
                textureSize += faceSize;

                if (mipWidth > 1) mipWidth /= 2;
                if (mipHeight > 1) mipHeight /= 2;
                if (mipDepth > 1) mipDepth /= 2;
            }
            textureSize *= (texture->numLayers * texture->numFaces);
        }

        // copy the data and repack into ordered assumed by VSG
        uint8_t* copiedData = new uint8_t[textureSize];

        size_t offset = 0;

        auto mipWidth = width;
        auto mipHeight = height;
        auto mipDepth = depth;

        for (uint32_t level = 0; level < numMipMaps; ++level)
        {
            const auto faceSize = std::max(mipWidth * mipHeight * mipDepth * valueSize, valueSize);
            for (uint32_t layer = 0; layer < texture->numLayers; ++layer)
            {
                for (uint32_t face = 0; face < texture->numFaces; ++face)
                {
                    if (ktx_size_t ktxOffset = 0; ktxTexture_GetImageOffset(texture, level, layer, face, &ktxOffset) == KTX_SUCCESS)
                    {
                        std::memcpy(copiedData + offset, textureData + ktxOffset, faceSize);
                    }

                    offset += faceSize;
                }
            }
            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;
            if (mipDepth > 1) mipDepth /= 2;
        }

        uint32_t arrayDimensions = 0;
        switch (texture->numDimensions)
        {
        case 1:
            layout.imageViewType = (numLayers == 1) ? VK_IMAGE_VIEW_TYPE_1D : VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            arrayDimensions = (numLayers == 1) ? 1 : 2;
            height = numLayers;
            break;

        case 2:
            if (texture->isCubemap)
            {
                layout.imageViewType = (numLayers == 1) ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
                arrayDimensions = 3;
                depth = 6 * numLayers;
            }
            else
            {
                layout.imageViewType = (numLayers == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                arrayDimensions = (numLayers == 1) ? 2 : 3;
                depth = numLayers;
            }
            break;

        case 3:
            layout.imageViewType = VK_IMAGE_VIEW_TYPE_3D;
            arrayDimensions = 3;
            break;

        default:
            throw vsg::Exception{"Invalid number of dimensions."};
        }

        // create the VSG compressed image objects
        if (texture->isCompressed)
        {
            switch (valueSize)
            {
            case 8: return createImage<vsg::block64>(arrayDimensions, width, height, depth, copiedData, layout);
            case 16: return createImage<vsg::block128>(arrayDimensions, width, height, depth, copiedData, layout);
            default: throw vsg::Exception{"Unsupported compressed format."};
            }
        }

        // handle common formats
        switch (format)
        {
        case VK_FORMAT_R8_SRGB:
        case VK_FORMAT_R8_UNORM: return createImage<uint8_t>(arrayDimensions, width, height, depth, copiedData, layout);
        case VK_FORMAT_R8_SNORM: return createImage<int8_t>(arrayDimensions, width, height, depth, copiedData, layout);
        case VK_FORMAT_R8G8_SRGB:
        case VK_FORMAT_R8G8_UNORM: return createImage<vsg::ubvec2>(arrayDimensions, width, height, depth, copiedData, layout);
        case VK_FORMAT_R8G8_SNORM: return createImage<vsg::bvec2>(arrayDimensions, width, height, depth, copiedData, layout);
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_R8G8B8_UNORM: return createImage<vsg::ubvec3>(arrayDimensions, width, height, depth, copiedData, layout);
        case VK_FORMAT_R8G8B8_SNORM: return createImage<vsg::bvec3>(arrayDimensions, width, height, depth, copiedData, layout);
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_R8G8B8A8_UNORM: return createImage<vsg::ubvec4>(arrayDimensions, width, height, depth, copiedData, layout);
        case VK_FORMAT_R8G8B8A8_SNORM: return createImage<vsg::bvec4>(arrayDimensions, width, height, depth, copiedData, layout);

        case VK_FORMAT_R16_UNORM: return createImage<uint16_t>(arrayDimensions, width, height, depth, copiedData, layout);
        case VK_FORMAT_R16_SNORM: return createImage<int16_t>(arrayDimensions, width, height, depth, copiedData, layout);
        case VK_FORMAT_R16G16_UNORM: return createImage<vsg::usvec2>(arrayDimensions, width, height, depth, copiedData, layout);
        case VK_FORMAT_R16G16_SNORM: return createImage<vsg::svec2>(arrayDimensions, width, height, depth, copiedData, layout);
        case VK_FORMAT_R16G16B16_UNORM: return createImage<vsg::usvec3>(arrayDimensions, width, height, depth, copiedData, layout);
        case VK_FORMAT_R16G16B16_SNORM: return createImage<vsg::svec3>(arrayDimensions, width, height, depth, copiedData, layout);
        case VK_FORMAT_R16G16B16A16_UNORM: return createImage<vsg::usvec4>(arrayDimensions, width, height, depth, copiedData, layout);
        case VK_FORMAT_R16G16B16A16_SNORM: return createImage<vsg::svec4>(arrayDimensions, width, height, depth, copiedData, layout);
        default: break;
        }

        // create the VSG uncompressed image objects
        switch (valueSize)
        {
        case 1:
            // int8_t or uint8_t
            return createImage<uint8_t>(arrayDimensions, width, height, depth, copiedData, layout);
        case 2:
            // short, ushort, ubvec2, bvec2
            return createImage<uint16_t>(arrayDimensions, width, height, depth, copiedData, layout);
        case 3:
            // ubvec3 or bvec3
            return createImage<vsg::ubvec3>(arrayDimensions, width, height, depth, copiedData, layout);
        case 4:
            // float, int, uint, usvec2, svec2, ubvec4, bvec4
            return createImage<uint32_t>(arrayDimensions, width, height, depth, copiedData, layout);
        case 8:
            // double, vec2, ivec4, uivec4, svec4, uvec4
            return createImage<vsg::usvec4>(arrayDimensions, width, height, depth, copiedData, layout);
        case 16:
            // dvec2, vec4, ivec4, uivec4
            return createImage<vsg::vec4>(arrayDimensions, width, height, depth, copiedData, layout);
        default:
            throw vsg::Exception{"Unsupported valueSize."};
        }

        return {};
    }

} // namespace

using namespace vsgXchange;

ktx::ktx() :
    _supportedExtensions{".ktx", ".ktx2"}
{
}

vsg::ref_ptr<vsg::Object> ktx::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    if (const auto ext = vsg::lowerCaseFileExtension(filename); _supportedExtensions.count(ext) == 0)
        return {};

    vsg::Path filenameToUse = findFile(filename, options);
    if (filenameToUse.empty()) return {};

    if (ktxTexture * texture{nullptr}; ktxTexture_CreateFromNamedFile(filenameToUse.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture) == KTX_SUCCESS)
    {
        vsg::ref_ptr<vsg::Data> data;
        try
        {
            data = readKtx(texture, filename);
        }
        catch (const vsg::Exception& ve)
        {
            std::cout << "ktx::read(" << filenameToUse << ") failed : " << ve.message << std::endl;
        }

        ktxTexture_Destroy(texture);

        return data;
    }

    return {};
}

vsg::ref_ptr<vsg::Object> ktx::read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!options || _supportedExtensions.count(options->extensionHint) == 0)
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
    {
        vsg::ref_ptr<vsg::Data> data;
        try
        {
            data = readKtx(texture, "");
        }
        catch (const vsg::Exception& ve)
        {
            std::cout << "ktx::read(std::istream&) failed : " << ve.message << std::endl;
        }

        ktxTexture_Destroy(texture);

        return data;
    }

    return {};
}

vsg::ref_ptr<vsg::Object> ktx::read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!options || _supportedExtensions.count(options->extensionHint) == 0)
        return {};

    ktxTexture* texture = nullptr;
    if (ktxTexture_CreateFromMemory(ptr, size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture) == KTX_SUCCESS)
    {
        vsg::ref_ptr<vsg::Data> data;
        try
        {
            data = readKtx(texture, "");
        }
        catch (const vsg::Exception& ve)
        {
            std::cout << "ktx::read(uint_8_t*, size_t) failed : " << ve.message << std::endl;
        }

        ktxTexture_Destroy(texture);

        return data;
    }

    return {};
}

bool ktx::getFeatures(Features& features) const
{
    for(auto& ext : _supportedExtensions)
    {
        features.extensionFeatureMap[ext] = static_cast<vsg::ReaderWriter::FeatureMask>(vsg::ReaderWriter::READ_FILENAME | vsg::ReaderWriter::READ_ISTREAM | vsg::ReaderWriter::READ_MEMORY);
    }
    return true;
}
