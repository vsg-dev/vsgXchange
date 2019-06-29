#include <vsgXchange/ReaderWriter_cpp.h>

#include <vsg/vk/ShaderStage.h>

#include <iostream>

using namespace vsgXchange;

ReaderWriter_cpp::ReaderWriter_cpp()
{
}

bool ReaderWriter_cpp::writeFile(const vsg::Object* object, const vsg::Path& filename) const
{
    std::cout<<"ReaderWriter_cpp::writeFile("<<object->className()<<", "<<filename<<")"<<std::endl;

    auto ext = vsg::fileExtension(filename);
    if (ext=="cpp")
    {
        std::string funcname = vsg::simpleFilename(filename);
        const vsg::ShaderStage* ss = dynamic_cast<const vsg::ShaderStage*>(object);
        const vsg::ShaderModule* sm = ss ? ss->getShaderModule() : dynamic_cast<const vsg::ShaderModule*>(object);
        if (ss)
        {
            vsg::ShaderModule::Source source;
            vsg::ShaderModule::SPIRV spirv;
            if (sm)
            {
                source = sm->source();
                spirv = sm->spirv();
            }

            std::ofstream fout(filename);
            fout<<"auto "<<funcname<<" = []() {\n";
            fout<<"return vsg::ShaderStage::create(\n";
            fout<<"VkShaderStageFlagBits("<< ss->getShaderStageFlagBits()<<"),\n";
                write(fout, ss->getEntryPointName());
            fout<<",\n";
                write(fout, source);
            fout<<",\n";
            fout<<"vsg::ShaderModule::SPIRV{";
                if (spirv.size()>1) fout<<spirv.front();
                for(unsigned int i=1; i<spirv.size(); ++i) fout<<", "<<spirv[i];
            fout<<"});\n";
            fout<<"};\n";
            fout.close();
            return true;
        }
        else if (sm)
        {
            const vsg::ShaderModule::Source& source = sm->source();
            const vsg::ShaderModule::SPIRV& spirv = sm->spirv();

            std::ofstream fout(filename);
            fout<<"auto "<<funcname<<" = []() {\n";
            fout<<"return vsg::ShadeModule::create(\n";
                write(fout, source);
            fout<<",\n";
            fout<<"vsg::ShaderModule::SPIRV{";
                if (spirv.size()>1) fout<<spirv.front();
                for(unsigned int i=1; i<spirv.size(); ++i) fout<<", "<<spirv[i];
            fout<<"});\n";
            fout<<"};\n";
            fout.close();
            return true;
        }
    }
    return false;
}

void ReaderWriter_cpp::write(std::ostream& out, const std::string& str) const
{
    out<<"\"";
    for(auto& c : str)
    {
        if (c=='\n') out<<"\\n\\\n";
        else out<<c;
    }
    out<<"\"";
}
