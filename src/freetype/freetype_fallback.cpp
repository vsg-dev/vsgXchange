
#include <vsgXchange/freetype.h>

using namespace vsgXchange;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// freetype ReaderWriter fallback
//
struct freetype::Implementation
{
};
freetype::freetype()
{
}
vsg::ref_ptr<vsg::Object> freetype::read(const vsg::Path&, vsg::ref_ptr<const vsg::Options>) const
{
    return {};
}
bool freetype::getFeatures(Features&) const
{
    return false;
}
