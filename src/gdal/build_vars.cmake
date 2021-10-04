# add vsgGIS/GDAL if available
find_package(vsgGIS QUIET)

if(vsgGIS_FOUND)
    OPTION(vsgXchange_GDAL "vsgGIS/GDAL support provided" ON)
endif()

if(${vsgXchange_GDAL})
    set(SOURCES ${SOURCES}
        gdal/GDAL.cpp
    )
    set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} vsgGIS::vsgGIS)
    set(EXTRA_DEFINES ${EXTRA_DEFINES} USE_vsgGIS)
    if(NOT BUILD_SHARED_LIBS)
        set(FIND_DEPENDENCY ${FIND_DEPENDENCY} "find_dependency(vsgGIS)")
    endif()
else()
    set(SOURCES ${SOURCES}
        gdal/GDAL_fallback.cpp
    )
endif()
