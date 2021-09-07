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
#include <vsgXchange/glsl.h>
#include <vsgXchange/images.h>
#include <vsgXchange/models.h>

#include <vsg/io/VSG.h>
#include <vsg/io/spirv.h>

using namespace vsgXchange;

all::all()
{
#ifdef vsgXchange_CURL
    add(curl::create());
#endif

    add(vsg::VSG::create());
    add(vsg::spirv::create());

    add(glsl::create());
    add(cpp::create());

#ifdef vsgXchange_GDAL
    add(GDAL::create());
#endif

    add(stbi::create());
    add(dds::create());
    add(ktx::create());

#ifdef vsgXchange_freetype
    add(freetype::create());
#endif

#ifdef vsgXchange_assimp
    add(assimp::create());
#endif

#ifdef vsgXchange_OSG
    add(OSG::create());
#endif
}
