include(CMakeFindDependencyMacro)

find_dependency(Vulkan)
find_dependency(vsg)
find_dependency(glslang)


include("${CMAKE_CURRENT_LIST_DIR}/vsgXchangeTargets.cmake")
