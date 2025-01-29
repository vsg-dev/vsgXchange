/* <editor-fold desc="MIT License">

Copyright(c) 2021 André Normann & Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgXchange/images.h>

#include <vsg/io/FileSystem.h>
#include <vsg/io/Logger.h>
#include <vsg/utils/CoordinateSpace.h>
#include <vsg/utils/CommandLine.h>

#include <cstring>

#include <iostream>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#if defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wunused-variable"
#    pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#    pragma GCC diagnostic ignored "-Wsign-compare"
#    pragma GCC diagnostic ignored "-Wunused-function"
#endif

void* cpp_malloc(size_t size)
{
    return vsg::allocate(size, vsg::ALLOCATOR_AFFINITY_DATA);
}

void cpp_free(void* ptr)
{
    vsg::deallocate(ptr);
}

void* cpp_realloc_sized(void* old_ptr, size_t old_size, size_t new_size)
{
    if (!old_ptr) return cpp_malloc(new_size);

    if (old_size >= new_size) return old_ptr;

    void* new_ptr = vsg::allocate(new_size);

    std::memcpy(new_ptr, old_ptr, old_size);

    vsg::deallocate(old_ptr);

    return new_ptr;
}

// override the stb memory allocation to make it compatible with vsg::Array* use of standard C++ new/delete.
#define STBI_MALLOC(sz) cpp_malloc(sz)
#define STBI_REALLOC(p, newsiz) ERROR_SHOULD_NEVER_BE_CALLED
#define STBI_REALLOC_SIZED(p, oldsz, newsz) cpp_realloc_sized(p, oldsz, newsz)
#define STBI_FREE(p) cpp_free(p)

#include "stb_image.h"
#include "stb_image_write.h"

#if defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif

using namespace vsgXchange;

static void writeToStream(void* context, void* data, int size)
{
    reinterpret_cast<std::ostream*>(context)->write(reinterpret_cast<const char*>(data), size);
}

static void process_image_format(vsg::ref_ptr<const vsg::Options> options, VkFormat& format)
{
    if (!options) return;

    vsg::CoordinateSpace coordinateSpace;
    if (options->getValue(stbi::image_format, coordinateSpace))
    {
        if (coordinateSpace==vsg::CoordinateSpace::sRGB) format = vsg::uNorm_to_sRGB(format);
        else if (coordinateSpace==vsg::CoordinateSpace::LINEAR) format = vsg::sRGB_to_uNorm(format);
    }
}

// if the data is in BGR or BGRA form create a copy that is reformated into RGB or RGBA respectively
static std::pair<int, vsg::ref_ptr<const vsg::Data>> reformatForWriting(const vsg::Data* data, const vsg::Path& filename)
{
    int num_components = 0;
    vsg::ref_ptr<const vsg::Data> local_data;
    switch (data->properties.format)
    {
    case (VK_FORMAT_R8_UNORM):
        num_components = 1;
        break;
    case (VK_FORMAT_R8G8_UNORM):
        num_components = 2;
        break;
    case (VK_FORMAT_R8G8B8_SRGB):
    case (VK_FORMAT_R8G8B8_UNORM):
        num_components = 3;
        break;
    case (VK_FORMAT_R8G8B8A8_SRGB):
    case (VK_FORMAT_R8G8B8A8_UNORM):
        num_components = 4;
        break;
    case (VK_FORMAT_B8G8R8_SRGB):
    case (VK_FORMAT_B8G8R8_UNORM): {
        auto dest_data = vsg::ubvec3Array2D::create(data->width(), data->height(), vsg::Data::Properties{VK_FORMAT_R8G8B8_UNORM});
        auto src_ptr = static_cast<const vsg::ubvec3*>(data->dataPointer());
        for (auto& dest : *dest_data)
        {
            auto& src = *(src_ptr++);
            dest.set(src[2], src[1], src[0]);
        }

        num_components = 3;
        local_data = dest_data;
        break;
    }
    case (VK_FORMAT_B8G8R8A8_SRGB):
    case (VK_FORMAT_B8G8R8A8_UNORM): {
        auto dest_data = vsg::ubvec4Array2D::create(data->width(), data->height(), vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM});
        auto src_ptr = static_cast<const vsg::ubvec4*>(data->dataPointer());
        for (auto& dest : *dest_data)
        {
            auto& src = *(src_ptr++);
            dest.set(src[2], src[1], src[0], src[3]);
        }

        num_components = 4;
        local_data = dest_data;
        break;
    }
    default:
        vsg::warn("stbi::write(", data->className(), ", ", filename, ") data format VkFormat(", data->properties.format, ") not supported.");
        return {0, {}};
    }
    return {num_components, local_data};
}

stbi::stbi() :
    _supportedExtensions{".jpg", ".jpeg", ".jpe", ".png", ".gif", ".bmp", ".tga", ".psd", ".pgm", ".ppm"}
{
}

bool stbi::getFeatures(Features& features) const
{
    vsg::ReaderWriter::FeatureMask read_mask = static_cast<vsg::ReaderWriter::FeatureMask>(READ_FILENAME | READ_ISTREAM | READ_MEMORY);
    vsg::ReaderWriter::FeatureMask read_write_mask = static_cast<vsg::ReaderWriter::FeatureMask>(WRITE_FILENAME | WRITE_OSTREAM | READ_FILENAME | READ_ISTREAM | READ_MEMORY);

    features.extensionFeatureMap[".png"] = read_write_mask;
    features.extensionFeatureMap[".bmp"] = read_write_mask;
    features.extensionFeatureMap[".tga"] = read_write_mask;
    features.extensionFeatureMap[".jpg"] = features.extensionFeatureMap[".jpeg"] = features.extensionFeatureMap[".jpe"] = read_write_mask;

    features.extensionFeatureMap[".psd"] = read_mask;
    features.extensionFeatureMap[".pgm"] = read_mask;
    features.extensionFeatureMap[".ppm"] = read_mask;

    features.optionNameTypeMap[stbi::jpeg_quality] = vsg::type_name<int>();
    features.optionNameTypeMap[stbi::image_format] = vsg::type_name<vsg::CoordinateSpace>();

    return true;
}

bool stbi::readOptions(vsg::Options& options, vsg::CommandLine& arguments) const
{
    bool result = arguments.readAndAssign<int>(stbi::jpeg_quality, &options);
    result = arguments.readAndAssign<vsg::CoordinateSpace>(stbi::image_format, &options) | result;
    return result;
}

vsg::ref_ptr<vsg::Object> stbi::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!vsg::compatibleExtension(filename, options, _supportedExtensions)) return {};

    vsg::Path filenameToUse = findFile(filename, options);
    if (!filenameToUse) return {};

    int width, height, channels;

    auto file = vsg::fopen(filenameToUse, "rb");
    if (!file) return {};

    const auto pixels = stbi_load_from_file(file, &width, &height, &channels, STBI_rgb_alpha);

    fclose(file);

    if (pixels)
    {
        auto vsg_data = vsg::ubvec4Array2D::create(width, height, reinterpret_cast<vsg::ubvec4*>(pixels), vsg::Data::Properties{VK_FORMAT_R8G8B8A8_SRGB});
        process_image_format(options, vsg_data->properties.format);
        return vsg_data;
    }

    return {};
}

vsg::ref_ptr<vsg::Object> stbi::read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const
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

    int width, height, channels;
    const auto pixels = stbi_load_from_memory(reinterpret_cast<stbi_uc*>(input.data()), static_cast<int>(input.size()), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels)
    {
        auto vsg_data = vsg::ubvec4Array2D::create(width, height, reinterpret_cast<vsg::ubvec4*>(pixels), vsg::Data::Properties{VK_FORMAT_R8G8B8A8_SRGB});
        process_image_format(options, vsg_data->properties.format);
        return vsg_data;
    }

    return {};
}

vsg::ref_ptr<vsg::Object> stbi::read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!vsg::compatibleExtension(options, _supportedExtensions)) return {};

    int width, height, channels;
    const auto pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(ptr), static_cast<int>(size), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels)
    {
        auto vsg_data = vsg::ubvec4Array2D::create(width, height, reinterpret_cast<vsg::ubvec4*>(pixels), vsg::Data::Properties{VK_FORMAT_R8G8B8A8_SRGB});
        process_image_format(options, vsg_data->properties.format);
        return vsg_data;
    }

    return {};
}

bool stbi::write(const vsg::Object* object, std::ostream& stream, vsg::ref_ptr<const vsg::Options> options) const
{
    const auto ext = options->extensionHint;
    if (_supportedExtensions.count(ext) == 0)
    {
        return false;
    }

    auto data = object->cast<vsg::Data>();
    if (!data) return false;

    // if we need to swizzle the image we'll need to allocate a temporary vsg::Data to store the swizzled data
    auto [num_components, local_data] = reformatForWriting(data, {});
    if (num_components == 0) return false;
    if (local_data) data = local_data.get();

    int result = 0;
    if (ext == ".png")
    {
        result = stbi_write_png_to_func(&writeToStream, &stream, data->width(), data->height(), num_components, data->dataPointer(), data->properties.stride * data->width());
    }
    else if (ext == ".bmp")
    {
        result = stbi_write_bmp_to_func(&writeToStream, &stream, data->width(), data->height(), num_components, data->dataPointer());
    }
    else if (ext == ".tga")
    {
        result = stbi_write_tga_to_func(&writeToStream, &stream, data->width(), data->height(), num_components, data->dataPointer());
    }
    else if (ext == ".jpg" || ext == ".jpeg" || ext == ".jpe")
    {
        int quality = 100;
        if (options)
        {
            options->getValue(stbi::jpeg_quality, quality);
        }
        result = stbi_write_jpg_to_func(&writeToStream, &stream, data->width(), data->height(), num_components, data->dataPointer(), quality);
    }
    return result == 1;
}

bool stbi::write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    const auto ext = vsg::lowerCaseFileExtension(filename);
    if (_supportedExtensions.count(ext) == 0)
    {
        return false;
    }

    auto data = object->cast<vsg::Data>();
    if (!data) return false;

    // if we need to swizzle the image we'll need to allocate a temporary vsg::Data to store the swizzled data
    auto [num_components, local_data] = reformatForWriting(data, filename);
    if (num_components == 0) return false;
    if (local_data) data = local_data.get();

    // convert to utf8 std::string
    std::string filename_str = filename.string();
    int result = 0;
    if (ext == ".png")
    {
        result = stbi_write_png(filename_str.c_str(), data->width(), data->height(), num_components, data->dataPointer(), data->properties.stride * data->width());
    }
    else if (ext == ".bmp")
    {
        result = stbi_write_bmp(filename_str.c_str(), data->width(), data->height(), num_components, data->dataPointer());
    }
    else if (ext == ".tga")
    {
        result = stbi_write_tga(filename_str.c_str(), data->width(), data->height(), num_components, data->dataPointer());
    }
    else if (ext == ".jpg" || ext == ".jpeg" || ext == ".jpe")
    {
        int quality = 100;
        if (options)
        {
            options->getValue(stbi::jpeg_quality, quality);
        }
        result = stbi_write_jpg(filename_str.c_str(), data->width(), data->height(), num_components, data->dataPointer(), quality);
    }

    return result == 1;
}
