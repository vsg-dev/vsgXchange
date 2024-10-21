/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgXchange/cpp.h>

#include <vsg/io/AsciiOutput.h>
#include <vsg/io/VSG.h>
#include <vsg/io/stream.h>

#include <iostream>

using namespace vsgXchange;

cpp::cpp()
{
}

bool cpp::write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    auto ext = vsg::lowerCaseFileExtension(filename);
    if (ext != ".cpp") return false;

    std::cout << "cpp::write(" << object->className() << ", " << filename << ")" << std::endl;

    auto funcname = vsg::simpleFilename(filename);

    bool binary = options ? (options->extensionHint == ".vsgb") : false;

    auto local_options = vsg::Options::create();
    local_options->extensionHint = binary ? ".vsgb" : ".vsgt";

    // serialize object(s) to string
    std::ostringstream str;
    vsg::VSG io;
    io.write(object, str, local_options);
    std::string s = str.str();

    std::ofstream fout(filename);
    fout << "#include <vsg/io/VSG.h>\n";
    fout << "#include <vsg/io/mem_stream.h>\n";
    fout << "static auto " << funcname << " = []() {\n";

    if (binary || s.size() > 65535)
    {
        // long string has to be handled as a byte array as VisualStudio can't handle long strings.
        fout << "static const uint8_t data[] = {\n";
        fout << uint32_t(uint8_t(s[0]));
        for (size_t i = 1; i < s.size(); ++i)
        {
            if ((i % 32) == 0)
                fout << ",\n";
            else
                fout << ", ";
            fout << uint32_t(uint8_t(s[i]));
        }
        fout << " };\n";
        //fout<<"vsg::mem_stream str(data, sizeof(data));\n";
        fout << "vsg::VSG io;\n";
        fout << "return io.read_cast<" << object->className() << ">(data, sizeof(data));\n";
    }
    else
    {
        fout << "static const char str[] = \n";
        write(fout, str.str());
        fout << ";\n";
        fout << "vsg::VSG io;\n";
        fout << "return io.read_cast<" << object->className() << ">(reinterpret_cast<const uint8_t*>(str), sizeof(str));\n";
    }

    fout << "};\n";
    fout.close();

    return true;
}

void cpp::write(std::ostream& out, const std::string& str) const
{
    std::size_t max_string_literal_length = 16360;
    if (str.size() > max_string_literal_length)
    {
        std::size_t n = 0;
        while ((n + max_string_literal_length) < str.size())
        {
            auto pos_previous_end_of_line = str.find_last_of("\n", n + max_string_literal_length);
            if (pos_previous_end_of_line > n)
            {
                out << "R\"(" << str.substr(n, pos_previous_end_of_line + 1 - n) << ")\"\n";
                n = pos_previous_end_of_line + 1;
            }
            else
            {
                out << "R\"(" << str.substr(n, max_string_literal_length) << ")\" ";
                n += max_string_literal_length;
            }
        }

        if (n < str.size())
        {
            out << "R\"(" << str.substr(n, std::string::npos) << ")\"";
        }
    }
    else
    {
        out << "R\"(" << str << ")\"";
    }
}

bool cpp::getFeatures(Features& features) const
{
    features.extensionFeatureMap[".cpp"] = vsg::ReaderWriter::WRITE_FILENAME;
    return true;
}
