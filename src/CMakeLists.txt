
# vars used to enable subdirectories to extend the build of the vsgXchange library in a loose coupled way
set(EXTRA_DEFINES)
set(EXTRA_INCLUDES)

SET(HEADER_PATH ${CMAKE_SOURCE_DIR}/include/vsgXchange)

set(HEADERS
    ${VSGXCHANGE_VERSION_HEADER}
    ${HEADER_PATH}/Export.h
    ${HEADER_PATH}/all.h
    ${HEADER_PATH}/cpp.h
    ${HEADER_PATH}/glsl.h
    ${HEADER_PATH}/spirv.h
    ${HEADER_PATH}/freetype.h
    ${HEADER_PATH}/images.h
 )

set(SOURCES
    all/Version.cpp
    all/all.cpp
    cpp/cpp.cpp
    glsl/glsl.cpp
    spirv/spirv.cpp
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

# add OpenSceneGraph support if available
include(osg/build_vars.cmake)

# add optional vsgGIS/GDAL component
include(gdal/build_vars.cmake)

# add optional CURL component
include(curl/build_vars.cmake)

if(${VSGXCHANGE_GDAL})
    set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} vsgGIS::vsgGIS)
    set(EXTRA_DEFINES ${EXTRA_DEFINES} USE_vsgGIS)
    set(FIND_DEPENDENCY ${FIND_DEPENDENCY} "find_dependency(vsgGIS)")
endif()

# create the version header
set(VSGXCHANGE_VERSION_HEADER "${PROJECT_BINARY_DIR}/include/vsgXchange/Version.h")
configure_file("${CMAKE_SOURCE_DIR}/src/all/Version.h.in" "${VSGXCHANGE_VERSION_HEADER}")


# create the vsgXchange library
add_library(vsgXchange ${HEADERS} ${SOURCES})


set_property(TARGET vsgXchange PROPERTY VERSION ${VSGXCHANGE_VERSION_MAJOR}.${VSGXCHANGE_VERSION_MINOR}.${VSGXCHANGE_VERSION_PATCH})
set_property(TARGET vsgXchange PROPERTY SOVERSION ${VSGXCHANGE_SOVERSION})
set_property(TARGET vsgXchange PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET vsgXchange PROPERTY CXX_STANDARD 17)

target_compile_definitions(vsgXchange PRIVATE ${EXTRA_DEFINES})

target_include_directories(vsgXchange
    PUBLIC
        $<BUILD_INTERFACE:${VSGXCHANGE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${VSGXCHANGE_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        $<BUILD_INTERFACE:${VSGXCHANGE_SOURCE_DIR}/src/ktx/libktx>
        ${EXTRA_INCLUDES}
)

target_link_libraries(vsgXchange
    PUBLIC
        vsg::vsg
    PRIVATE
        ${EXTRA_LIBRARIES}
)

install(TARGETS vsgXchange EXPORT vsgXchangeTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
)

if (BUILD_SHARED_LIBS)
    target_compile_definitions(vsgXchange INTERFACE VSGXCHANGE_SHARED_LIBRARY)
endif()


install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/vsgXchange DESTINATION include)
if (NOT(${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR}))
    install(DIRECTORY ${CMAKE_BINARY_DIR}/include/vsgXchange DESTINATION include)
endif()



# [==[
install(EXPORT vsgXchangeTargets
    FILE vsgXchangeTargets.cmake
    NAMESPACE vsgXchange::
    DESTINATION lib/cmake/vsgXchange
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file("${CMAKE_BINARY_DIR}/src/vsgXchangeConfigVersion.cmake"
    VERSION ${VSGXCHANGE_MAJOR_VERSION}.${VSGXCHANGE_MINOR_VERSION}.${VSGXCHANGE_PATCH_VERSION}
    COMPATIBILITY SameMajorVersion
)

string(REPLACE ";" "\n" FIND_DEPENDENCY_OUT "${FIND_DEPENDENCY}")
configure_file("${CMAKE_SOURCE_DIR}/src/vsgXchangeConfig.cmake.in" "${CMAKE_BINARY_DIR}/src/vsgXchangeConfig.cmake" @ONLY)

install(FILES "${CMAKE_BINARY_DIR}/src/vsgXchangeConfig.cmake" "${CMAKE_BINARY_DIR}/src/vsgXchangeConfigVersion.cmake"
    DESTINATION lib/cmake/vsgXchange
)

# ]==]
