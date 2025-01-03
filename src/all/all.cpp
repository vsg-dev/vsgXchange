/* <editor-fold desc="MIT License">

Copyright(c) 2021 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgXchange/all.h>
#include <vsgXchange/cpp.h>
#include <vsgXchange/curl.h>
#include <vsgXchange/freetype.h>
#include <vsgXchange/gdal.h>
#include <vsgXchange/images.h>
#include <vsgXchange/models.h>

#include <vsg/io/Logger.h>
#include <vsg/io/VSG.h>
#include <vsg/io/glsl.h>
#include <vsg/io/spirv.h>
#include <vsg/io/txt.h>

#ifdef vsgXchange_OSG
#    include <osg2vsg/OSG.h>
#endif

using namespace vsgXchange;

void vsgXchange::init()
{
    static bool s_vsgXchange_initialized = false;
    if (s_vsgXchange_initialized) return;
    s_vsgXchange_initialized = true;

    vsg::debug("vsgXchange::init()");

    vsg::ObjectFactory::instance()->add<vsgXchange::all>();
    vsg::ObjectFactory::instance()->add<vsgXchange::curl>();

    vsg::ObjectFactory::instance()->add<vsgXchange::cpp>();

    vsg::ObjectFactory::instance()->add<vsgXchange::stbi>();
    vsg::ObjectFactory::instance()->add<vsgXchange::dds>();
    vsg::ObjectFactory::instance()->add<vsgXchange::ktx>();

    vsg::ObjectFactory::instance()->add<vsgXchange::openexr>();
    vsg::ObjectFactory::instance()->add<vsgXchange::freetype>();
    vsg::ObjectFactory::instance()->add<vsgXchange::assimp>();
    vsg::ObjectFactory::instance()->add<vsgXchange::GDAL>();
    //    vsg::ObjectFactory::instance()->add<vsgXchange::OSG>();
}

all::all()
{
    // for convenience make sure the init() method is called
    vsgXchange::init();

#ifdef vsgXchange_curl
    add(curl::create());
#endif

    add(vsg::VSG::create());
    add(vsg::spirv::create());
    add(vsg::glsl::create());
    add(vsg::txt::create());

    add(cpp::create());

    add(stbi::create());
    add(dds::create());
    add(ktx::create());

#ifdef vsgXchange_openexr
    add(openexr::create());
#endif

#ifdef vsgXchange_freetype
    add(freetype::create());
#endif

#ifdef vsgXchange_assimp
    add(assimp::create());
#endif

#ifdef vsgXchange_GDAL
    add(GDAL::create());
#endif

#ifdef vsgXchange_OSG
    add(osg2vsg::OSG::create());
#endif
}
