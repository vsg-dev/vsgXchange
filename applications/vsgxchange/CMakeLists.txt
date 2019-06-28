if(NOT ANDROID)
    find_package(Threads)
endif()

if (UNIX)
    find_library(DL_LIBRARY dl)
endif()

set(SOURCES
    vsgxchange.cpp
    AnimationPath.cpp
)

add_executable(vsgxchange_viewer ${SOURCES})

target_include_directories(vsgxchange_viewer PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    ${OSG_INCLUDE_DIR}
)

set_target_properties(vsgxchange_viewer PROPERTIES OUTPUT_NAME vsgxchange)

target_link_libraries(vsgxchange_viewer
    vsgXchange
    vsg::vsg
    ${GLSLANG}
    ${CMAKE_THREAD_LIBS_INIT}
    Vulkan::Vulkan
)
