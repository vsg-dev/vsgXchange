# Find OpenScenegraph
find_package(OpenThreads)
find_package(osg)
find_package(osgDB)
find_package(osgTerrain)
find_package(osgUtil)
find_package(OpenGL)

if(OSG_FOUND AND OSGDB_FOUND AND OSGTERRAIN_FOUND AND OSGUTIL_FOUND AND OPENGL_FOUND)
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
else()
    set(SOURCES ${SOURCES}
        osg/OSG_fallback.cpp
    )
endif()
