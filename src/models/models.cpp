#include <vsgXchange/models.h>

using namespace vsgXchange;

models::models()
{
#ifdef USE_ASSIMP
    add(vsgXchange::assimp::create());
#endif

#ifdef USE_OPENSCENEGRAPH
    add(vsgXchange::OSG::create());
#endif
}
