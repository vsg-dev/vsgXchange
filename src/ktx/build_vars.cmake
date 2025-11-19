# add CURL if available
# find_package(ktx)

find_package(Ktx)

if(Ktx_FOUND)
    OPTION(vsgXchange_ktx "Optional KTX support provided" ON)
endif()


if(${vsgXchange_ktx})
    set(SOURCES ${SOURCES}
        ktx/ktx.cpp
    )
    set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} KTX::ktx)
    if(NOT BUILD_SHARED_LIBS)
        set(FIND_DEPENDENCY ${FIND_DEPENDENCY} "find_dependency(Ktx)")
    endif()

else()
    set(SOURCES ${SOURCES}
        ktx/ktx_fallback.cpp
    )
endif()
