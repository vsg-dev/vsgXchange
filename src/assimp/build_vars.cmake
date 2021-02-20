# add assimp if vailable
find_package(assimp 5.0.0)

if(assimp_FOUND)
    set(SOURCES ${SOURCES}
        assimp/assimp.cpp
    )
    set(EXTRA_INCLUDES ${EXTRA_INCLUDES} ${assimp_INCLUDE_DIRS})
    set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} assimp::assimp)
    set(EXTRA_DEFINES ${EXTRA_DEFINES} USE_ASSIMP)
    set(FIND_DEPENDENCY ${FIND_DEPENDENCY} "find_dependency(assimp)")
else()
    set(SOURCES ${SOURCES}
        assimp/assimp_fallback.cpp
    )
endif()
