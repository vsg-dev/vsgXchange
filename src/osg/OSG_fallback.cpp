
#include <vsgXchange/models.h>

using namespace vsgXchange;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// OSG ReaderWriter fallback
//
struct OSG::Implementation {};
OSG::OSG() {}
bool OSG::readOptions(vsg::Options&, vsg::CommandLine&) const { return false; }
vsg::ref_ptr<vsg::Object> OSG::read(const vsg::Path&, vsg::ref_ptr<const vsg::Options>) const { return {}; }
bool OSG::getFeatures(Features&) const { return false; }
