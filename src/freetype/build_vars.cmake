# add freetype if available
find_package(Freetype)

set(SOURCES ${SOURCES}
    freetype/freetype.cpp
)
if(FREETYPE_FOUND)
    set(EXTRA_INCLUDES ${EXTRA_INCLUDES} ${FREETYPE_INCLUDE_DIRS})
    set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} ${FREETYPE_LIBRARIES})
    set(EXTRA_DEFINES ${EXTRA_DEFINES} USE_FREETYPE)
endif()
