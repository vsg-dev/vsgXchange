# add assimp if vailable
find_package(assimp 5.0 QUIET)

if(assimp_FOUND)
    OPTION(vsgXchange_assimp "Optional Assimp support provided" ON)
endif()

if (${vsgXchange_assimp})
    set(SOURCES ${SOURCES}
        assimp/assimp.cpp
    )
    set(EXTRA_INCLUDES ${EXTRA_INCLUDES} ${assimp_INCLUDE_DIRS})
    set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} assimp::assimp)
    if(NOT BUILD_SHARED_LIBS)
        set(FIND_DEPENDENCY ${FIND_DEPENDENCY} "find_dependency(assimp)")
    endif()
else()
    set(SOURCES ${SOURCES}
        assimp/assimp_fallback.cpp
    )
endif()
