# Compile ktx read library
set(KTX_SOURCES
    ktx/libktx/ktx.h
#    ktx/libktx/basis_sgd.h
    ktx/libktx/checkheader.c
    ktx/libktx/dfdutils/createdfd.c
#    ktx/libktx/dfdutils/colourspaces.c
    ktx/libktx/dfdutils/dfd.h
    ktx/libktx/dfdutils/dfd2vk.inl
    ktx/libktx/dfdutils/interpretdfd.c
    ktx/libktx/dfdutils/printdfd.c
    ktx/libktx/dfdutils/queries.c
    ktx/libktx/dfdutils/vk2dfd.c
#    ktx/libktx/dfdutils/vulkan/vk_platform.h
#    ktx/libktx/dfdutils/vulkan/vulkan_core.h
#    ktx/libktx/etcdec.cxx
#    ktx/libktx/etcunpack.cxx
    ktx/libktx/filestream.c
    ktx/libktx/filestream.h
#    ktx/libktx/formatsize.h
#    ktx/libktx/gl_format.h
#    ktx/libktx/gl_funcs.c
#    ktx/libktx/gl_funcs.h
#    ktx/libktx/glloader.c
    ktx/libktx/hashlist.c
#    ktx/libktx/info.c
    ktx/libktx/ktxint.h
    ktx/libktx/memstream.c
    ktx/libktx/memstream.h
#    ktx/libktx/stream.h
#    ktx/libktx/strings.c
    ktx/libktx/swap.c
    ktx/libktx/texture.c
    ktx/libktx/texture.h
    ktx/libktx/texture1.c
    ktx/libktx/texture1.h
    ktx/libktx/texture2.c
    ktx/libktx/texture2.h
#    ktx/libktx/uthash.h
#    ktx/libktx/vk_format.h
#    ktx/libktx/vkformat_check.c
#    ktx/libktx/vkformat_enum.h
#    ktx/libktx/vkformat_str.c
#    ktx/libktx/ktxvulkan.h
    ktx/libktx/vkloader.c
    ktx/libktx/zstddeclib.c
)
source_group(libktx FILES ${KTX_SOURCES})
set(SOURCES ${SOURCES} ${KTX_SOURCES} ktx/ktx.cpp)
set(EXTRA_DEFINES ${EXTRA_DEFINES} KHRONOS_STATIC LIBKTX BASISD_SUPPORT_FXT1=0 BASISU_NO_ITERATOR_DEBUG_LEVEL KTX_FEATURE_KTX1 KTX_FEATURE_KTX2)
