cmake_minimum_required(VERSION 3.10)

project(vsgXchange
    VERSION 1.1.9
    DESCRIPTION "VulkanSceneGraph 3rd party data integration library"
    LANGUAGES CXX C
)
set(VSGXCHANGE_SOVERSION 2)
SET(VSGXCHANGE_RELEASE_CANDIDATE 0)

set(VSGXCHANGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "Root source directory of vsgXchange.")
set(VSGXCHANGE_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}" CACHE INTERNAL "Root binary directory of vsgXchange.")

# Find Vulkan and the VSG
if (VULKAN_SDK)
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
endif()

set(VSG_MIN_VERSION 1.1.13)
find_package(vsg ${VSG_MIN_VERSION})

vsg_setup_dir_vars()
vsg_setup_build_vars()

# find optional osg2vsg package for loading image/nodes using OpenSceneGraph
find_package(osg2vsg QUIET)
if(osg2vsg_FOUND)
    OPTION(vsgXchange_OSG "OSG support provided" ON)
endif()

if (vsgXchange_OSG)
    set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} osg2vsg::osg2vsg)
    if(NOT BUILD_SHARED_LIBS)
        set(FIND_DEPENDENCY ${FIND_DEPENDENCY} "find_dependency(osg2vsg)")
    endif()
endif()

# for generated cmake support files
set(FIND_DEPENDENCY ${FIND_DEPENDENCY} "find_dependency(vsg ${VSG_MIN_VERSION} REQUIRED)")

vsg_add_target_clang_format(
    FILES
        ${VSGXCHANGE_SOURCE_DIR}/include/*/*.h
        ${VSGXCHANGE_SOURCE_DIR}/src/*/*.h
        ${VSGXCHANGE_SOURCE_DIR}/src/*/*.cpp
        ${VSGXCHANGE_SOURCE_DIR}/applications/*/*.h
        ${VSGXCHANGE_SOURCE_DIR}/applications/*/*.cpp
    EXCLUDES
        ${VSGXCHANGE_SOURCE_DIR}/src/stbi/stb_image.h
        ${VSGXCHANGE_SOURCE_DIR}/src/stbi/stb_image_write.h
        ${VSGXCHANGE_SOURCE_DIR}/src/dds/tinyddsloader.h
)
vsg_add_target_clobber()
vsg_add_target_cppcheck(
    FILES
        ${VSGXCHANGE_SOURCE_DIR}/include/*/*.h
        ${VSGXCHANGE_SOURCE_DIR}/src/*/*.h
        ${VSGXCHANGE_SOURCE_DIR}/src/*/*.cpp
        ${VSGXCHANGE_SOURCE_DIR}/applications/*/*.h
        ${VSGXCHANGE_SOURCE_DIR}/applications/*/*.cpp
)
vsg_add_target_docs(
    FILES
        ${VSGXCHANGE_SOURCE_DIR}/include
)
vsg_add_target_uninstall()

# only provide custom targets if not building as a submodule/FetchContent
if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})

    vsg_add_option_maintainer(
        PREFIX v
        RCLEVEL ${VSGXCHANGE_RELEASE_CANDIDATE}
    )

endif()

# source directory for main vsgXchange library
add_subdirectory(src)
add_subdirectory(applications/vsgconv)

vsg_add_feature_summary()
