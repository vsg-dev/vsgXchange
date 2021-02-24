# add freetype if available
find_package(Freetype)

if(FREETYPE_FOUND)
    OPTION(vsgXchange_freetype "Freetype support provided" ON)
endif()

if(${vsgXchange_freetype})
    set(SOURCES ${SOURCES}
        freetype/freetype.cpp
    )
    set(EXTRA_INCLUDES ${EXTRA_INCLUDES} ${FREETYPE_INCLUDE_DIRS})
    set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} ${FREETYPE_LIBRARIES})
    set(EXTRA_DEFINES ${EXTRA_DEFINES} USE_FREETYPE)
else()
    set(SOURCES ${SOURCES}
        freetype/freetype_fallback.cpp
    )
endif()
