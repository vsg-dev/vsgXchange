#include "ReaderWriter_cpp.h"

#include <vsg/io/AsciiOutput.h>
#include <vsg/io/ReaderWriter_vsg.h>

#include <iostream>

using namespace vsgXchange;

ReaderWriter_cpp::ReaderWriter_cpp()
{
}

bool ReaderWriter_cpp::write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> /*options*/) const
{
    auto ext = vsg::lowerCaseFileExtension(filename);
    if (ext != "cpp") return false;

    std::cout << "ReaderWriter_cpp::write(" << object->className() << ", " << filename << ")" << std::endl;

    std::string funcname = vsg::simpleFilename(filename);

    std::ostringstream str;

    vsg::ReaderWriter_vsg io;
    io.write(object, str);

    std::ofstream fout(filename);
    fout << "#include <vsg/io/ReaderWriter_vsg.h>\n";
    fout << "static auto " << funcname << " = []() {";
    fout << "std::istringstream str(\n";
    write(fout, str.str());
    fout << ");\n";

    fout << "vsg::ReaderWriter_vsg io;\n";
    fout << "return io.read_cast<" << object->className() << ">(str);\n";
    fout << "};\n";
    fout.close();

    return true;
}

void ReaderWriter_cpp::write(std::ostream& out, const std::string& str) const
{
    out << "R\"(" << str << ")\"";
}
