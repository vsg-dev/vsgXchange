/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include "ImageUtils.h"

#include <vsg/vk/CommandBuffer.h>

#include <vsg/core/Array2D.h>
#include <vsg/core/Array3D.h>

namespace osg2vsg
{

    VkFormat convertGLImageFormatToVulkan(GLenum dataType, GLenum pixelFormat)
    {
        using GLtoVkFormatMap = std::map<std::pair<GLenum, GLenum>, VkFormat>;
        static GLtoVkFormatMap s_GLtoVkFormatMap = {
            {{GL_UNSIGNED_BYTE, GL_ALPHA}, VK_FORMAT_R8_UNORM},
            {{GL_UNSIGNED_BYTE, GL_LUMINANCE}, VK_FORMAT_R8_UNORM},
            {{GL_UNSIGNED_BYTE, GL_LUMINANCE_ALPHA}, VK_FORMAT_R8G8_UNORM},
            {{GL_UNSIGNED_BYTE, GL_RGB}, VK_FORMAT_R8G8B8_UNORM},
            {{GL_UNSIGNED_BYTE, GL_RGBA}, VK_FORMAT_R8G8B8A8_UNORM}};

        auto itr = s_GLtoVkFormatMap.find({dataType, pixelFormat});
        if (itr != s_GLtoVkFormatMap.end())
        {
            std::cout << "convertGLImageFormatToVulkan(" << dataType << ", " << pixelFormat << ") vkFormat=" << itr->second << std::endl;
            return itr->second;
        }
        else
        {
            std::cout << "convertGLImageFormatToVulkan(" << dataType << ", " << pixelFormat << ") no match found." << std::endl;
            return VK_FORMAT_UNDEFINED;
        }
    }

    osg::ref_ptr<osg::Image> formatImage(const osg::Image* image, GLenum targetPixelFormat = GL_RGBA)
    {
        if (targetPixelFormat == image->getPixelFormat())
        {
            return const_cast<osg::Image*>(image);
        }

        osg::ref_ptr<osg::Image> new_image(new osg::Image);

        new_image->allocateImage(image->s(), image->t(), image->r(), targetPixelFormat, image->getDataType());

        int numBytesPerComponent = 1;
        unsigned char component_default[8] = {255, 0, 0, 0, 0, 0, 0, 0};
        switch (image->getDataType())
        {
        case (GL_BYTE):
            numBytesPerComponent = 1;
            *reinterpret_cast<char*>(component_default) = 127;
            break;
        case (GL_UNSIGNED_BYTE):
            numBytesPerComponent = 1;
            *reinterpret_cast<unsigned char*>(component_default) = 255;
            break;
        case (GL_SHORT):
            numBytesPerComponent = 2;
            *reinterpret_cast<short*>(component_default) = 32767;
            break;
        case (GL_UNSIGNED_SHORT):
            numBytesPerComponent = 2;
            *reinterpret_cast<unsigned short*>(component_default) = 65535;
            break;
        case (GL_INT):
            numBytesPerComponent = 4;
            *reinterpret_cast<int*>(component_default) = 2147483647;
            break;
        case (GL_UNSIGNED_INT):
            numBytesPerComponent = 4;
            *reinterpret_cast<unsigned int*>(component_default) = 4294967295;
            break;
        case (GL_FLOAT):
            numBytesPerComponent = 4;
            *reinterpret_cast<float*>(component_default) = 1.0f;
            break;
        case (GL_DOUBLE):
            numBytesPerComponent = 8;
            break;
            numBytesPerComponent = 8;
            *reinterpret_cast<double*>(component_default) = 1.0;
            break;
        default:
        {
            std::cout << "Warning: formateImage() DataType " << image->getDataType() << " not supprted." << std::endl;
            return {};
        }
        }

        int numComponents = 4;
        switch (targetPixelFormat)
        {
        case (GL_RED): numComponents = 1; break;
        case (GL_ALPHA): numComponents = 1; break;
        case (GL_LUMINANCE): numComponents = 1; break;
        case (GL_LUMINANCE_ALPHA): numComponents = 2; break;
        case (GL_RGB): numComponents = 3; break;
        case (GL_BGR): numComponents = 3; break;
        case (GL_RGBA): numComponents = 4; break;
        case (GL_BGRA): numComponents = 4; break;
        default:
        {
            std::cout << "Warning: formateImage() targetPixelFormat " << targetPixelFormat << " not supprted." << std::endl;
            return {};
        }
        }

        std::vector<int> componentOffset = {0, 1 * numBytesPerComponent, 2 * numBytesPerComponent, 3 * numBytesPerComponent};
        switch (image->getPixelFormat())
        {
        case (GL_RED):
            componentOffset = {0, -1, -1, -1};
            break;
        case (GL_ALPHA):
            componentOffset = {-1, -1, -1, 0};
            break;
        case (GL_LUMINANCE):
            componentOffset = {0, 0, 0, -1};
            break;
        case (GL_LUMINANCE_ALPHA):
            componentOffset = {0, 0, 0, numBytesPerComponent};
            break;
        case (GL_RGB):
            componentOffset = {0, numBytesPerComponent, numBytesPerComponent * 2, -1};
            break;
        case (GL_BGR):
            componentOffset = {numBytesPerComponent * 2, numBytesPerComponent, 0, -1};
            break;
        case (GL_RGBA):
            componentOffset = {0, numBytesPerComponent, numBytesPerComponent * 2, numBytesPerComponent * 3};
            break;
        case (GL_BGRA):
            componentOffset = {numBytesPerComponent * 2, numBytesPerComponent, 0, numBytesPerComponent * 3};
            break;
        default:
        {
            std::cout << "Warning: formateImage() source PixelFormat " << image->getPixelFormat() << " not supprted." << std::endl;
            return {};
        }
        }

        for (int r = 0; r < image->r(); ++r)
        {
            for (int t = 0; t < image->t(); ++t)
            {
                for (int s = 0; s < image->s(); ++s)
                {
                    const unsigned char* src = image->data(s, t, r);
                    unsigned char* dst = new_image->data(s, t, r);

                    for (int c = 0; c < numComponents; ++c)
                    {
                        int offset = componentOffset[c];
                        const unsigned char* component_src = (offset >= 0) ? (src + offset) : component_default;
                        for (int b = 0; b < numBytesPerComponent; ++b)
                        {
                            *(dst++) = *(component_src + b);
                        }
                    }
                }
            }
        }

        return new_image;
    }

