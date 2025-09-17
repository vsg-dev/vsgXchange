# add GDAL if available
find_package(GDAL QUIET)

if(GDAL_FOUND)
    OPTION(vsgXchange_GDAL "GDAL support provided" ON)
endif()

if(${vsgXchange_GDAL})
    set(SOURCES ${SOURCES}
        gdal/gdal_utils.cpp
        gdal/meta_utils.cpp
        gdal/GDAL.cpp
    )

    if(TARGET GDAL::GDAL)
        set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} GDAL::GDAL)
    else()
        set(EXTRA_INCLUDES ${EXTRA_INCLUDES} ${GDAL_INCLUDE_DIR})
        set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} ${GDAL_LIBRARY})
    endif() 
    set(EXTRA_DEFINES ${EXTRA_DEFINES} USE_GDAL)
    if(NOT BUILD_SHARED_LIBS)
        set(FIND_DEPENDENCY ${FIND_DEPENDENCY} "find_dependency(GDAL)")
    endif()
else()
    set(SOURCES ${SOURCES}
        gdal/GDAL_fallback.cpp
    )
endif()
