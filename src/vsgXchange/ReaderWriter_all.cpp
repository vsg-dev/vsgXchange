#include <vsgXchange/ReaderWriter_all.h>

#include <vsgXchange/ReaderWriter_glsl.h>
#include <vsgXchange/ReaderWriter_spirv.h>
#include <vsgXchange/ReaderWriter_cpp.h>

using namespace vsgXchange;

ReaderWriter_all::ReaderWriter_all()
{
    add(vsg::vsgReaderWriter::create());
    add(vsgXchange::ReaderWriter_glsl::create());
    add(vsgXchange::ReaderWriter_spirv::create());
    add(vsgXchange::ReaderWriter_cpp::create());
}