    vsg::ref_ptr<vsg::Data> createWhiteTexture()
    {
        auto vsg_data = vsg::vec4Array2D::create(1, 1, vsg::Data::Layout{VK_FORMAT_R32G32B32A32_SFLOAT});
        for (auto& color : *vsg_data)
        {
            color = vsg::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        }
        return vsg_data;
    }

    vsg::ref_ptr<vsg::Data> convertCompressedImageToVsg(const osg::Image* image)
    {
        uint32_t blockSize = 0;
        vsg::Data::Layout layout;
        switch (image->getPixelFormat())
        {
        case (GL_COMPRESSED_ALPHA_ARB):
        case (GL_COMPRESSED_INTENSITY_ARB):
        case (GL_COMPRESSED_LUMINANCE_ALPHA_ARB):
        case (GL_COMPRESSED_LUMINANCE_ARB):
        case (GL_COMPRESSED_RGBA_ARB):
        case (GL_COMPRESSED_RGB_ARB):
            break;
        case (GL_COMPRESSED_RGB_S3TC_DXT1_EXT):
            blockSize = 64;
            layout.blockWidth = 4;
            layout.blockHeight = 4;
            layout.format = VK_FORMAT_BC1_RGB_UNORM_BLOCK;
            break;
        case (GL_COMPRESSED_RGBA_S3TC_DXT1_EXT):
            blockSize = 64;
            layout.blockWidth = 4;
            layout.blockHeight = 4;
            layout.format = VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
            break;
        case (GL_COMPRESSED_RGBA_S3TC_DXT3_EXT):
            blockSize = 128;
            layout.blockWidth = 4;
            layout.blockHeight = 4;
            layout.format = VK_FORMAT_BC2_UNORM_BLOCK;
            break;
        case (GL_COMPRESSED_RGBA_S3TC_DXT5_EXT):
            blockSize = 128;
            layout.blockWidth = 4;
            layout.blockHeight = 4;
            layout.format = VK_FORMAT_BC3_UNORM_BLOCK;
            break;
        case (GL_COMPRESSED_SIGNED_RED_RGTC1_EXT):
        case (GL_COMPRESSED_RED_RGTC1_EXT):
        case (GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT):
        case (GL_COMPRESSED_RED_GREEN_RGTC2_EXT):
        case (GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG):
        case (GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG):
        case (GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG):
        case (GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG):
        case (GL_ETC1_RGB8_OES):
        case (GL_COMPRESSED_RGB8_ETC2):
        case (GL_COMPRESSED_SRGB8_ETC2):
        case (GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2):
        case (GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2):
        case (GL_COMPRESSED_RGBA8_ETC2_EAC):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC):
        case (GL_COMPRESSED_R11_EAC):
        case (GL_COMPRESSED_SIGNED_R11_EAC):
        case (GL_COMPRESSED_RG11_EAC):
        case (GL_COMPRESSED_SIGNED_RG11_EAC):
#if OSG_MIN_VERSION_REQUIRED(3, 5, 8)
        case (GL_COMPRESSED_RGBA_ASTC_4x4_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_5x4_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_5x5_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_6x5_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_6x6_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_8x5_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_8x6_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_8x8_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_10x5_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_10x6_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_10x8_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_10x10_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_12x10_KHR):
        case (GL_COMPRESSED_RGBA_ASTC_12x12_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR):
        case (GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR):
#endif
            break;
        default:
            break;
        }

