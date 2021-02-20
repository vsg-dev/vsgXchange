#include <vsgXchange/all.h>
#include <vsgXchange/glsl.h>
#include <vsgXchange/cpp.h>
#include <vsgXchange/freetype.h>
#include <vsgXchange/spirv.h>
#include <vsgXchange/images.h>
#include <vsgXchange/models.h>

#include <vsg/io/VSG.h>


using namespace vsgXchange;

all::all()
{
    add(vsg::VSG::create());
    add(glsl::create());
    add(spirv::create());
    add(cpp::create());

    // equivlant to using vsgXchange::images
    add(vsgXchange::stbi::create());
    add(vsgXchange::dds::create());
    add(vsgXchange::ktx::create());

#ifdef USE_FREETYPE
    add(freetype::create());
#endif

#ifdef USE_ASSIMP
    add(vsgXchange::assimp::create());
#endif

#ifdef USE_OPENSCENEGRAPH
    add(vsgXchange::OSG::create());
#endif
}