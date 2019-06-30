include(CMakeFindDependencyMacro)

find_dependency(Vulkan)
find_dependency(vsg)
find_dependency(OpenThreads)
find_dependency(osg)
find_dependency(osgUtil)
find_dependency(osgDB)

include("${CMAKE_CURRENT_LIST_DIR}/osg2vsgTargets.cmake")
