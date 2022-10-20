if(NOT ANDROID)
    find_package(Threads)
endif()

if (UNIX)
    find_library(DL_LIBRARY dl)
endif()

set(SOURCES
    vsgshaderset.cpp
)

add_executable(vsgshaderset ${SOURCES})

target_include_directories(vsgshaderset PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

set_target_properties(vsgshaderset PROPERTIES OUTPUT_NAME vsgshaderset DEBUG_POSTFIX "d")

target_link_libraries(vsgshaderset
    vsgXchange
    vsg::vsg
)

install(TARGETS vsgshaderset
        RUNTIME DESTINATION bin
)
