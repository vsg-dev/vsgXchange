/* <editor-fold desc="MIT License">

Copyright(c) 2021 André Normann & Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgXchange/images.h>

#include <vsg/core/Exception.h>
#include <vsg/core/MipmapLayout.h>
#include <vsg/io/stream.h>
#include <vsg/state/DescriptorImage.h>

#include <ktx.h>
#include <ktxvulkan.h>
// #include <texture.h>
// #include <vk_format.h>

#include <algorithm>
#include <cstring>
#include <iostream>

// https://docs.vulkan.org/samples/latest/samples/performance/texture_compression_basisu/README.html
// https://github.com/KhronosGroup/3D-Formats-Guidelines/blob/main/KTXDeveloperGuide.md
// https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#resources-image-mip-level-sizing

namespace vsgXchange
{

    class ktx::Implementation
    {
    public:
        Implementation();

        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const;
        vsg::ref_ptr<vsg::Object> read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const;
        vsg::ref_ptr<vsg::Object> read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options) const;

        bool getFeatures(Features& features) const;

        std::set<vsg::Path> _supportedExtensions;

        template<typename T>
        vsg::ref_ptr<vsg::Data> createImage(uint32_t arrayDimensions, uint32_t width, uint32_t height, uint32_t depth, uint8_t* data, vsg::Data::Properties properties, vsg::ref_ptr<vsg::MipmapLayout> mipmapLayout = {}) const
        {
            switch (arrayDimensions)
            {
            case 1: return vsg::Array<T>::create(width, reinterpret_cast<T*>(data), properties, mipmapLayout);
            case 2: return vsg::Array2D<T>::create(width, height, reinterpret_cast<T*>(data), properties, mipmapLayout);
            case 3: return vsg::Array3D<T>::create(width, height, depth, reinterpret_cast<T*>(data), properties, mipmapLayout);
            default: return {};
            }
        }

        vsg::ref_ptr<vsg::Data> createImage(uint32_t arrayDimensions, uint32_t width, uint32_t height, uint32_t depth, uint8_t* data, vsg::Data::Properties properties, int valueSize, vsg::ref_ptr<vsg::MipmapLayout> mipmapLayout = {}) const;
        vsg::ref_ptr<vsg::Data> readKtx(ktxTexture* texture, const vsg::Path& filename) const;
        vsg::ref_ptr<vsg::Data> readKtx2(ktxTexture2* texture, const vsg::Path& filename) const;

        struct Face
        {
            int width;
            int height;
            int depth;
            ktx_uint64_t faceLodSize;
            void* pixels;
        };

        struct Faces
        {
            std::map<int, Face> faces;
        };

        struct Mipmaps
        {
            int maxMipMapLevel = 16;
            std::map<int, Faces> mipmaps;
        };

        static KTX_error_code imageIterator(int miplevel, int face, int width, int height, int depth, ktx_uint64_t faceLodSize, void* pixels, void* userdata);
    };

} // namespace vsgXchange

using namespace vsgXchange;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// KTX ReaderWriter facade
//
ktx::ktx() :
    _implementation(new ktx::Implementation())
{
}

ktx::~ktx()
{
    delete _implementation;
}

vsg::ref_ptr<vsg::Object> ktx::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    return _implementation->read(filename, options);
}

vsg::ref_ptr<vsg::Object> ktx::read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const
{
    return _implementation->read(fin, options);
}
vsg::ref_ptr<vsg::Object> ktx::read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options) const
{
    return _implementation->read(ptr, size, options);
}

bool ktx::getFeatures(Features& features) const
{
    return _implementation->getFeatures(features);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// KTX ReaderWriter implementation
//
ktx::Implementation::Implementation() :
    _supportedExtensions{".ktx", ".ktx2"}
{
}

KTX_error_code ktx::Implementation::imageIterator(int miplevel, int face, int width, int height, int depth, ktx_uint64_t faceLodSize, void* pixels, void* userdata)
{
    auto mipmaps = reinterpret_cast<Mipmaps*>(userdata);

    // ignore miplevels higher than the max
    if (miplevel > mipmaps->maxMipMapLevel) return KTX_SUCCESS;

    auto& mipmap = mipmaps->mipmaps[miplevel];
    mipmap.faces[face] = Face{width, height, depth, faceLodSize, pixels};
    return KTX_SUCCESS;
}

vsg::ref_ptr<vsg::Data> ktx::Implementation::createImage(uint32_t arrayDimensions, uint32_t width, uint32_t height, uint32_t depth, uint8_t* data, vsg::Data::Properties layout, int valueSize, vsg::ref_ptr<vsg::MipmapLayout> mipmapLayout) const
{
    // create the VSG compressed image objects
    if (layout.blockWidth != 1 || layout.blockHeight != 1 || layout.blockDepth != 1)
    {
        switch (valueSize)
        {
        case 8: return createImage<vsg::block64>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
        case 16: return createImage<vsg::block128>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
        default: {
            vsg::warn("vsgXchange::ktx : Unsupported compressed format, valueSize = ", valueSize);
            return {};
        }
        }
    }

    // handle common formats
    switch (layout.format)
    {
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_R8_UNORM: return createImage<uint8_t>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    case VK_FORMAT_R8_SNORM: return createImage<int8_t>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R8G8_UNORM: return createImage<vsg::ubvec2>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    case VK_FORMAT_R8G8_SNORM: return createImage<vsg::bvec2>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_R8G8B8_UNORM: return createImage<vsg::ubvec3>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    case VK_FORMAT_R8G8B8_SNORM: return createImage<vsg::bvec3>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM: return createImage<vsg::ubvec4>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    case VK_FORMAT_R8G8B8A8_SNORM: return createImage<vsg::bvec4>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);

    case VK_FORMAT_R16_UNORM: return createImage<uint16_t>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    case VK_FORMAT_R16_SNORM: return createImage<int16_t>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    case VK_FORMAT_R16G16_UNORM: return createImage<vsg::usvec2>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    case VK_FORMAT_R16G16_SNORM: return createImage<vsg::svec2>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    case VK_FORMAT_R16G16B16_UNORM: return createImage<vsg::usvec3>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    case VK_FORMAT_R16G16B16_SNORM: return createImage<vsg::svec3>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    case VK_FORMAT_R16G16B16A16_UNORM: return createImage<vsg::usvec4>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    case VK_FORMAT_R16G16B16A16_SNORM: return createImage<vsg::svec4>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    default: break;
    }

    // create the VSG uncompressed image objects
    switch (valueSize)
    {
    case 1:
        // int8_t or uint8_t
        return createImage<uint8_t>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    case 2:
        // short, ushort, ubvec2, bvec2
        return createImage<uint16_t>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    case 3:
        // ubvec3 or bvec3
        return createImage<vsg::ubvec3>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    case 4:
        // float, int, uint, usvec2, svec2, ubvec4, bvec4
        return createImage<uint32_t>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    case 8:
        // double, vec2, ivec4, uivec4, svec4, uvec4
        return createImage<vsg::usvec4>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    case 16:
        // dvec2, vec4, ivec4, uivec4
        return createImage<vsg::vec4>(arrayDimensions, width, height, depth, data, layout, mipmapLayout);
    default: break;
    }

    vsg::info("Unsupported valueSize = ", valueSize);

    throw vsg::Exception{"Unsupported valueSize."};

    return {};
}

vsg::ref_ptr<vsg::Data> ktx::Implementation::readKtx(ktxTexture* texture, const vsg::Path& filename) const
{
    uint32_t width = texture->baseWidth;
    uint32_t height = texture->baseHeight;
    uint32_t depth = texture->baseDepth;
    const auto format = ktxTexture_GetVkFormat(texture);

    if (format == VK_FORMAT_UNDEFINED)
    {
        vsg::warn("vsgXchange::ktx : unabled to use ", filename, " due to incompatible vkFormat.");
        return {};
    }
#if 0
    vsg::info("\nreadKtx(", texture, ", ", filename, ")");
    vsg::info("   generateMipmaps = ", texture->generateMipmaps);
    vsg::info("   numLayers = ", texture->numLayers);
    vsg::info("   numFaces = ", texture->numFaces);
    vsg::info("   numLevels = ", texture->numLevels);
    vsg::info("   baseWidth = ", texture->baseWidth);
    vsg::info("   baseHeight = ", texture->baseHeight);
    vsg::info("   baseDepth = ", texture->baseDepth);
    vsg::info("   format = ", format);
#endif

    const auto numMipMaps = texture->numLevels;
    const auto numLayers = texture->numLayers;

    vsg::Data::Properties layout;
    layout.format = format;

    auto formatTraits = vsg::getFormatTraits(format);

    layout.blockWidth = formatTraits.blockWidth;
    layout.blockHeight = formatTraits.blockHeight;
    layout.blockDepth = formatTraits.blockDepth;

    layout.mipLevels = numMipMaps;
    layout.origin = static_cast<uint8_t>(((texture->orientation.x == KTX_ORIENT_X_RIGHT) ? 0 : 1) |
                                         ((texture->orientation.y == KTX_ORIENT_Y_DOWN) ? 0 : 2) |
                                         ((texture->orientation.z == KTX_ORIENT_Z_OUT) ? 0 : 4));

    uint32_t valueSize = formatTraits.size;

    width = (width + layout.blockWidth - 1) / layout.blockWidth;
    height = (height + layout.blockHeight - 1) / layout.blockHeight;
    depth = (depth + layout.blockDepth - 1) / layout.blockDepth;

    Mipmaps mipmaps;
    ktxTexture_IterateLevelFaces(texture, imageIterator, &mipmaps);

    // compute the textureSize.
    ktx_uint64_t textureSize = 0;
    for (auto& [level, mipmap] : mipmaps.mipmaps)
    {
        for (auto& [face, faceData] : mipmap.faces)
        {
            textureSize += faceData.faceLodSize;
        }
    }

    // copy the data and repack into ordering assumed by VSG
    uint8_t* copiedData = static_cast<uint8_t*>(vsg::allocate(textureSize, vsg::ALLOCATOR_AFFINITY_DATA));

    ktx_uint64_t offset = 0;

    auto mipmapLayout = vsg::MipmapLayout::create(mipmaps.mipmaps.size());

    for (auto& [level, mipmap] : mipmaps.mipmaps)
    {
        const auto& firstFace = mipmap.faces.begin()->second;
        mipmapLayout->set(level, vsg::uivec4(firstFace.width, firstFace.height, firstFace.depth, static_cast<uint32_t>(offset)));
        for (auto& [face, faceData] : mipmap.faces)
        {
            std::memcpy(copiedData + offset, faceData.pixels, faceData.faceLodSize);
            offset += faceData.faceLodSize;
        }
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

    auto data = createImage(arrayDimensions, width, height, depth, copiedData, layout, valueSize, mipmapLayout);
    if (data)
    {
        // data->setObject("mipmapLayout", mipmapLayout);
    }
    return data;
}

vsg::ref_ptr<vsg::Data> ktx::Implementation::readKtx2(ktxTexture2* texture, const vsg::Path& filename) const
{
    uint32_t width = texture->baseWidth;
    uint32_t height = texture->baseHeight;
    uint32_t depth = texture->baseDepth;
    uint32_t numComponents = static_cast<uint32_t>(ktxTexture2_GetNumComponents(texture));

#if 0
    vsg::info("\nreadKtx(", texture, ", ", filename, ")");
    vsg::info("   vkFormat = ", texture->vkFormat);
    vsg::info("   pDfd = ", texture->pDfd);
    vsg::info("   supercompressionScheme = ", texture->supercompressionScheme, ", ", ktxSupercompressionSchemeString(texture->supercompressionScheme));
    vsg::info("   isVideo = ", texture->isVideo);
    vsg::info("   duration = ", texture->duration);
    vsg::info("   timescale = ", texture->timescale);
    vsg::info("   loopcount = ", texture->loopcount);
    vsg::info("   generateMipmaps = ", texture->generateMipmaps);
    vsg::info("   numLayers = ", texture->numLayers);
    vsg::info("   numFaces = ", texture->numFaces);
    vsg::info("   numLevels = ", texture->numLevels);
    vsg::info("   numComponents = ", numComponents);
#endif
    if (ktxTexture2_NeedsTranscoding(texture))
    {
        ktx_transcode_fmt_e fmt = KTX_TTF_RGBA32; // TODO value?
#if 1
        switch (numComponents)
        {
        case (1): fmt = KTX_TTF_BC4_R; break;
        case (2): fmt = KTX_TTF_BC5_RG; break;
        case (3): fmt = KTX_TTF_BC1_RGB; break; // KTX_TTF_ETC1_RGB?
        case (4):
        default: fmt = KTX_TTF_BC7_RGBA; break; // KTX_TTF_ETC2_RGBA?
        }
#endif

        ktx_transcode_flags transcodeFlags = 0;

        if (auto error_code = ktxTexture2_TranscodeBasis(texture, fmt, transcodeFlags); error_code != KTX_SUCCESS)
        {
            vsg::warn("vsgXchange::ktx : unabled to transcode ", filename, ", error_code = ", ktxErrorString(error_code));
            return {};
        }
    }

    // see ~/3rdParty/cesium-native/CesiumGltfReader/src/ImageDecoder.cpp

    if (texture->vkFormat == VK_FORMAT_UNDEFINED)
    {
        vsg::warn("vsgXchange::ktx : unabled to use ", filename, " due to incompatible vkFormat.");
        return {};
    }

    const auto numLayers = texture->numLayers;
    VkFormat format = static_cast<VkFormat>(texture->vkFormat);

    auto formatTraits = vsg::getFormatTraits(format);

    width = (width + formatTraits.blockWidth - 1) / formatTraits.blockWidth;
    height = (height + formatTraits.blockHeight - 1) / formatTraits.blockHeight;
    depth = (depth + formatTraits.blockDepth - 1) / formatTraits.blockDepth;

    uint32_t numMipMaps = texture->numLevels;

    vsg::Data::Properties layout;
    layout.format = format;

    layout.blockWidth = formatTraits.blockWidth;
    layout.blockHeight = formatTraits.blockHeight;
    layout.blockDepth = formatTraits.blockDepth;

    layout.mipLevels = numMipMaps;
    layout.origin = static_cast<uint8_t>(((texture->orientation.x == KTX_ORIENT_X_RIGHT) ? 0 : 1) |
                                         ((texture->orientation.y == KTX_ORIENT_Y_DOWN) ? 0 : 2) |
                                         ((texture->orientation.z == KTX_ORIENT_Z_OUT) ? 0 : 4));

    uint32_t valueSize = formatTraits.size;

    auto texture1 = ktxTexture(texture);
    Mipmaps mipmaps;
    mipmaps.maxMipMapLevel = numMipMaps;
    ktxTexture_IterateLevelFaces(texture1, imageIterator, &mipmaps);

    // compute the textureSize.
    ktx_uint64_t textureSize = 0;
    for (auto& [level, mipmap] : mipmaps.mipmaps)
    {
        for (auto& [face, faceData] : mipmap.faces)
        {
            textureSize += faceData.faceLodSize;
        }
    }

    // copy the data and repack into ordering assumed by VSG
    uint8_t* copiedData = static_cast<uint8_t*>(vsg::allocate(textureSize, vsg::ALLOCATOR_AFFINITY_DATA));

    ktx_uint64_t offset = 0;

    auto mipmapLayout = vsg::MipmapLayout::create(mipmaps.mipmaps.size());

    for (auto& [level, mipmap] : mipmaps.mipmaps)
    {
        const auto& firstFace = mipmap.faces.begin()->second;
        mipmapLayout->set(level, vsg::uivec4(firstFace.width, firstFace.height, firstFace.depth, static_cast<uint32_t>(offset)));
        for (auto& [face, faceData] : mipmap.faces)
        {

            std::memcpy(copiedData + offset, faceData.pixels, faceData.faceLodSize);
            offset += faceData.faceLodSize;
        }
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

    auto data = createImage(arrayDimensions, width, height, depth, copiedData, layout, valueSize, mipmapLayout);
    if (data)
    {
        //data->setValue("filename", std::string(filename));
        //data->setObject("mipmapLayout", mipmapLayout);
    }
    return data;
}

vsg::ref_ptr<vsg::Object> ktx::Implementation::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!vsg::compatibleExtension(filename, options, _supportedExtensions)) return {};

    vsg::Path filenameToUse = vsg::findFile(filename, options);
    if (!filenameToUse) return {};

    auto file = vsg::fopen(filenameToUse, "rb");
    if (!file) return {};

    KTX_error_code result = KTX_SUCCESS;
    vsg::ref_ptr<vsg::Data> data;
    try
    {
        if (vsg::fileExtension(filename) == ".ktx")
        {
            ktxTexture* texture = nullptr;
            result = ktxTexture_CreateFromStdioStream(file, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);
            if (result == KTX_SUCCESS) data = readKtx(texture, filename);
            if (texture) ktxTexture_Destroy(texture);
        }
        else
        {
            ktxTexture2* texture = nullptr;
            result = ktxTexture2_CreateFromStdioStream(file, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);
            if (result == KTX_SUCCESS) data = readKtx2(texture, filename);
            if (texture) ktxTexture2_Destroy(texture);
        }
    }
    catch (const vsg::Exception& ve)
    {
        std::cout << "ktx::read(" << filenameToUse << ") failed : " << ve.message << std::endl;
    }

    fclose(file);

    if (data)
    {
        return data;
    }
    else
    {
        std::cout << "ktx::read(" << filenameToUse << ") failed, result = " << ktxErrorString(result) << std::endl;
    }

    return {};
}

vsg::ref_ptr<vsg::Object> ktx::Implementation::read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!vsg::compatibleExtension(options, _supportedExtensions)) return {};

    std::string buffer(1 << 16, 0); // 64kB
    std::string input;

    while (!fin.eof())
    {
        fin.read(&buffer[0], buffer.size());
        const auto bytes_readed = fin.gcount();
        input.append(&buffer[0], bytes_readed);
    }

    if (ktxTexture2 * texture{nullptr}; ktxTexture2_CreateFromMemory((const ktx_uint8_t*)input.data(), input.size(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture) == KTX_SUCCESS)
    {
        vsg::ref_ptr<vsg::Data> data;
        try
        {
            data = readKtx2(texture, "");
        }
        catch (const vsg::Exception& ve)
        {
            std::cout << "ktx::read(std::istream&) failed : " << ve.message << std::endl;
        }

        ktxTexture2_Destroy(texture);

        return data;
    }

    return {};
}

vsg::ref_ptr<vsg::Object> ktx::Implementation::read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!vsg::compatibleExtension(options, _supportedExtensions)) return {};

    ktxTexture2* texture = nullptr;
    if (ktxTexture2_CreateFromMemory(ptr, size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture) == KTX_SUCCESS)
    {
        vsg::ref_ptr<vsg::Data> data;
        try
        {
            data = readKtx2(texture, "");
        }
        catch (const vsg::Exception& ve)
        {
            std::cout << "ktx::read(uint_8_t*, size_t) failed : " << ve.message << std::endl;
        }

        ktxTexture2_Destroy(texture);

        return data;
    }

    return {};
}

bool ktx::Implementation::getFeatures(Features& features) const
{
    for (auto& ext : _supportedExtensions)
    {
        features.extensionFeatureMap[ext] = static_cast<vsg::ReaderWriter::FeatureMask>(vsg::ReaderWriter::READ_FILENAME | vsg::ReaderWriter::READ_ISTREAM | vsg::ReaderWriter::READ_MEMORY);
    }
    return true;
}