        if (blockSize == 0)
        {
            std::cout << "Compressed format not supported, falling back to white texture." << std::endl;
            return createWhiteTexture();
        }

        // stop the OSG image from deleting the image data as we are passing this on to the vsg Data object
        auto size = image->getTotalSizeInBytesIncludingMipmaps();
        uint8_t* data = new uint8_t[size];
        memcpy(data, image->data(), size);

        layout.maxNumMipmaps = image->getNumMipmapLevels();

        uint32_t width = image->s() / layout.blockWidth;
        uint32_t height = image->t() / layout.blockHeight;
        uint32_t depth = image->r() / layout.blockDepth;

        layout.origin = (image->getOrigin() == osg::Image::BOTTOM_LEFT) ? vsg::BOTTOM_LEFT : vsg::TOP_LEFT;

        if (blockSize == 64)
        {
            if (image->r() == 1)
            {
                return vsg::block64Array2D::create(width, height, reinterpret_cast<vsg::block64*>(data), layout);
            }
            else
            {
                return vsg::block64Array3D::create(width, height, depth, reinterpret_cast<vsg::block64*>(data), layout);
            }
        }
        else
        {
            if (image->r() == 1)
            {
                return vsg::block128Array2D::create(width, height, reinterpret_cast<vsg::block128*>(data), layout);
            }
            else
            {
                return vsg::block128Array3D::create(width, height, depth, reinterpret_cast<vsg::block128*>(data), layout);
            }
        }

