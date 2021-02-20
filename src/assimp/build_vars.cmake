# add assimp if vailable
find_package(assimp)

set(SOURCES ${SOURCES}
    assimp/assimp.cpp
)
if(assimp_FOUND)
    set(EXTRA_INCLUDES ${EXTRA_INCLUDES} ${assimp_INCLUDE_DIRS})
    set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} assimp::assimp)
    set(EXTRA_DEFINES ${EXTRA_DEFINES} USE_ASSIMP)
    set(FIND_DEPENDENCY ${FIND_DEPENDENCY} "find_dependency(assimp)")
endif()
