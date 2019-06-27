#include <vsgXchange/ShaderCompiler.h>

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>

#include "glsllang/ResourceLimits.h"

#include <algorithm>
#include <iomanip>
#include <iostream>

using namespace vsgXchange;

#if 1
#define DEBUG_OUTPUT std::cout
#else
#define DEBUG_OUTPUT if (false) std::cout
#endif
#define INFO_OUTPUT std::cout


std::string debugFormatShaderSource(const std::string& source)
{
    std::istringstream iss(source);
    std::ostringstream oss;

    uint32_t linecount = 1;

    for (std::string line; std::getline(iss, line);)
    {
        oss << std::setw(4) << std::setfill(' ') << linecount << ": " << line << "\n";
        linecount++;
    }
    return oss.str();
}


ShaderCompiler::ShaderCompiler(vsg::Allocator* allocator):
    vsg::Object(allocator)
{
    glslang::InitializeProcess();
}

ShaderCompiler::~ShaderCompiler()
{
    glslang::FinalizeProcess();
}

bool ShaderCompiler::compile(vsg::ShaderStages& shaders)
{
    auto getFriendlyNameForShader = [](const vsg::ref_ptr<vsg::ShaderStage>& vsg_shader)
    {
        switch (vsg_shader->getShaderStageFlagBits())
        {
            case(VK_SHADER_STAGE_VERTEX_BIT): return "Vertex Shader";
            case(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT): return "Tessellation Control Shader";
            case(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT): return "Tessellation Evaluation Shader";
            case(VK_SHADER_STAGE_GEOMETRY_BIT): return "Geometry Shader";
            case(VK_SHADER_STAGE_FRAGMENT_BIT): return "Fragment Shader";
            case(VK_SHADER_STAGE_COMPUTE_BIT): return "Compute Shader";
    #ifdef VK_SHADER_STAGE_RAYGEN_BIT_NV
            case(VK_SHADER_STAGE_RAYGEN_BIT_NV): return "RayGen Shader";
            case(VK_SHADER_STAGE_ANY_HIT_BIT_NV): return "Any Hit Shader";
            case(VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV): return "Closest Hit Shader";
            case(VK_SHADER_STAGE_MISS_BIT_NV): return "Miss Shader";
            case(VK_SHADER_STAGE_INTERSECTION_BIT_NV): return "Intersection Shader";
            case(VK_SHADER_STAGE_CALLABLE_BIT_NV): return "Callable Shader";
            case(VK_SHADER_STAGE_TASK_BIT_NV): return "Task Shader";
            case(VK_SHADER_STAGE_MESH_BIT_NV): return "Mesh Shader";
    #endif
            default: return "Unkown Shader Type";
        }
        return "";
    };

    using StageShaderMap = std::map<EShLanguage, vsg::ref_ptr<vsg::ShaderStage>>;
    using TShaders = std::list<std::unique_ptr<glslang::TShader>>;
    TShaders tshaders;

    TBuiltInResource builtInResources = glslang::DefaultTBuiltInResource;

    StageShaderMap stageShaderMap;
    std::unique_ptr<glslang::TProgram> program(new glslang::TProgram);

    for(auto& vsg_shader : shaders)
    {
        EShLanguage envStage = EShLangCount;
        switch(vsg_shader->getShaderStageFlagBits())
        {
            case(VK_SHADER_STAGE_VERTEX_BIT): envStage = EShLangVertex; break;
            case(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT): envStage = EShLangTessControl; break;
            case(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT): envStage = EShLangTessEvaluation; break;
            case(VK_SHADER_STAGE_GEOMETRY_BIT): envStage = EShLangGeometry; break;
            case(VK_SHADER_STAGE_FRAGMENT_BIT) : envStage = EShLangFragment; break;
            case(VK_SHADER_STAGE_COMPUTE_BIT): envStage = EShLangCompute; break;
    #ifdef VK_SHADER_STAGE_RAYGEN_BIT_NV
            case(VK_SHADER_STAGE_RAYGEN_BIT_NV): envStage = EShLangRayGenNV; break;
            case(VK_SHADER_STAGE_ANY_HIT_BIT_NV): envStage = EShLangAnyHitNV; break;
            case(VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV): envStage = EShLangClosestHitNV; break;
            case(VK_SHADER_STAGE_MISS_BIT_NV): envStage = EShLangMissNV; break;
            case(VK_SHADER_STAGE_INTERSECTION_BIT_NV): envStage = EShLangIntersectNV; break;
            case(VK_SHADER_STAGE_CALLABLE_BIT_NV): envStage = EShLangCallableNV; break;
            case(VK_SHADER_STAGE_TASK_BIT_NV): envStage = EShLangTaskNV; ;
            case(VK_SHADER_STAGE_MESH_BIT_NV): envStage = EShLangMeshNV; break;
    #endif
            default: break;
        }

        if (envStage==EShLangCount) return false;

        glslang::TShader* shader(new glslang::TShader(envStage));
        tshaders.emplace_back(shader);

        shader->setEnvInput(glslang::EShSourceGlsl, envStage, glslang::EShClientVulkan, 150);
        shader->setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
        shader->setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

        const char* str = vsg_shader->getShaderModule()->source().c_str();
        shader->setStrings(&str, 1);

        int defaultVersion = 110; // 110 desktop, 100 non desktop
        bool forwardCompatible = false;
        EShMessages messages = EShMsgDefault;
        bool parseResult = shader->parse(&builtInResources, defaultVersion, forwardCompatible,  messages);

        if (parseResult)
        {
            program->addShader(shader);

            stageShaderMap[envStage] = vsg_shader;
        }
        else
        {
            // print error infomation
            INFO_OUTPUT << std::endl << "----  " << getFriendlyNameForShader(vsg_shader) << "  ----" << std::endl << std::endl;
            INFO_OUTPUT << debugFormatShaderSource(vsg_shader->getShaderModule()->source()) << std::endl;
            INFO_OUTPUT << "Warning: GLSL source failed to parse." << std::endl;
            INFO_OUTPUT << "glslang info log: " << std::endl << shader->getInfoLog();
            DEBUG_OUTPUT << "glslang debug info log: " << std::endl << shader->getInfoDebugLog();
            INFO_OUTPUT << "--------" << std::endl;
        }
    }

    if (stageShaderMap.empty() || stageShaderMap.size() != shaders.size())
    {
        DEBUG_OUTPUT<<"ShaderCompiler::compile(Shaders& shaders) stageShaderMap.size() != shaders.size()"<<std::endl;
        return false;
    }

    EShMessages messages = EShMsgDefault;
    if (!program->link(messages))
    {
        INFO_OUTPUT << std::endl << "----  Program  ----" << std::endl << std::endl;

        for (auto& vsg_shader : shaders)
        {
            INFO_OUTPUT << std::endl << getFriendlyNameForShader(vsg_shader) << ":" << std::endl << std::endl;
            INFO_OUTPUT << debugFormatShaderSource(vsg_shader->getShaderModule()->source()) << std::endl;
        }
        
        INFO_OUTPUT << "Warning: Program failed to link." << std::endl;
        INFO_OUTPUT << "glslang info log: " << std::endl << program->getInfoLog();
        DEBUG_OUTPUT << "glslang debug info log: " << std::endl << program->getInfoDebugLog();
        INFO_OUTPUT << "--------" << std::endl;

        return false;
    }

    for (int eshl_stage = 0; eshl_stage < EShLangCount; ++eshl_stage)
    {
        auto vsg_shader = stageShaderMap[(EShLanguage)eshl_stage];
        if (vsg_shader && program->getIntermediate((EShLanguage)eshl_stage))
        {
            vsg::ShaderModule::SPIRV spirv;
            std::string warningsErrors;
            spv::SpvBuildLogger logger;
            glslang::SpvOptions spvOptions;
            glslang::GlslangToSpv(*(program->getIntermediate((EShLanguage)eshl_stage)), vsg_shader->getShaderModule()->spirv(), &logger, &spvOptions);
        }
    }

    return true;
}
