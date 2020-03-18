#include "ReaderWriter_glsl.h"

#include <vsg/vk/ShaderStage.h>

using namespace vsgXchange;

ReaderWriter_glsl::ReaderWriter_glsl()
{
    add("vert", VK_SHADER_STAGE_VERTEX_BIT);
    add("tesc", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    add("tese", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    add("geom", VK_SHADER_STAGE_GEOMETRY_BIT);
    add("frag", VK_SHADER_STAGE_FRAGMENT_BIT);
    add("comp", VK_SHADER_STAGE_COMPUTE_BIT);
    add("mesh", VK_SHADER_STAGE_MESH_BIT_NV);
    add("task", VK_SHADER_STAGE_TASK_BIT_NV);
    add("rgen", VK_SHADER_STAGE_RAYGEN_BIT_NV);
    add("rint", VK_SHADER_STAGE_INTERSECTION_BIT_NV);
    add("rahit", VK_SHADER_STAGE_ANY_HIT_BIT_NV);
    add("rchit", VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
    add("rmiss", VK_SHADER_STAGE_MISS_BIT_NV);
    add("rcall", VK_SHADER_STAGE_CALLABLE_BIT_NV);
    add("glsl", VK_SHADER_STAGE_ALL);
    add("hlsl", VK_SHADER_STAGE_ALL);
}

vsg::ref_ptr<vsg::Object> ReaderWriter_glsl::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options>  /*options*/) const
{
    auto ext = vsg::fileExtension(filename);
    auto stage_itr = extensionToStage.find(ext);
    if (stage_itr != extensionToStage.end() && vsg::fileExists(filename))
    {
        std::string source;

        std::ifstream fin(filename, std::ios::ate);
        size_t fileSize = fin.tellg();

        source.resize(fileSize);

        fin.seekg(0);
        fin.read(reinterpret_cast<char*>(source.data()), fileSize);
        fin.close();

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
    return vsg::ref_ptr<vsg::Object>();
}

bool ReaderWriter_glsl::write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options>  /*options*/) const
{
    auto ext = vsg::fileExtension(filename);
    auto stage_itr = extensionToStage.find(ext);
    if (stage_itr != extensionToStage.end())
    {
        const vsg::ShaderStage* ss = dynamic_cast<const vsg::ShaderStage*>(object);
        const vsg::ShaderModule* sm = ss ? ss->getShaderModule() : dynamic_cast<const vsg::ShaderModule*>(object);
        if (sm)
        {
            if (!sm->source().empty())
            {
                std::ofstream fout(filename);
                fout.write(sm->source().data(), sm->source().size());
                fout.close();
                return true;
            }
        }
    }
    return false;
}
