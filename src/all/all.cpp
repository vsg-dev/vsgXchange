#include <vsgXchange/all.h>
#include <vsgXchange/glsl.h>
#include <vsgXchange/cpp.h>
#include <vsgXchange/freetype.h>

#include <vsg/io/VSG.h>

#include "../dds/ReaderWriter_dds.h"
#include "../spirv/ReaderWriter_spirv.h"
#include "../stbi/ReaderWriter_stbi.h"

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
    add(cpp::create());

    add(vsgXchange::ReaderWriter_stbi::create());
    add(vsgXchange::ReaderWriter_dds::create());

#ifdef USE_FREETYPE
    add(freetype::create());
#endif

#ifdef USE_ASSIMP
    add(vsgXchange::ReaderWriter_assimp::create());
#endif
#ifdef USE_OPENSCENEGRAPH
    add(vsgXchange::ReaderWriter_osg::create());
#endif
}
