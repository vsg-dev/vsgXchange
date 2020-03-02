include(CMakeFindDependencyMacro)

find_dependency(Vulkan)
find_dependency(vsg)

include("${CMAKE_CURRENT_LIST_DIR}/vsgXchangeTargets.cmake")
