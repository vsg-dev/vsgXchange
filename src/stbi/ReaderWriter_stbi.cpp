#include "ReaderWriter_stbi.h"

#include <vsg/io/ObjectCache.h>
#include <vsg/io/FileSystem.h>

#include <cstring>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-function"

#include "stb_image.h"

#pragma GCC diagnostic pop

using namespace vsgXchange;


ReaderWriter_stbi::ReaderWriter_stbi()
    : _supportedExtensions{"jpg", "jpeg", "jpe", "png", "gif", "bmp", "tga", "psd"}
{
}

vsg::ref_ptr<vsg::Object> ReaderWriter_stbi::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> /*options*/) const
{
    if (!vsg::fileExists(filename))
        return {};

    if (const auto ext = vsg::fileExtension(filename); _supportedExtensions.count(ext) == 0)
        return {};

    int width, height, channels;
    if (const auto pixels = stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha); pixels)
    {
        auto vsg_data = vsg::ubvec4Array2D::create(width, height, vsg::Data::Layout{VK_FORMAT_R8G8B8A8_UNORM});
        std::memcpy(vsg_data->data(), pixels, vsg_data->dataSize());
        stbi_image_free(pixels);
        return vsg_data;
    }

    return {};
}

vsg::ref_ptr<vsg::Object> ReaderWriter_stbi::read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const
{
    if (_supportedExtensions.count(options->extensionHint) == 0)
        return {};

    std::string buffer(1<<16, 0); // 64kB
    std::string input;

    while (!fin.eof())
    {
        fin.read(&buffer[0], buffer.size());
        const auto bytes_readed = fin.gcount();
        input.append(&buffer[0], bytes_readed);
    }

    int width, height, channels;
    if (const auto pixels = stbi_load_from_memory(reinterpret_cast<stbi_uc*>(input.data()), static_cast<int>(input.size()), 
        &width, &height, &channels, STBI_rgb_alpha); pixels)
    {
        auto vsg_data = vsg::ubvec4Array2D::create(width, height, vsg::Data::Layout{VK_FORMAT_R8G8B8A8_UNORM});
        std::memcpy(vsg_data->data(), pixels, vsg_data->dataSize());
        stbi_image_free(pixels);
        return vsg_data;
    }

    return {};
}
