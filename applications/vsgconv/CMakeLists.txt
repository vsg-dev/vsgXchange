if(NOT ANDROID)
    find_package(Threads)
endif()

if (UNIX)
    find_library(DL_LIBRARY dl)
endif()

set(SOURCES
    vsgconv.cpp
)

add_executable(vsgconv ${SOURCES})

target_include_directories(vsgconv PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

set_target_properties(vsgconv PROPERTIES OUTPUT_NAME vsgconv DEBUG_POSTFIX "d")

target_link_libraries(vsgconv
    vsgXchange
    vsg::vsg
)

install(TARGETS vsgconv
        RUNTIME DESTINATION bin
)
