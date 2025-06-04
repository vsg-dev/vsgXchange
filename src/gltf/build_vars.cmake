# add draco support if available
find_package(draco)

if(draco_FOUND)
    OPTION(vsgXchange_draco "Optional glTF draco support provided" ON)
endif()

set(SOURCES ${SOURCES}
    gltf/gltf.cpp
    gltf/SceneGraphBuilder.cpp
)

if (draco_FOUND AND vsgXchange_draco)
    set(EXTRA_INCLUDES ${EXTRA_INCLUDES} ${draco_INCLUDE_DIRS})
    set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} draco::draco)
    if(NOT BUILD_SHARED_LIBS)
        set(FIND_DEPENDENCY ${FIND_DEPENDENCY} "find_dependency(draco)")
    endif()
endif()
