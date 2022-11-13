# add freetype if available
# first check for modern freetype config file
find_package(freetype CONFIG QUIET)

# if freetype config not found fallback to using the original FindFreetype.cmake module
if (NOT freetype_FOUND)
    find_package(Freetype)
endif()

if(freetype_FOUND OR FREETYPE_FOUND)
    OPTION(vsgXchange_freetype "Freetype support provided" ON)
endif()

if(${vsgXchange_freetype})
    set(SOURCES ${SOURCES}
        freetype/freetype.cpp
    )

    set(EXTRA_DEFINES ${EXTRA_DEFINES} USE_FREETYPE)

    if (freetype_FOUND)
        # new freetype config usage
        set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} Freetype::Freetype)
        if(NOT BUILD_SHARED_LIBS)
            set(FIND_DEPENDENCY ${FIND_DEPENDENCY} "find_dependency(freetype)")
        endif()
    else()
        # old Freetype module usage
        set(EXTRA_INCLUDES ${EXTRA_INCLUDES} ${FREETYPE_INCLUDE_DIRS})
        set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} ${FREETYPE_LIBRARIES})

        if(NOT BUILD_SHARED_LIBS)
            set(FIND_DEPENDENCY ${FIND_DEPENDENCY} "find_dependency(Freetype)")
        endif()
    endif()
else()
    set(SOURCES ${SOURCES}
        freetype/freetype_fallback.cpp
    )
endif()
