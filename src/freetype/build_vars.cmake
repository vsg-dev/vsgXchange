# add freetype if available
find_package(Freetype)
if(FREETYPE_FOUND)
    set(SOURCES ${SOURCES}
        freetype/FreeTypeFont.cpp
    )
    set(EXTRA_INCLUDES ${EXTRA_INCLUDES} ${FREETYPE_INCLUDE_DIRS})
    set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} ${FREETYPE_LIBRARIES})
    set(EXTRA_DEFINES ${EXTRA_DEFINES} USE_FREETYPE)
endif()
