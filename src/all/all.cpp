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

#ifdef VSGXCHANGE_GDAL
    add(GDAL::create());
#endif

    add(stbi::create());
    add(dds::create());
    add(ktx::create());

#ifdef VSGXCHANGE_freetype
    add(freetype::create());
#endif

#ifdef VSGXCHANGE_assimp
    add(assimp::create());
#endif

#ifdef VSGXCHANGE_OSG
    add(OSG::create());
#endif
}
