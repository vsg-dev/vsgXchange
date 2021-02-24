#include <vsgXchange/models.h>

using namespace vsgXchange;

models::models()
{
#ifdef VSGXCHANGE_assimp
    add(assimp::create());
#endif

#ifdef VSGXCHANGE_OSG
    add(OSG::create());
#endif
}
