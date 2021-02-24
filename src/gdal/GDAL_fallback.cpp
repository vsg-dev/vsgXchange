
#include <vsgXchange/image.h>

using namespace vsgXchange;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// GDAL ReaderWriter fallback
//
struct GDAL::Implementation
{
};
GDAL::GDAL()
{
}
vsg::ref_ptr<vsg::Object> GDAL::read(const vsg::Path&, vsg::ref_ptr<const vsg::Options>) const
{
    return {};
}
bool GDAL::getFeatures(Features&) const
{
    return false;
}
