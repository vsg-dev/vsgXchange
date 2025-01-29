/* <editor-fold desc="MIT License">

Copyright(c) 2021 André Normann & Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgXchange/models.h>

#include "SceneConverter.h"

using namespace vsgXchange;

class assimp::Implementation
{
public:
    Implementation();

    vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const;
    vsg::ref_ptr<vsg::Object> read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options = {}) const;
    vsg::ref_ptr<vsg::Object> read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options = {}) const;

    const uint32_t _importFlags;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// assimp ReaderWriter facade
//
assimp::assimp() :
    _implementation(new assimp::Implementation())
{
    vsg::debug("ASSIMP_VERSION_MAJOR ", ASSIMP_VERSION_MAJOR);
    vsg::debug("ASSIMP_VERSION_MINOR ", ASSIMP_VERSION_MINOR);
    vsg::debug("ASSIMP_VERSION_PATCH ", ASSIMP_VERSION_PATCH);
}
assimp::~assimp()
{
    delete _implementation;
}
vsg::ref_ptr<vsg::Object> assimp::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    return _implementation->read(filename, options);
}

vsg::ref_ptr<vsg::Object> assimp::read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const
{
    return _implementation->read(fin, options);
}

vsg::ref_ptr<vsg::Object> assimp::read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options) const
{
    return _implementation->read(ptr, size, options);
}

bool assimp::getFeatures(Features& features) const
{
    std::string suported_extensions;
    Assimp::Importer importer;
    importer.GetExtensionList(suported_extensions);

    vsg::ReaderWriter::FeatureMask supported_features = static_cast<vsg::ReaderWriter::FeatureMask>(vsg::ReaderWriter::READ_FILENAME | vsg::ReaderWriter::READ_ISTREAM | vsg::ReaderWriter::READ_MEMORY);

    std::string::size_type start = 1; // skip *
    std::string::size_type semicolon = suported_extensions.find(';', start);
    while (semicolon != std::string::npos)
    {
        features.extensionFeatureMap[suported_extensions.substr(start, semicolon - start)] = supported_features;
        start = semicolon + 2;
        semicolon = suported_extensions.find(';', start);
    }
    features.extensionFeatureMap[suported_extensions.substr(start, std::string::npos)] = supported_features;

    // enumerate the supported vsg::Options::setValue(str, value) options
    features.optionNameTypeMap[assimp::generate_smooth_normals] = vsg::type_name<bool>();
    features.optionNameTypeMap[assimp::generate_sharp_normals] = vsg::type_name<bool>();
    features.optionNameTypeMap[assimp::crease_angle] = vsg::type_name<float>();
    features.optionNameTypeMap[assimp::two_sided] = vsg::type_name<bool>();
    features.optionNameTypeMap[assimp::discard_empty_nodes] = vsg::type_name<bool>();
    features.optionNameTypeMap[assimp::print_assimp] = vsg::type_name<int>();
    features.optionNameTypeMap[assimp::external_textures] = vsg::type_name<bool>();
    features.optionNameTypeMap[assimp::external_texture_format] = vsg::type_name<TextureFormat>();
    features.optionNameTypeMap[assimp::culling] = vsg::type_name<bool>();
    features.optionNameTypeMap[assimp::vertex_color_space] = vsg::type_name<vsg::CoordinateSpace>();
    features.optionNameTypeMap[assimp::material_color_space] = vsg::type_name<vsg::CoordinateSpace>();

    return true;
}

bool assimp::readOptions(vsg::Options& options, vsg::CommandLine& arguments) const
{
    bool result = arguments.readAndAssign<bool>(assimp::generate_smooth_normals, &options);
    result = arguments.readAndAssign<bool>(assimp::generate_sharp_normals, &options) || result;
    result = arguments.readAndAssign<float>(assimp::crease_angle, &options) || result;
    result = arguments.readAndAssign<bool>(assimp::two_sided, &options) || result;
    result = arguments.readAndAssign<bool>(assimp::discard_empty_nodes, &options) || result;
    result = arguments.readAndAssign<int>(assimp::print_assimp, &options) || result;
    result = arguments.readAndAssign<bool>(assimp::external_textures, &options) || result;
    result = arguments.readAndAssign<TextureFormat>(assimp::external_texture_format, &options) || result;
    result = arguments.readAndAssign<bool>(assimp::culling, &options) || result;
    result = arguments.readAndAssign<vsg::CoordinateSpace>(assimp::vertex_color_space, &options) || result;
    result = arguments.readAndAssign<vsg::CoordinateSpace>(assimp::material_color_space, &options) || result;

    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// assimp ReaderWriter implementation
//
assimp::Implementation::Implementation() :
    _importFlags{aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_OptimizeMeshes | aiProcess_SortByPType | aiProcess_ImproveCacheLocality | aiProcess_GenUVCoords | aiProcess_PopulateArmatureData}
{
}

vsg::ref_ptr<vsg::Object> assimp::Implementation::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    Assimp::Importer importer;
    vsg::Path ext = (options && options->extensionHint) ? options->extensionHint : vsg::lowerCaseFileExtension(filename);

    if (importer.IsExtensionSupported(ext.string()))
    {
        vsg::Path filenameToUse = vsg::findFile(filename, options);
        if (!filenameToUse) return {};

        uint32_t flags = _importFlags;
        if (vsg::value<bool>(false, assimp::generate_smooth_normals, options))
        {
            importer.SetPropertyFloat(AI_CONFIG_PP_CT_MAX_SMOOTHING_ANGLE, vsg::value<float>(80.0f, assimp::crease_angle, options));
            flags |= aiProcess_GenSmoothNormals;
        }
        else if (vsg::value<bool>(false, assimp::generate_sharp_normals, options))
        {
            flags |= aiProcess_GenNormals;
        }

        if (auto scene = importer.ReadFile(filenameToUse.string(), flags); scene)
        {
            auto opt = vsg::clone(options);
            opt->paths.insert(opt->paths.begin(), vsg::filePath(filenameToUse));

            SceneConverter converter;
            converter.filename = filename;

            auto root = converter.visit(scene, opt, ext);
            if (root)
            {
                if (converter.externalTextures && converter.externalObjects && !converter.externalObjects->entries.empty())
                    root->setObject("external", converter.externalObjects);
            }

            return root;
        }
        else
        {
            vsg::warn("Failed to load file: ", filename, '\n', importer.GetErrorString());
        }
    }

    return {};
}

vsg::ref_ptr<vsg::Object> assimp::Implementation::read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!options || !options->extensionHint) return {};

    Assimp::Importer importer;
    if (importer.IsExtensionSupported(options->extensionHint.string()))
    {
        std::string buffer(1 << 16, 0); // 64kB
        std::string input;

        while (!fin.eof())
        {
            fin.read(&buffer[0], buffer.size());
            const auto bytes_readed = fin.gcount();
            input.append(&buffer[0], bytes_readed);
        }

        if (auto scene = importer.ReadFileFromMemory(input.data(), input.size(), _importFlags); scene)
        {
            SceneConverter converter;
            return converter.visit(scene, options, options->extensionHint);
        }
        else
        {
            vsg::warn("Failed to load file from stream: ", importer.GetErrorString());
        }
    }

    return {};
}

vsg::ref_ptr<vsg::Object> assimp::Implementation::read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!options || !options->extensionHint) return {};

    Assimp::Importer importer;
    if (importer.IsExtensionSupported(options->extensionHint.string()))
    {
        if (auto scene = importer.ReadFileFromMemory(ptr, size, _importFlags); scene)
        {
            SceneConverter converter;
            return converter.visit(scene, options, options->extensionHint);
        }
        else
        {
            vsg::warn("Failed to load file from memory: ", importer.GetErrorString());
        }
    }
    return {};
}
