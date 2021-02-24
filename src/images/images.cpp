#include <vsgXchange/images.h>

using namespace vsgXchange;

images::images()
{
#ifdef vsgXchange_GDAL
    add(GDAL::create());
#endif

    add(stbi::create());
    add(dds::create());
    add(ktx::create());
}
