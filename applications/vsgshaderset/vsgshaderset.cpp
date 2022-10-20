#include <vsg/all.h>

#include <chrono>
#include <iostream>
#include <ostream>
#include <thread>

#include <vsgXchange/Version.h>
#include <vsgXchange/all.h>

void print(const vsg::ShaderSet& shaderSet, std::ostream& out)
{
    out<<"stages.size() = "<<shaderSet.stages.size()<<std::endl;
    for(auto& shaderStage : shaderSet.stages)
    {
        out<<"  ShaderStage {"<<std::endl;
        out<<"    flags = "<<shaderStage->flags<<std::endl;
        out<<"    stage = "<<shaderStage->stage<<std::endl;
        out<<"    entryPointName = "<<shaderStage->entryPointName<<std::endl;
        out<<"    module = "<<shaderStage->module<<std::endl;
        std::cout<<"  }"<<std::endl;
    }

    out<<std::endl;
    out<<"attributeBindings.size() = "<<shaderSet.attributeBindings.size()<<std::endl;
    for(auto& ab : shaderSet.attributeBindings)
    {
        out<<"  AttributeBinding {"<<std::endl;
        out<<"    name = "<<ab.name<<std::endl;
        out<<"    define = "<<ab.define<<std::endl;
        out<<"    location = "<<ab.location<<std::endl;
        out<<"    format = "<<ab.format<<std::endl;
        out<<"    data = "<<ab.data<<std::endl;
        out<<"  }"<<std::endl;
    }

    out<<std::endl;
    out<<"uniformBindings.size() = "<<shaderSet.uniformBindings.size()<<std::endl;
    for(auto& ub : shaderSet.uniformBindings)
    {
        out<<"  UniformBinding {"<<std::endl;
        out<<"    name = "<<ub.name<<std::endl;
        out<<"    define = "<<ub.define<<std::endl;
        out<<"    set = "<<ub.set<<std::endl;
        out<<"    binding = "<<ub.binding<<std::endl;
        out<<"    descriptorType = "<<ub.descriptorType<<std::endl;
        out<<"    stageFlags = "<<ub.stageFlags<<std::endl;
        out<<"    data = "<<ub.data<<std::endl;
        out<<"  }"<<std::endl;
    }

    out<<std::endl;
    out<<"pushConstantRanges.size() = "<<shaderSet.pushConstantRanges.size()<<std::endl;
    for(auto& pcr : shaderSet.pushConstantRanges)
    {
        out<<"  PushConstantRange {"<<std::endl;
        out<<"    name = "<<pcr.name<<std::endl;
        out<<"    define = "<<pcr.define<<std::endl;
        out<<"    range = { stageFlags = "<<pcr.range.stageFlags<<", offset = "<<pcr.range.offset<<", size = "<<pcr.range.size<<" }"<<std::endl;;
        out<<"  }"<<std::endl;
    }
    out<<std::endl;
    out<<"definesArrayStates.size() = "<<shaderSet.definesArrayStates.size()<<std::endl;
    for(auto& das : shaderSet.definesArrayStates)
    {
        out<<"  DefinesArrayState {"<<std::endl;
        out<<"    defines = { ";
        for(auto& define : das.defines) out<<define<<" ";
        out<<"}"<<std::endl;
        out<<"    arrayState = "<<das.arrayState<<std::endl;
        out<<"  }"<<std::endl;
    }

    out<<std::endl;
    out<<"optionalDefines.size() = "<<shaderSet.optionalDefines.size()<<std::endl;
    for(auto& define : shaderSet.optionalDefines)
    {
        out<<"    define = "<<define<<std::endl;
    }

    out<<std::endl;
    out<<"variants.size() = "<<shaderSet.variants.size()<<std::endl;
    for(auto& [shaderCompileSettings, shaderStages] : shaderSet.variants)
    {
        out<<"  Varient {"<<std::endl;
        out<<"    shaderCompileSettings = "<<shaderCompileSettings<<std::endl;
        out<<"    shaderStages = "<<shaderStages.size()<<std::endl;
        out<<"  }"<<std::endl;
    }

#if 0
        std::vector<DefinesArrayState> definesArrayStates; // put more constrained ArrayState matches first so they are matched first.
#endif
}

std::set<std::string> supportedDefines(const vsg::ShaderSet& shaderSet)
{
    std::set<std::string> defines;

    for(auto& ab : shaderSet.attributeBindings)
    {
        if (!ab.define.empty()) defines.insert(ab.define);
    }

    for(auto& ub : shaderSet.uniformBindings)
    {
        if (!ub.define.empty()) defines.insert(ub.define);
    }

    for(auto& pcr : shaderSet.pushConstantRanges)
    {
        if (!pcr.define.empty()) defines.insert(pcr.define);
    }

    for(auto& define : shaderSet.optionalDefines)
    {
        defines.insert(define);
    }

    return defines;
}

int main(int argc, char** argv)
{
    // use the vsg::Options object to pass the ReaderWriter_all to use when reading files.
    auto options = vsg::Options::create(vsgXchange::all::create());
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->sharedObjects = vsg::SharedObjects::create();

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    // read any command line options that the ReaderWrite support
    arguments.read(options);
    if (argc <= 1) return 0;

    auto inputFilename = arguments.value<vsg::Path>("", "-i");
    std::cout<<"inputFilename = "<<inputFilename<<std::endl;

    vsg::ref_ptr<vsg::ShaderSet> shaderSet;
    if (inputFilename)
    {
        auto object = vsg::read(inputFilename, options);
        if (!object)
        {
            std::cout<<"Unable to load "<<inputFilename<<std::endl;
            return 1;
        }

        shaderSet = object.cast<vsg::ShaderSet>();
        if (!shaderSet)
        {
            std::cout<<"Loaded file is not a ShaderSet"<<std::endl;
            return 1;
        }
    }

    if (arguments.read("--text")) shaderSet = vsg::createTextShaderSet(options);
    if (arguments.read("--flat")) shaderSet = vsg::createFlatShadedShaderSet(options);
    if (arguments.read("--phong")) shaderSet = vsg::createPhongShaderSet(options);
    if (arguments.read("--pbr")) shaderSet = vsg::createPhysicsBasedRenderingShaderSet(options);

    std::cout<<"shaderSet = "<<shaderSet<<std::endl;

    if (!shaderSet)
    {
        std::cout<<"No vsg::ShaderSet to process."<<std::endl;
        return 1;
    }

    print(*shaderSet, std::cout);

    auto defines = supportedDefines(*shaderSet);

    std::cout<<"\nSupported defines.size() = "<<defines.size()<<std::endl;
    for(auto& define : defines)
    {
        std::cout<<"   "<<define<<std::endl;
    }

    auto shaderCompiler = vsg::ShaderCompiler::create();
    std::cout<<"shaderCompiler->supported() = "<<shaderCompiler->supported()<<std::endl;

    return 0;
}
