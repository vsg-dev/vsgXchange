
SET(HEADER_PATH ${CMAKE_SOURCE_DIR}/include/vsgXchange)

set(HEADERS
    ${HEADER_PATH}/Export.h
    ${HEADER_PATH}/ShaderCompiler.h
    ${HEADER_PATH}/ReaderWriter_all.h
 )

set(SOURCES
    vsgXchange/ShaderCompiler.cpp
    vsgXchange/ReaderWriter_all.cpp

    glsl/ResourceLimits.cpp
    glsl/ReaderWriter_glsl.cpp

    spirv/ReaderWriter_spirv.cpp

    cpp/ReaderWriter_cpp.cpp
)

if (osg2vsg_FOUND)
    set(SOURCES ${SOURCES}
        osg/ReaderWriter_osg.cpp
    )
endif()

add_library(vsgXchange ${HEADERS} ${SOURCES})



# enable OpenSceneGraph integration
if (osg2vsg_FOUND)
    target_compile_definitions(vsgXchange PRIVATE USE_OPENSCENEGRAPH)
    target_link_libraries(vsgXchange PRIVATE osg2vsg::osg2vsg)
    set(FIND_DEPEDENCY_OSG2VSG "find_dependency(osg2vsg)")
endif()



set_property(TARGET vsgXchange PROPERTY VERSION ${VSGXCHANGE_VERSION_MAJOR}.${VSGXCHANGE_VERSION_MINOR}.${VSGXCHANGE_VERSION_PATCH})
set_property(TARGET vsgXchange PROPERTY SOVERSION ${VSGXCHANGE_SOVERSION})
set_property(TARGET vsgXchange PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET vsgXchange PROPERTY CXX_STANDARD 17)

target_include_directories(vsgXchange PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(vsgXchange
    PUBLIC
        vsg::vsg
    PRIVATE
        ${GLSLANG}
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

install(
    FILES glslangConfig.cmake
    DESTINATION lib/cmake/glslang
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file("${CMAKE_BINARY_DIR}/src/vsgXchangeConfigVersion.cmake"
    VERSION ${VSGXCHANGE_MAJOR_VERSION}.${VSGXCHANGE_MINOR_VERSION}.${VSGXCHANGE_PATCH_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_file("${CMAKE_SOURCE_DIR}/src/vsgXchangeConfig.cmake.in" "${CMAKE_BINARY_DIR}/src/vsgXchangeConfig.cmake" @ONLY)

install(FILES "${CMAKE_BINARY_DIR}/src/vsgXchangeConfig.cmake" "${CMAKE_BINARY_DIR}/src/vsgXchangeConfigVersion.cmake"
    DESTINATION lib/cmake/vsgXchange
)

# ]==]
