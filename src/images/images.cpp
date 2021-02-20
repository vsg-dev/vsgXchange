#include <vsgXchange/images.h>

using namespace vsgXchange;

images::images()
{
    add(vsgXchange::stbi::create());
    add(vsgXchange::dds::create());
    add(vsgXchange::ktx::create());
}
