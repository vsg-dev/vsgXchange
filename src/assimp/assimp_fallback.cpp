
#include <vsgXchange/models.h>

using namespace vsgXchange;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// assimp ReaderWriter fallback
//
struct assimp::Implementation {};
assimp::assimp() {}
vsg::ref_ptr<vsg::Object> assimp::read(const vsg::Path&, vsg::ref_ptr<const vsg::Options>) const { return {}; }
vsg::ref_ptr<vsg::Object> assimp::read(std::istream&, vsg::ref_ptr<const vsg::Options>) const { return {}; }