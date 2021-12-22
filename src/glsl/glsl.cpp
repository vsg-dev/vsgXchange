/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgXchange/glsl.h>

#include <vsg/state/ShaderStage.h>

using namespace vsgXchange;

glsl::glsl()
{
    add(".vert", VK_SHADER_STAGE_VERTEX_BIT);
    add(".tesc", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    add(".tese", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    add(".geom", VK_SHADER_STAGE_GEOMETRY_BIT);
    add(".frag", VK_SHADER_STAGE_FRAGMENT_BIT);
    add(".comp", VK_SHADER_STAGE_COMPUTE_BIT);
    add(".mesh", VK_SHADER_STAGE_MESH_BIT_NV);
    add(".task", VK_SHADER_STAGE_TASK_BIT_NV);
    add(".rgen", VK_SHADER_STAGE_RAYGEN_BIT_NV);
    add(".rint", VK_SHADER_STAGE_INTERSECTION_BIT_NV);
    add(".rahit", VK_SHADER_STAGE_ANY_HIT_BIT_NV);
    add(".rchit", VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
    add(".rmiss", VK_SHADER_STAGE_MISS_BIT_NV);
    add(".rcall", VK_SHADER_STAGE_CALLABLE_BIT_NV);
    add(".glsl", VK_SHADER_STAGE_ALL);
    add(".hlsl", VK_SHADER_STAGE_ALL);
}

void glsl::add(const std::string& ext, VkShaderStageFlagBits stage)
{
    extensionToStage[ext] = stage;
    stageToExtension[stage] = ext;
}

vsg::ref_ptr<vsg::Object> glsl::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    auto ext = vsg::lowerCaseFileExtension(filename);
    auto stage_itr = extensionToStage.find(ext);
    if (stage_itr != extensionToStage.end())
    {
        vsg::Path found_filename = vsg::findFile(filename, options);
        if (found_filename.empty()) return {};

        std::string source;

        std::ifstream fin(found_filename, std::ios::ate | std::ios::binary);
        size_t fileSize = fin.tellg();

        source.resize(fileSize);

        fin.seekg(0);
        fin.read(reinterpret_cast<char*>(source.data()), fileSize);
        fin.close();

        // handle any #includes in the source
        if (source.find("include") != std::string::npos)
        {
            source = vsg::insertIncludes(source, prependPathToOptionsIfRequired(found_filename, options));
        }

        auto sm = vsg::ShaderModule::create(source);

        if (stage_itr->second == VK_SHADER_STAGE_ALL)
        {
            return sm;
        }
        else
        {
            return vsg::ShaderStage::create(stage_itr->second, "main", sm);
        }

        return sm;
    }
    return {};
}

bool glsl::write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> /*options*/) const
{
    auto ext = vsg::lowerCaseFileExtension(filename);
    auto stage_itr = extensionToStage.find(ext);
    if (stage_itr != extensionToStage.end())
    {
        const vsg::ShaderStage* ss = dynamic_cast<const vsg::ShaderStage*>(object);
        const vsg::ShaderModule* sm = ss ? ss->module.get() : dynamic_cast<const vsg::ShaderModule*>(object);
        if (sm)
        {
            if (!sm->source.empty())
            {
                std::ofstream fout(filename);
                fout.write(sm->source.data(), sm->source.size());
                fout.close();
                return true;
            }
        }
    }
    return false;
}

bool glsl::getFeatures(Features& features) const
{
    for (auto& ext : extensionToStage)
    {
        features.extensionFeatureMap[ext.first] = static_cast<vsg::ReaderWriter::FeatureMask>(vsg::ReaderWriter::READ_FILENAME | vsg::ReaderWriter::WRITE_FILENAME);
    }
    return true;
}
