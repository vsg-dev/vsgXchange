#include "ReaderWriter_cpp.h"

#include <vsg/io/AsciiOutput.h>
#include <vsg/vk/ShaderStage.h>

#include <iostream>

using namespace vsgXchange;

ReaderWriter_cpp::ReaderWriter_cpp()
{
}

bool ReaderWriter_cpp::writeFile(const vsg::Object* object, const vsg::Path& filename, vsg::Options* /*options*/) const
{
    std::cout<<"ReaderWriter_cpp::writeFile("<<object->className()<<", "<<filename<<")"<<std::endl;

    auto ext = vsg::fileExtension(filename);
    if (ext=="cpp")
    {
        std::string funcname = vsg::simpleFilename(filename);

        std::ostringstream str;
        vsg::AsciiOutput output(str);
        output.writeObject("Root", object);

        std::ofstream fout(filename);
        fout<<"auto "<<funcname<<" = []() {\n";
        fout<<"std::istringstream str(";
            write(fout, str.str());
        fout<<");\n";
        fout<<"vsg::AsciiInput input(str);\n";
        fout<<"return input.readObject<"<<object->className()<<">(\"Root\");\n";
        fout<<"};\n";
        fout.close();

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
