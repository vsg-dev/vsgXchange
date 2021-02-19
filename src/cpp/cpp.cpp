#include <vsgXchange/cpp.h>

#include <vsg/io/AsciiOutput.h>
#include <vsg/io/VSG.h>

#include <iostream>

using namespace vsgXchange;

cpp::cpp()
{
}

bool cpp::write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> /*options*/) const
{
    auto ext = vsg::lowerCaseFileExtension(filename);
    if (ext != "cpp") return false;

    std::cout << "cpp::write(" << object->className() << ", " << filename << ")" << std::endl;

    std::string funcname = vsg::simpleFilename(filename);

    std::ostringstream str;

    vsg::VSG io;
    io.write(object, str);

    std::ofstream fout(filename);
    fout << "#include <vsg/io/VSG.h>\n";
    fout << "static auto " << funcname << " = []() {";
    fout << "std::istringstream str(\n";
    write(fout, str.str());
    fout << ");\n";

    fout << "vsg::VSG io;\n";
    fout << "return io.read_cast<" << object->className() << ">(str);\n";
    fout << "};\n";
    fout.close();

    return true;
}

void cpp::write(std::ostream& out, const std::string& str) const
{
    out << "R\"(" << str << ")\"";
}
