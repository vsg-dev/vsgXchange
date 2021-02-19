#include <vsgXchange/all.h>
#include <vsgXchange/glsl.h>

#include <vsg/io/VSG.h>

#include "../cpp/ReaderWriter_cpp.h"
#include "../dds/ReaderWriter_dds.h"
#include "../spirv/ReaderWriter_spirv.h"
#include "../stbi/ReaderWriter_stbi.h"

#ifdef USE_FREETYPE
#    include "../freetype/FreeTypeFont.h"
#endif

#ifdef USE_OPENSCENEGRAPH
#    include "../osg/ReaderWriter_osg.h"
#endif

#ifdef USE_ASSIMP
#    include "../assimp/ReaderWriter_assimp.h"
#endif

using namespace vsgXchange;

all::all()
{
    add(vsg::VSG::create());
    add(glsl::create());
    add(vsgXchange::ReaderWriter_spirv::create());
    add(vsgXchange::ReaderWriter_cpp::create());
    add(vsgXchange::ReaderWriter_stbi::create());
    add(vsgXchange::ReaderWriter_dds::create());
#ifdef USE_FREETYPE
    add(vsgXchange::ReaderWriter_freetype::create());
#endif
#ifdef USE_ASSIMP
    add(vsgXchange::ReaderWriter_assimp::create());
#endif
#ifdef USE_OPENSCENEGRAPH
    add(vsgXchange::ReaderWriter_osg::create());
#endif
}
