
# vars used to enable subdirectories to extend the build of the vsgXchange library in a loose coupled way
set(EXTRA_DEFINES)
set(EXTRA_INCLUDES)

SET(HEADER_PATH ${VSGXCHANGE_SOURCE_DIR}/include/vsgXchange)

set(HEADERS
    ${VSGXCHANGE_VERSION_HEADER}
    ${HEADER_PATH}/Export.h
    ${HEADER_PATH}/all.h
    ${HEADER_PATH}/cpp.h
    ${HEADER_PATH}/freetype.h
    ${HEADER_PATH}/glsl.h
    ${HEADER_PATH}/images.h
    ${HEADER_PATH}/models.h
)

set(SOURCES
    all/Version.cpp
    all/all.cpp
    cpp/cpp.cpp
    glsl/glsl.cpp
    stbi/stbi.cpp
    dds/dds.cpp
    images/images.cpp
)

# add freetype if available
include(freetype/build_vars.cmake)

# add assimp if available
include(assimp/build_vars.cmake)

# add ktx
include(ktx/build_vars.cmake)

# add optional GDAL component
include(gdal/build_vars.cmake)

# add optional CURL component
include(curl/build_vars.cmake)

# add optional OpenEXR component
include(openexr/build_vars.cmake)

# create the version header
set(VSGXCHANGE_VERSION_HEADER "${VSGXCHANGE_BINARY_DIR}/include/vsgXchange/Version.h")
configure_file("${VSGXCHANGE_SOURCE_DIR}/src/all/Version.h.in" "${VSGXCHANGE_VERSION_HEADER}")


# create the vsgXchange library
add_library(vsgXchange ${HEADERS} ${SOURCES})
add_library(vsgXchange::vsgXchange ALIAS vsgXchange)


set_property(TARGET vsgXchange PROPERTY VERSION ${VSGXCHANGE_VERSION_MAJOR}.${VSGXCHANGE_VERSION_MINOR}.${VSGXCHANGE_VERSION_PATCH})
set_property(TARGET vsgXchange PROPERTY SOVERSION ${VSGXCHANGE_SOVERSION})
set_property(TARGET vsgXchange PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET vsgXchange PROPERTY CXX_STANDARD 17)

target_compile_definitions(vsgXchange PRIVATE ${EXTRA_DEFINES})

target_include_directories(vsgXchange
    PUBLIC
        $<BUILD_INTERFACE:${VSGXCHANGE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${EXTRA_INCLUDES}
)

target_link_libraries(vsgXchange
    PUBLIC
        vsg::vsg
    PRIVATE
        ${EXTRA_LIBRARIES}
)

install(TARGETS vsgXchange ${INSTALL_TARGETS_DEFAULT_FLAGS})

if (BUILD_SHARED_LIBS)
    target_compile_definitions(vsgXchange INTERFACE VSGXCHANGE_SHARED_LIBRARY)
endif()

install(DIRECTORY ${VSGXCHANGE_SOURCE_DIR}/include/vsgXchange DESTINATION include)
if (NOT(${CMAKE_BINARY_DIR} STREQUAL ${VSGXCHANGE_SOURCE_DIR}))
    install(DIRECTORY ${CMAKE_BINARY_DIR}/include/vsgXchange DESTINATION include)
endif()

string(REPLACE ";" "\n" FIND_DEPENDENCY_OUT "${FIND_DEPENDENCY}")
vsg_add_cmake_support_files(
    CONFIG_TEMPLATE
        vsgXchangeConfig.cmake.in
)
