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

#include <iostream>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION

#if defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wunused-variable"
#    pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#    pragma GCC diagnostic ignored "-Wsign-compare"
#    pragma GCC diagnostic ignored "-Wunused-function"
#endif

void* cpp_malloc(size_t size)
{
    return new uint8_t[size];
}

void* cpp_realloc_sized(void* old_ptr, size_t old_size, size_t new_size)
{
    if (!old_ptr) return cpp_malloc(new_size);

    if (old_size >= new_size) return old_ptr;

    uint8_t* new_ptr = new uint8_t[new_size];

    std::memcpy(new_ptr, old_ptr, old_size);

    delete[](uint8_t*)(old_ptr);

    return new_ptr;
}

void cpp_free(void* ptr)
{
    delete[](uint8_t*)(ptr);
}

// override the stb memory allocation to make it compatible with vsg::Array* use of standard C++ new/delete.
#define STBI_MALLOC(sz) cpp_malloc(sz)
#define STBI_REALLOC(p, newsiz) ERROR_SHOULD_NEVER_BE_CALLED
#define STBI_REALLOC_SIZED(p, oldsz, newsz) cpp_realloc_sized(p, oldsz, newsz)
#define STBI_FREE(p) cpp_free(p)

#include "stb_image.h"

#if defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif

using namespace vsgXchange;

stbi::stbi() :
    _supportedExtensions{".jpg", ".jpeg", ".jpe", ".png", ".gif", ".bmp", ".tga", ".psd", ".pgm", ".ppm"}
{
}

vsg::ref_ptr<vsg::Object> stbi::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    if (const auto ext = vsg::lowerCaseFileExtension(filename); _supportedExtensions.count(ext) == 0)
    {
        return {};
    }

    vsg::Path filenameToUse = findFile(filename, options);
    if (filenameToUse.empty()) return {};

    int width, height, channels;
    const auto pixels = stbi_load(filenameToUse.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels)
    {
        auto vsg_data = vsg::ubvec4Array2D::create(width, height, reinterpret_cast<vsg::ubvec4*>(pixels), vsg::Data::Layout{VK_FORMAT_R8G8B8A8_UNORM});

        return vsg_data;
    }

    return {};
}

vsg::ref_ptr<vsg::Object> stbi::read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const
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

    int width, height, channels;
    const auto pixels = stbi_load_from_memory(reinterpret_cast<stbi_uc*>(input.data()), static_cast<int>(input.size()), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels)
    {
        auto vsg_data = vsg::ubvec4Array2D::create(width, height, reinterpret_cast<vsg::ubvec4*>(pixels), vsg::Data::Layout{VK_FORMAT_R8G8B8A8_UNORM});
        return vsg_data;
    }

    return {};
}

vsg::ref_ptr<vsg::Object> stbi::read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!options || _supportedExtensions.count(options->extensionHint) == 0)
        return {};

    int width, height, channels;
    const auto pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(ptr), static_cast<int>(size), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels)
    {
        auto vsg_data = vsg::ubvec4Array2D::create(width, height, reinterpret_cast<vsg::ubvec4*>(pixels), vsg::Data::Layout{VK_FORMAT_R8G8B8A8_UNORM});
        return vsg_data;
    }

    return {};
}

bool stbi::getFeatures(Features& features) const
{
    for (auto& ext : _supportedExtensions)
    {
        features.extensionFeatureMap[ext] = static_cast<vsg::ReaderWriter::FeatureMask>(vsg::ReaderWriter::READ_FILENAME | vsg::ReaderWriter::READ_ISTREAM | vsg::ReaderWriter::READ_MEMORY);
    }
    return true;
}
