#include "ReaderWriter_cpp.h"

#include <vsg/io/AsciiOutput.h>
#include <vsg/io/ReaderWriter_vsg.h>
#include <vsg/vk/ShaderStage.h>

#include <iostream>

using namespace vsgXchange;

ReaderWriter_cpp::ReaderWriter_cpp()
{
}

bool ReaderWriter_cpp::writeFile(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options>  /*options*/) const
{
    std::cout<<"ReaderWriter_cpp::writeFile("<<object->className()<<", "<<filename<<")"<<std::endl;

    auto ext = vsg::fileExtension(filename);
    if (ext=="cpp")
    {
        std::string funcname = vsg::simpleFilename(filename);

        std::ostringstream str;
#if 1
        vsg::ReaderWriter_vsg io;
        io.writeFile(object, str);
#else
        vsg::AsciiOutput output(str);
        output.writeObject("Root", object);
#endif
        std::ofstream fout(filename);
        fout<<"auto "<<funcname<<" = []() {";
        fout<<"std::istringstream str(\n";
            write(fout, str.str());
        fout<<");\n";
#if 1
        fout<<"vsg::ReaderWriter_vsg io;\n";
        fout<<"return io.readFile<"<<object->className()<<">(str);\n";
        fout<<"};\n";
        fout.close();
#else
        fout<<"vsg::AsciiInput input(str);\n";
        fout<<"return input.readObject<"<<object->className()<<">(\"Root\");\n";
        fout<<"};\n";
        fout.close();
#endif

        return true;
    }
    return false;
}

void ReaderWriter_cpp::write(std::ostream& out, const std::string& str) const
{
    out<<"\"";
    for(auto& c : str)
    {
        if (c=='\n') out<<"\\n\\\n";
        else if (c=='"') out<<"\\\"";
        else out<<c;
    }
    out<<"\"";
}
