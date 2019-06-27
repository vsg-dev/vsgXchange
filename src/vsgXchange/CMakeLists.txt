
SET(HEADER_PATH ${CMAKE_SOURCE_DIR}/include/vsgXchange)

set(HEADERS
    ${HEADER_PATH}/Export.h
    ${HEADER_PATH}/ShaderCompiler.h
)

set(SOURCES
    ShaderCompiler.cpp
    glsllang/ResourceLimits.cpp
)

add_library(vsgXchange ${HEADERS} ${SOURCES})

set_property(TARGET vsgXchange PROPERTY VERSION ${VSGXCHANGE_VERSION_MAJOR}.${VSGXCHANGE_VERSION_MINOR}.${VSGXCHANGE_VERSION_PATCH})
set_property(TARGET vsgXchange PROPERTY SOVERSION ${VSGXCHANGE_SOVERSION})
set_property(TARGET vsgXchange PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET vsgXchange PROPERTY CXX_STANDARD 17)

target_include_directories(vsgXchange PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(vsgXchange PUBLIC
    vsg::vsg
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

include(CMakePackageConfigHelpers)
write_basic_package_version_file("${CMAKE_BINARY_DIR}/src/vsgXchange/vsgXchangeConfigVersion.cmake"
    VERSION ${VSGXCHANGE_MAJOR_VERSION}.${VSGXCHANGE_MINOR_VERSION}.${VSGXCHANGE_PATCH_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES "vsgXchangeConfig.cmake" "${CMAKE_BINARY_DIR}/src/vsgXchange/vsgXchangeConfigVersion.cmake"
    DESTINATION lib/cmake/vsgXchange
)

# ]==]
