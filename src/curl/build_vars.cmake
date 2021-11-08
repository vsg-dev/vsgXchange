# add CURL if available
find_package(CURL)

if(CURL_FOUND)
    OPTION(vsgXchange_CURL "Optional CURL support provided" ON)
endif()

if(${vsgXchange_CURL})
    set(SOURCES ${SOURCES}
        curl/curl.cpp
    )
    set(EXTRA_INCLUDES ${EXTRA_INCLUDES} ${CURL_INCLUDE_DIR})
    set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} ${CURL_LIBRARIES})
    if(NOT BUILD_SHARED_LIBS)
        set(FIND_DEPENDENCY ${FIND_DEPENDENCY} "find_dependency(CURL)")
    endif()
else()
    set(SOURCES ${SOURCES}
        curl/curl_fallback.cpp
    )
endif()
