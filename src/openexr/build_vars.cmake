# add openexr if available
find_package(OpenEXR QUIET)

if(OpenEXR_FOUND)
    OPTION(vsgXchange_openexr "Optional OpenEXR support provided" ON)
endif()

if (${vsgXchange_openexr})
    set(SOURCES ${SOURCES}
        openexr/openexr.cpp
    )
    if(OpenEXR_VERSION VERSION_LESS "3.0")
        set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} OpenEXR::IlmImf)
    else()
        add_compile_definitions(EXRVERSION3)
        set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} OpenEXR::OpenEXR)
    endif()
    set(EXTRA_INCLUDES ${EXTRA_INCLUDES} ${OpenEXR_INCLUDE_DIRS})

    if(NOT BUILD_SHARED_LIBS)
        set(FIND_DEPENDENCY ${FIND_DEPENDENCY} "find_dependency(OpenEXR)")
    endif()
else()
    set(SOURCES ${SOURCES}
        openexr/openexr_fallback.cpp
    )
endif()
