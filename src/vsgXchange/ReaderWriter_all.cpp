#include <vsgXchange/ReaderWriter_all.h>

#include <vsg/io/ReaderWriter_vsg.h>

#include "../glsl//ReaderWriter_glsl.h"
#include "../spirv/ReaderWriter_spirv.h"
#include "../cpp/ReaderWriter_cpp.h"

#ifdef USE_OPENSCENEGRAPH
#include "../osg/ReaderWriter_osg.h"
#endif
using namespace vsgXchange;

ReaderWriter_all::ReaderWriter_all()
{
    add(vsg::ReaderWriter_vsg::create());
    add(vsgXchange::ReaderWriter_glsl::create());
    add(vsgXchange::ReaderWriter_spirv::create());
    add(vsgXchange::ReaderWriter_cpp::create());
#ifdef USE_OPENSCENEGRAPH
    add(vsgXchange::ReaderWriter_osg::create());
#endif
}
