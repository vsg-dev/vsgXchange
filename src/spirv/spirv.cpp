#include <vsgXchange/spirv.h>

#include <vsg/state/ShaderStage.h>
#include <vsg/vk/ShaderCompiler.h>

#include <iostream>

using namespace vsgXchange;

spirv::spirv()
{
}

vsg::ref_ptr<vsg::Object> spirv::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> /*options*/) const
{
    auto ext = vsg::lowerCaseFileExtension(filename);
    if (ext == "spv" && vsg::fileExists(filename))
    {
        vsg::ShaderModule::SPIRV spirv_module;
        vsg::readFile(spirv_module, filename);

        auto sm = vsg::ShaderModule::create(spirv_module);
        return sm;
    }
    return vsg::ref_ptr<vsg::Object>();
}

bool spirv::write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> /*options*/) const
{
    auto ext = vsg::lowerCaseFileExtension(filename);
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
                    std::cout << "spirv::write() Failed compile tp spv." << std::endl;
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

bool spirv::getFeatures(Features& features) const
{
    features.extensionFeatureMap["spv"] = static_cast<vsg::ReaderWriter::FeatureMask>(vsg::ReaderWriter::READ_FILENAME | vsg::ReaderWriter::WRITE_FILENAME );
    return true;
}
