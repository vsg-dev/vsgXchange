# add freetype if available
find_package(Freetype)

if(FREETYPE_FOUND)
    OPTION(vsgXchange_freetype "Freetype support provided" ON)
endif()

if(${vsgXchange_freetype})
    set(SOURCES ${SOURCES}
        freetype/freetype.cpp
    )
    if(TARGET Freetype::Freetype)
        set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} Freetype::Freetype)
    else()
        set(EXTRA_INCLUDES ${EXTRA_INCLUDES} ${FREETYPE_INCLUDE_DIRS})
        set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} ${FREETYPE_LIBRARIES})
    endif() 
    set(EXTRA_DEFINES ${EXTRA_DEFINES} USE_FREETYPE)

    if(NOT BUILD_SHARED_LIBS)
        set(FIND_DEPENDENCY ${FIND_DEPENDENCY} "find_dependency(Freetype)")
    endif()
else()
    set(SOURCES ${SOURCES}
        freetype/freetype_fallback.cpp
    )
endif()
