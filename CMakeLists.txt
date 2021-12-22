cmake_minimum_required(VERSION 3.7)

project(vsgXchange
    VERSION 0.0.1
    DESCRIPTION "VulkanSceneGraph 3rd party data integration library"
    LANGUAGES CXX C
)
set(VSGXCHANGE_SOVERSION 0)
SET(VSGXCHANGE_RELEASE_CANDIDATE 0)

# set the use of C++17 globally as all examples require it
set(CMAKE_CXX_STANDARD 17)


# Find Vulkan and the VSG
if (VULKAN_SDK)
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
endif()

set(VSG_MIN_VERSION 0.2.1)
find_package(vsg ${VSG_MIN_VERSION} REQUIRED)

# for generated cmake support files
set(FIND_DEPENDENCY ${FIND_DEPENDENCY} "find_dependency(vsg ${VSG_MIN_VERSION} REQUIRED)")

vsg_setup_build_vars()
vsg_setup_dir_vars()

vsg_add_target_clang_format(
    FILES
        ${CMAKE_SOURCE_DIR}/include/*/*.h
        ${CMAKE_SOURCE_DIR}/src/*/*.h
        ${CMAKE_SOURCE_DIR}/src/*/*.cpp
        ${CMAKE_SOURCE_DIR}/applications/*/*.h
        ${CMAKE_SOURCE_DIR}/applications/*/*.cpp
    EXCLUDES
        ${CMAKE_SOURCE_DIR}/src/stbi/stb_image.h
        ${CMAKE_SOURCE_DIR}/src/dds/tinyddsloader.h
)
vsg_add_target_clobber()
vsg_add_target_docs(
    FILES
        ${CMAKE_SOURCE_DIR}/include
)
vsg_add_target_uninstall()

vsg_add_option_maintainer(
    PREFIX ${PROJECT_NAME}
    RCLEVEL ${VSGXCHANGE_RELEASE_CANDIDATE}
)

# source directory for main vsgXchange library
add_subdirectory(src)
add_subdirectory(applications/vsgconv)

vsg_add_feature_summary()
