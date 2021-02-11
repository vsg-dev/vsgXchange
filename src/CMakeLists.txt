
# vars used to enable subdirectories to extend the build of the vsgXchange library in a loose coupled way
set(EXTRA_DEFINES)
set(EXTRA_INCLUDES)

SET(HEADER_PATH ${CMAKE_SOURCE_DIR}/include/vsgXchange)

set(HEADERS
    ${HEADER_PATH}/Export.h
    ${HEADER_PATH}/ReaderWriter_all.h
 )

set(SOURCES
    vsgXchange/ReaderWriter_all.cpp

    glsl/ReaderWriter_glsl.cpp

    spirv/ReaderWriter_spirv.cpp

    cpp/ReaderWriter_cpp.cpp

    stbi/ReaderWriter_stbi.cpp

    dds/ReaderWriter_dds.cpp
)

# add freetype if available
include(freetype/build_vars.cmake)

# add freetype if available
include(assimp/build_vars.cmake)

# add ktx
include(ktx/build_vars.cmake)

# add OpenSceneGraph if available
include(osg/build_vars.cmake)



add_library(vsgXchange ${HEADERS} ${SOURCES})


set_property(TARGET vsgXchange PROPERTY VERSION ${VSGXCHANGE_VERSION_MAJOR}.${VSGXCHANGE_VERSION_MINOR}.${VSGXCHANGE_VERSION_PATCH})
set_property(TARGET vsgXchange PROPERTY SOVERSION ${VSGXCHANGE_SOVERSION})
set_property(TARGET vsgXchange PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET vsgXchange PROPERTY CXX_STANDARD 17)

target_compile_definitions(vsgXchange PRIVATE ${EXTRA_DEFINES})

target_include_directories(vsgXchange PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/ktx/libktx>
    $<INSTALL_INTERFACE:include>
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
