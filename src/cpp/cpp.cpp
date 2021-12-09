/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

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
    if (ext != ".cpp") return false;

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

bool cpp::getFeatures(Features& features) const
{
    features.extensionFeatureMap[".cpp"] = vsg::ReaderWriter::WRITE_FILENAME;
    return true;
}