        return {};
    }

    template<typename T>
    vsg::ref_ptr<vsg::Data> create(osg::ref_ptr<osg::Image> image, VkFormat format)
    {
        vsg::ref_ptr<vsg::Data> vsg_data;
        if (image->r() == 1)
        {
            vsg_data = vsg::Array2D<T>::create(image->s(), image->t(), reinterpret_cast<T*>(image->data()), vsg::Data::Layout{format});
        }
        else
        {
            vsg_data = vsg::Array3D<T>::create(image->s(), image->t(), image->r(), reinterpret_cast<T*>(image->data()), vsg::Data::Layout{format});
        }

        return vsg_data;
    }

    vsg::ref_ptr<vsg::Data> convertToVsg(const osg::Image* image, bool mapRGBtoRGBAHint)
    {
        if (!image)
        {
            return createWhiteTexture();
        }

        if (image->isCompressed())
        {
            return convertCompressedImageToVsg(image);
        }

        int numComponents = 4;
        osg::ref_ptr<osg::Image> new_image;

        switch (image->getPixelFormat())
        {
        case (GL_RED):
        case (GL_LUMINANCE):
        case (GL_ALPHA):
            numComponents = 1;
            new_image = const_cast<osg::Image*>(image);
            break;

        case (GL_LUMINANCE_ALPHA):
            numComponents = 2;
            new_image = const_cast<osg::Image*>(image);
            break;

        case (GL_RGB):
            if (mapRGBtoRGBAHint)
            {
                numComponents = 4;
                new_image = formatImage(image, GL_RGBA);
            }
            else
            {
                numComponents = 3;
                new_image = const_cast<osg::Image*>(image);
            }
            break;

        case (GL_BGR):
            if (mapRGBtoRGBAHint)
            {
                numComponents = 4;
                new_image = formatImage(image, GL_RGBA);
            }
            else
            {
                numComponents = 3;
                new_image = formatImage(image, GL_RGB);
            }
            break;

        case (GL_RGBA):
            numComponents = 4;
            new_image = const_cast<osg::Image*>(image);
            break;

        case (GL_BGRA):
            numComponents = 4;
            new_image = formatImage(image, GL_RGBA);
            break;

        default:
            std::cout << "Warning: convertToVsg(osg::Image*) does not support image->getPixelFormat() == " << std::hex << image->getPixelFormat() << std::endl;
            return {};
        }

        if (!new_image)
        {
            std::cout << "Warning: convertToVsg(osg::Image*) unable to create vsg::Data." << std::endl;
            return {};
        }

        // we want to pass ownership of the new_image data onto th vsg_image so reset the allocation mode on the image to prevent deletetion.
        new_image->setAllocationMode(osg::Image::NO_DELETE);

        vsg::ref_ptr<vsg::Data> vsg_data;

        switch (numComponents)
        {
        case (1):
            if (image->getDataType() == GL_UNSIGNED_BYTE)
                vsg_data = create<uint8_t>(new_image, VK_FORMAT_R8_UNORM);
            else if (image->getDataType() == GL_UNSIGNED_SHORT)
                vsg_data = create<uint16_t>(new_image, VK_FORMAT_R16_UNORM);
            else if (image->getDataType() == GL_UNSIGNED_INT)
                vsg_data = create<uint32_t>(new_image, VK_FORMAT_R32_UINT);
            else if (image->getDataType() == GL_FLOAT)
                vsg_data = create<float>(new_image, VK_FORMAT_R32_SFLOAT);
            else if (image->getDataType() == GL_DOUBLE)
                vsg_data = create<double>(new_image, VK_FORMAT_R64_SFLOAT);
            break;
        case (2):
            if (image->getDataType() == GL_UNSIGNED_BYTE)
                vsg_data = create<vsg::ubvec2>(new_image, VK_FORMAT_R8G8_UNORM);
            else if (image->getDataType() == GL_UNSIGNED_SHORT)
                vsg_data = create<vsg::usvec2>(new_image, VK_FORMAT_R16G16_UNORM);
            else if (image->getDataType() == GL_UNSIGNED_INT)
                vsg_data = create<vsg::uivec2>(new_image, VK_FORMAT_R32G32_UINT);
            else if (image->getDataType() == GL_FLOAT)
                vsg_data = create<vsg::vec2>(new_image, VK_FORMAT_R32G32_SFLOAT);
            else if (image->getDataType() == GL_DOUBLE)
                vsg_data = create<vsg::dvec2>(new_image, VK_FORMAT_R64G64_SFLOAT);
            break;
        case (3):
            if (image->getDataType() == GL_UNSIGNED_BYTE)
                vsg_data = create<vsg::ubvec3>(new_image, VK_FORMAT_R8G8B8_UNORM);
            else if (image->getDataType() == GL_UNSIGNED_SHORT)
                vsg_data = create<vsg::usvec3>(new_image, VK_FORMAT_R16G16B16_UNORM);
            else if (image->getDataType() == GL_UNSIGNED_INT)
                vsg_data = create<vsg::uivec3>(new_image, VK_FORMAT_R32G32B32_UINT);
            else if (image->getDataType() == GL_FLOAT)
                vsg_data = create<vsg::vec3>(new_image, VK_FORMAT_R32G32B32_SFLOAT);
            else if (image->getDataType() == GL_DOUBLE)
                vsg_data = create<vsg::dvec3>(new_image, VK_FORMAT_R64G64B64_SFLOAT);
            break;
        case (4):
            if (image->getDataType() == GL_UNSIGNED_BYTE)
                vsg_data = create<vsg::ubvec4>(new_image, VK_FORMAT_R8G8B8A8_UNORM);
            else if (image->getDataType() == GL_UNSIGNED_SHORT)
                vsg_data = create<vsg::usvec4>(new_image, VK_FORMAT_R16G16B16A16_UNORM);
            else if (image->getDataType() == GL_UNSIGNED_INT)
                vsg_data = create<vsg::uivec4>(new_image, VK_FORMAT_R32G32B32A32_UINT);
            else if (image->getDataType() == GL_FLOAT)
                vsg_data = create<vsg::vec4>(new_image, VK_FORMAT_R32G32B32A32_SFLOAT);
            else if (image->getDataType() == GL_DOUBLE)
                vsg_data = create<vsg::dvec4>(new_image, VK_FORMAT_R64G64B64A64_SFLOAT);
            break;
        }

        vsg::Data::Layout& layout = vsg_data->getLayout();
        layout.maxNumMipmaps = image->getNumMipmapLevels();
        layout.origin = (image->getOrigin() == osg::Image::BOTTOM_LEFT) ? vsg::BOTTOM_LEFT : vsg::TOP_LEFT;

        return vsg_data;
    }

} // namespace osg2vsg
