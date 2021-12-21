# Find OpenScenegraph
set(OSG_MIN_VERSION 3.6)
find_package(OpenThreads ${OSG_MIN_VERSION})
find_package(osg ${OSG_MIN_VERSION})
find_package(osgDB ${OSG_MIN_VERSION})
find_package(osgTerrain ${OSG_MIN_VERSION})
find_package(osgUtil ${OSG_MIN_VERSION})

if(OSG_FOUND AND OSGDB_FOUND AND OSGTERRAIN_FOUND AND OSGUTIL_FOUND)
    OPTION(vsgXchange_OSG "Optional OpenSceneGraph support provided" ON)
endif()

if (${vsgXchange_OSG})
    set(SOURCES ${SOURCES}
        osg/BuildOptions.cpp
        osg/GeometryUtils.cpp
        osg/ImageUtils.cpp
        osg/Optimize.cpp
        osg/SceneAnalysis.cpp
        osg/SceneBuilder.cpp
        osg/ShaderUtils.cpp
        osg/ConvertToVsg.cpp
        osg/OSG.cpp
    )
    set(EXTRA_INCLUDES ${EXTRA_INCLUDES} ${OSG_INCLUDE_DIR})
    set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} ${OPENTHREADS_LIBRARIES} ${OSG_LIBRARIES} ${OSGUTIL_LIBRARIES} ${OSGDB_LIBRARIES} ${OSGTERRAIN_LIBRARIES})
    if(NOT BUILD_SHARED_LIBS)
        set(FIND_DEPENDENCY ${FIND_DEPENDENCY} "find_dependency(OpenThreads)" "find_dependency(osg)" "find_dependency(osgDB)" "find_dependency(osgTerrain)" "find_dependency(osgUtil)")
    endif()
else()
    set(SOURCES ${SOURCES}
        osg/OSG_fallback.cpp
    )
endif()
