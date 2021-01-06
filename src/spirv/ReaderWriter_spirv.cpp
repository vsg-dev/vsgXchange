#include "ReaderWriter_spirv.h"

#include <vsg/state/ShaderStage.h>
#include <vsg/vk/ShaderCompiler.h>

#include <iostream>

using namespace vsgXchange;

ReaderWriter_spirv::ReaderWriter_spirv()
{
}

vsg::ref_ptr<vsg::Object> ReaderWriter_spirv::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> /*options*/) const
{
    auto ext = vsg::fileExtension(filename);
    if (ext == "spv" && vsg::fileExists(filename))
    {
        vsg::ShaderModule::SPIRV spirv;
        vsg::readFile(spirv, filename);

        auto sm = vsg::ShaderModule::create(spirv);
        return sm;
    }
    return vsg::ref_ptr<vsg::Object>();
}

bool ReaderWriter_spirv::write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> /*options*/) const
{
    auto ext = vsg::fileExtension(filename);
    if (ext == "spv")
    {
        const vsg::ShaderStage* ss = dynamic_cast<const vsg::ShaderStage*>(object);
        const vsg::ShaderModule* sm = ss ? ss->module.get() : dynamic_cast<const vsg::ShaderModule*>(object);
        if (sm)
        {
            if (sm->code.empty())
            {
                vsg::ShaderCompiler sc;
                if (!sc.compile(vsg::ref_ptr<vsg::ShaderStage>(const_cast<vsg::ShaderStage*>(ss))))
                {
                    std::cout << "ReaderWriter_spirv::write() Failed compile tp spv." << std::endl;
                    return false;
                }
            }

            if (!sm->code.empty())
            {
                std::ofstream fout(filename);
                fout.write(reinterpret_cast<const char*>(sm->code.data()), sm->code.size() * sizeof(vsg::ShaderModule::SPIRV::value_type));
                fout.close();
                return true;
            }
        }
    }
    return false;
}
