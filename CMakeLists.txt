cmake_minimum_required(VERSION 3.7)

project(VSGXCHANGE
    VERSION 0.0.0
    DESCRIPTION "VulkanSceneGraph 3rd party data integration library"
    LANGUAGES CXX
)
set(VSGXCHANGE_SOVERSION 0)

set(OUTPUT_BINDIR ${PROJECT_BINARY_DIR}/bin)
set(OUTPUT_LIBDIR ${PROJECT_BINARY_DIR}/lib)

#set(OSG_PLUGINS_DIRECTORY osgPlugins-$OSG_MAJOR_VERSION}.$OSG_MINOR_VERSION}.${OSG_PATCH_VERSION})

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_BINDIR})
#set(CMAKE_MODULE_PATH "${VSGXCHANGE_SOURCE_DIR}/CMakeModules;${CMAKE_MODULE_PATH}")

SET(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "add a postfix, usually d on windows")
SET(CMAKE_RELEASE_POSTFIX "" CACHE STRING "add a postfix, usually empty on windows")
SET(CMAKE_RELWITHDEBINFO_POSTFIX "rd" CACHE STRING "add a postfix, usually empty on windows")
SET(CMAKE_MINSIZEREL_POSTFIX "s" CACHE STRING "add a postfix, usually empty on windows")

if(WIN32)
    SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_BINDIR})
else(WIN32)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_LIBDIR})
endif(WIN32)

# set up local bin directory to place all binaries
if(WIN32)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_BINDIR})
    # set up local bin directory to place all binaries
    make_directory(${OUTPUT_BINDIR})
    make_directory(${OUTPUT_LIBDIR})
else(WIN32)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_LIBDIR})
    # set up local bin directory to place all binaries
    make_directory(${OUTPUT_LIBDIR})
endif(WIN32)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_BINDIR})

# Change the default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

if(CMAKE_COMPILER_IS_GNUCXX)
    set(VSG_WARNING_FLAGS -Wall -Wparentheses -Wno-long-long -Wno-import -Wreturn-type -Wmissing-braces -Wunknown-pragmas -Wmaybe-uninitialized -Wshadow -Wunused -Wno-misleading-indentation -Wextra)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(VSG_WARNING_FLAGS -Wall -Wparentheses -Wno-long-long -Wno-import -Wreturn-type -Wmissing-braces -Wunknown-pragmas -Wuninitialized -Wshadow -Wunused -Wextra -Wno-overloaded-virtual)
endif()

set(VSG_WARNING_FLAGS ${VSG_WARNING_FLAGS} CACHE STRING "Compiler flags to use." FORCE)


foreach (flag ${VSG_WARNING_FLAGS})
    if(NOT CMAKE_CXX_FLAGS MATCHES "${flag}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
    endif()
endforeach ()


# set the use of C++17 globally as all examples require it
set(CMAKE_CXX_STANDARD 17)


# Find Vulkan and the VSG
if (VULKAN_SDK)
    set(ENV{VULKAN_SDK} ${VULKAN_SDK})
endif()

find_package(Vulkan REQUIRED)
find_package(vsg REQUIRED)

# Find GLSLang
find_package(glslang REQUIRED CONFIG PATHS ${VSGXCHANGE_SOURCE_DIR}/src)


# Find OpenScenegraph
find_package(OpenThreads)
find_package(osg)
find_package(osgDB)
find_package(osgTerrain)
find_package(osgUtil)
find_package(OpenGL)

if(OSG_FOUND AND OSGDB_FOUND AND OSGTERRAIN_FOUND AND OSGUTIL_FOUND AND OPENGL_FOUND)
    set(OPENSCENEGRAPH_FOUND ON)
    set(OPENSCENEGRAPH_LIBRARIES ${OPENTHREADS_LIBRARIES} ${OSG_LIBRARIES} ${OSGUTIL_LIBRARIES} ${OSGDB_LIBRARIES})
endif()


add_custom_target(clobber
    COMMAND git clean -d -f -x
)

# automatically buil_all_h build target to generate the include/vsg/all.h from the headers in the include/vsg/* directories
add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/build/uninstall.cmake
)

# source directory for main vsgXchange library
add_subdirectory(src)
add_subdirectory(applications/vsgconv)


