#include <vsgXchange/ReaderWriter_all.h>

#include <vsg/io/ReaderWriter_vsg.h>

#include "../cpp/ReaderWriter_cpp.h"
#include "../glsl//ReaderWriter_glsl.h"
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

ReaderWriter_all::ReaderWriter_all()
{
    add(vsg::ReaderWriter_vsg::create());
    add(vsgXchange::ReaderWriter_glsl::create());
    add(vsgXchange::ReaderWriter_spirv::create());
    add(vsgXchange::ReaderWriter_cpp::create());
    add(vsgXchange::ReaderWriter_stbi::create());
#ifdef USE_FREETYPE
    add(vsgXchange::ReaderWriter_freetype::create());
#endif
#ifdef USE_OPENSCENEGRAPH
    add(vsgXchange::ReaderWriter_osg::create());
#endif
#ifdef USE_ASSIMP
    add(vsgXchange::ReaderWriter_assimp::create());
#endif
}
