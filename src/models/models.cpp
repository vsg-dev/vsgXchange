#include <vsgXchange/models.h>

using namespace vsgXchange;

models::models()
{
#ifdef vsgXchange_assimp
    add(assimp::create());
#endif

#ifdef vsgXchange_OSG
    add(OSG::create());
#endif
}
