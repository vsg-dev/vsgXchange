# vsgXchange
Utility library for converting 3rd party images, models and fonts formats to/from [VulkanSceneGraph](https://github.com/vsg-dev/VulkanSceneGraph)

## Provides builtin support for:

vsgXchange contains source code that can directly read a [range of shader and image formats](#file-formats-supported-by-built-in-readerwriters).
* reading KTX, DDS, JPEG, PNG, GIF, BMP, TGA and PSD image formats as vsg::Data objects.
* reading GLSL shader files as vsg::ShaderStage objects.
* reading and writing SPIRV shader files as vsg::ShaderModule.
* writing vsg::Object of all types to .cpp source files that can be directly compiled into applications.

## Optional support:

cmake automatically finds which dependencies are available and builds the appropriate components:

* [reading font formats](#font-file-formats-supported-by-optional-vsgxchangefreetype) TrueType etc. using [Freetype](https://www.freetype.org/) as vsg::Font.
* [reading image & DEM formats](#image-formats-supported-by-optional-vsgxchangegdal) .exr using [OpenEXR](https://www.openexr.com/), and GeoTiff etc. using [GDAL](https://gdal.org/) as vsg::Data.
* [reading 3d model formats](#model-formats-supported-by-optional-vsgxchangeassimp)  GLTF, OBJ, 3DS, LWO etc. using Assimp as vsg::Node.
* [reading data over the internet](#protocols-supported-by-optional-vsgxchangecurl) reading image and model files from http:// and https:// using [libcurl](https://curl.se/libcurl/)
* [reading image and 3d model formats](#image-and-model-formats-supported-by-optional-vsgxchangeosg) OpenSceneGraph, OpenFlight etc. using [osg2vsg](https://github.com/vsg-dev/osg2vsg)/[OpenSceneGraph](http://www.openscenegraph.org/).

## Required dependencies:

* [VulkanSDK](https://www.lunarg.com/vulkan-sdk/)
* [VulkanSceneGraph](https://github.com/vsg-dev/VulkanSceneGraph)
* [CMake](https://cmake.org/) minimum version 3.7
* C++17 capable compiler

## Optional dependencies:

* [Freetype](https://www.freetype.org/)
* [GDAL](https://gdal.org/)
* [Assimp](https://www.assimp.org/), [Assimp on github](https://github.com/assimp/assimp)
* [libcurl](https://curl.se/libcurl/)
* [OpenEXP](https://www.openexr.com/)
* [osg2vsg](https://github.com/vsg-dev/osg2vsg)

## Building vsgXchange:

### Unix:

In source build:

    cmake .
    make -j 8
    sudo make install

### Windows:

Using cmake:
```
mkdir build && cd build
cmake ..
cmake --build .
```

## How to use vsgXchange in your own applications

CMake additions:

    find_package(vsgXchange)

    target_link_libraries(myapplication vsgXchange::vsgXchange)

C++ additions:

    #include <vsgXchange/all.h>

    ...

    // assign a composite ReaderWriter that includes all supported formats
    auto options = vsg::Options::create(vsgXchange::all::create());

    // pass in the options that provide the link to the ReaderWriter of interest.
    auto object = vsg::read("myimage.dds", options);

    // read file and cast to vsg::Data if possible, returns vsg::ref_ptr<vsg::Data>
    auto image = vsg::read_cast<vsg::Data>("myimage.dds", options);

    // read file and cast to vsg::Node if possible, returns vsg::ref_ptr<vsg::Node>
    auto model = vsg::read_cast<vsg::Node>("mymodel.gltf", options);

    // write file
    vsg::write(model, "mymodel.vsgb", options);

## How to use vsgconv utility that is built as part of vsgXchange

To convert shaders to SPIRV, native VSG format or source file:

    vsgconv myshader.vert myshader.vsgb
    vsgconv myshader.frag myshader.spv
    vsgconv myshader.comp myshader_comp.cpp

To convert 3rd party image formats to native VSG format or source file:

    vsgconv image.jpg image.vsgb
    vsgconv image.jpg image.cpp

To convert 3rd party model formats to native VSG format or source file:

    vsgconv mymodel.obj mymodel.vsgt # convert OBJ model to VSG ascii text format (requires Assimp)
    vsgconv mymodel.gltif mymodel.vsgt # convert GLTF model to VSG ascii text format  (requires Assimp)
    vsgconv mymodel.osgb mymodel.vsgb # convert OSG binary format to VSG binary format (requires osg2vsg/OpenSceneGraph)
    vsgconv mymodel.flt mymodel.vsgb # convert OpenFlight format to VSG binary format (requires osg2vsg/OpenSceneGraph)
    vsgconv mymodel.vsgb mymodel.cpp # convert native VSG binary format to source file.

To convert an OpenSceneGraph Paged database (requires osg2vsg/OpenSceneGraph):

    vsgconv OsgDatabase/earth.osgb VsgDatabase/earth.vsgb -l 30 # convert up to level 30

Options specific to a ReaderWriter are specified on the commandline using two dashes:

    vsgconv FlightHelmet.gltf helmet.vsgb --discard_empty_nodes false

## File formats supported by all built in ReaderWriters

    vsgXchange::all
        vsgXchange::curl provides support for 0 extensions, and 2 protocols.
            Protocols       Supported ReaderWriter methods
            ----------      ------------------------------
            http            read(vsg::Path, ..)
            https           read(vsg::Path, ..)

            vsg::Options::Value  type
            -------------------  ----
            CURLOPT_SSL_OPTIONS  uint32_t

        vsg::VSG provides support for 2 extensions, and 0 protocols.
            Extensions      Supported ReaderWriter methods
            ----------      ------------------------------
            .vsgb           read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..) write(vsg::Path, ..) write(std::ostream, ..)
            .vsgt           read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..) write(vsg::Path, ..) write(std::ostream, ..)

        vsg::spirv provides support for 1 extensions, and 0 protocols.
            Extensions      Supported ReaderWriter methods
            ----------      ------------------------------
            .spv            read(vsg::Path, ..) write(vsg::Path, ..)

        vsg::glsl provides support for 16 extensions, and 0 protocols.
            Extensions      Supported ReaderWriter methods
            ----------      ------------------------------
            .comp           read(vsg::Path, ..) write(vsg::Path, ..)
            .frag           read(vsg::Path, ..) write(vsg::Path, ..)
            .geom           read(vsg::Path, ..) write(vsg::Path, ..)
            .glsl           read(vsg::Path, ..) write(vsg::Path, ..)
            .hlsl           read(vsg::Path, ..) write(vsg::Path, ..)
            .mesh           read(vsg::Path, ..) write(vsg::Path, ..)
            .rahit          read(vsg::Path, ..) write(vsg::Path, ..)
            .rcall          read(vsg::Path, ..) write(vsg::Path, ..)
            .rchit          read(vsg::Path, ..) write(vsg::Path, ..)
            .rgen           read(vsg::Path, ..) write(vsg::Path, ..)
            .rint           read(vsg::Path, ..) write(vsg::Path, ..)
            .rmiss          read(vsg::Path, ..) write(vsg::Path, ..)
            .task           read(vsg::Path, ..) write(vsg::Path, ..)
            .tesc           read(vsg::Path, ..) write(vsg::Path, ..)
            .tese           read(vsg::Path, ..) write(vsg::Path, ..)
            .vert           read(vsg::Path, ..) write(vsg::Path, ..)

        vsg::txt provides support for 7 extensions, and 0 protocols.
            Extensions      Supported ReaderWriter methods
            ----------      ------------------------------
            .bat            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .json           read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .md             read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .sh             read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .text           read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .txt            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .xml            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)

        vsgXchange::cpp provides support for 1 extensions, and 0 protocols.
            Extensions      Supported ReaderWriter methods
            ----------      ------------------------------
            .cpp            write(vsg::Path, ..)

        vsgXchange::stbi provides support for 9 extensions, and 0 protocols.
            Extensions      Supported ReaderWriter methods
            ----------      ------------------------------
            .bmp            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..) write(vsg::Path, ..) write(std::ostream, ..)
            .jpe            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..) write(vsg::Path, ..) write(std::ostream, ..)
            .jpeg           read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..) write(vsg::Path, ..) write(std::ostream, ..)
            .jpg            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..) write(vsg::Path, ..) write(std::ostream, ..)
            .pgm            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .png            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..) write(vsg::Path, ..) write(std::ostream, ..)
            .ppm            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .psd            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .tga            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..) write(vsg::Path, ..) write(std::ostream, ..)

            vsg::Options::Value  type
            -------------------  ----
            image_format         vsg::CoordinateSpace
            jpeg_quality         int

        vsgXchange::dds provides support for 1 extensions, and 0 protocols.
            Extensions      Supported ReaderWriter methods
            ----------      ------------------------------
            .dds            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)

        vsgXchange::ktx provides support for 2 extensions, and 0 protocols.
            Extensions      Supported ReaderWriter methods
            ----------      ------------------------------
            .ktx            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .ktx2           read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)

        vsgXchange::openexr provides support for 1 extensions, and 0 protocols.
            Extensions      Supported ReaderWriter methods
            ----------      ------------------------------
            .exr            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..) write(vsg::Path, ..) write(std::ostream, ..)

        vsgXchange::freetype provides support for 11 extensions, and 0 protocols.
            Extensions      Supported ReaderWriter methods
            ----------      ------------------------------
            .cef            read(vsg::Path, ..)
            .cff            read(vsg::Path, ..)
            .cid            read(vsg::Path, ..)
            .fnt            read(vsg::Path, ..)
            .fon            read(vsg::Path, ..)
            .otf            read(vsg::Path, ..)
            .pfa            read(vsg::Path, ..)
            .pfb            read(vsg::Path, ..)
            .ttc            read(vsg::Path, ..)
            .ttf            read(vsg::Path, ..)
            .woff           read(vsg::Path, ..)

            vsg::Options::Value  type
            -------------------  ----
            quad_margin_ratio    float
            texel_margin_ratio   float

        vsgXchange::assimp provides support for 70 extensions, and 0 protocols.
            Extensions      Supported ReaderWriter methods
            ----------      ------------------------------
            .3d             read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .3ds            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .3mf            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .ac             read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .ac3d           read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .acc            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .amf            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .ase            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .ask            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .assbin         read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .b3d            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .blend          read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .bsp            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .bvh            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .cob            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .csm            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .dae            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .dxf            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .enff           read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .fbx            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .glb            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .gltf           read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .hmp            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .ifc            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .ifczip         read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .iqm            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .irr            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .irrmesh        read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .lwo            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .lws            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .lxo            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .md2            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .md3            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .md5anim        read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .md5camera      read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .md5mesh        read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .mdc            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .mdl            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .mesh           read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .mesh.xml       read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .mot            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .ms3d           read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .ndo            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .nff            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .obj            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .off            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .ogex           read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .pk3            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .ply            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .pmx            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .prj            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .q3o            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .q3s            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .raw            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .scn            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .sib            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .smd            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .step           read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .stl            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .stp            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .ter            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .uc             read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .vta            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .x              read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .x3d            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .x3db           read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .xgl            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .xml            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .zae            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)
            .zgl            read(vsg::Path, ..) read(std::istream, ..) read(uint8_t* ptr, size_t size, ..)

            vsg::Options::Value      type
            -------------------      ----
            crease_angle             float
            culling                  bool
            discard_empty_nodes      bool
            external_texture_format  vsgXchange::TextureFormat
            external_textures        bool
            generate_sharp_normals   bool
            generate_smooth_normals  bool
            material_color_space     vsg::CoordinateSpace
            print_assimp             int
            two_sided                bool
            vertex_color_space       vsg::CoordinateSpace

        vsgXchange::GDAL provides support for 96 extensions, and 0 protocols.
            Extensions      Supported ReaderWriter methods
            ----------      ------------------------------
            .ACE2           read(vsg::Path, ..)
            .asc            read(vsg::Path, ..)
            .b              read(vsg::Path, ..)
            .bag            read(vsg::Path, ..)
            .bil            read(vsg::Path, ..)
            .bin            read(vsg::Path, ..)
            .blx            read(vsg::Path, ..)
            .bmp            read(vsg::Path, ..)
            .bt             read(vsg::Path, ..)
            .byn            read(vsg::Path, ..)
            .cal            read(vsg::Path, ..)
            .ct1            read(vsg::Path, ..)
            .cub            read(vsg::Path, ..)
            .dat            read(vsg::Path, ..)
            .ddf            read(vsg::Path, ..)
            .dem            read(vsg::Path, ..)
            .dt0            read(vsg::Path, ..)
            .dt1            read(vsg::Path, ..)
            .dt2            read(vsg::Path, ..)
            .dwg            read(vsg::Path, ..)
            .err            read(vsg::Path, ..)
            .ers            read(vsg::Path, ..)
            .fits           read(vsg::Path, ..)
            .gdb            read(vsg::Path, ..)
            .gen            read(vsg::Path, ..)
            .gff            read(vsg::Path, ..)
            .gif            read(vsg::Path, ..)
            .gpkg           read(vsg::Path, ..)
            .gpkg.zip       read(vsg::Path, ..)
            .grb            read(vsg::Path, ..)
            .grb2           read(vsg::Path, ..)
            .grc            read(vsg::Path, ..)
            .grd            read(vsg::Path, ..)
            .grib2          read(vsg::Path, ..)
            .gsb            read(vsg::Path, ..)
            .gtx            read(vsg::Path, ..)
            .gvb            read(vsg::Path, ..)
            .gxf            read(vsg::Path, ..)
            .h5             read(vsg::Path, ..)
            .hdf            read(vsg::Path, ..)
            .hdf5           read(vsg::Path, ..)
            .hdr            read(vsg::Path, ..)
            .heic           read(vsg::Path, ..)
            .hf2            read(vsg::Path, ..)
            .hgt            read(vsg::Path, ..)
            .img            read(vsg::Path, ..)
            .isg            read(vsg::Path, ..)
            .j2k            read(vsg::Path, ..)
            .jp2            read(vsg::Path, ..)
            .jpeg           read(vsg::Path, ..)
            .jpg            read(vsg::Path, ..)
            .json           read(vsg::Path, ..)
            .kap            read(vsg::Path, ..)
            .kml            read(vsg::Path, ..)
            .kmz            read(vsg::Path, ..)
            .kro            read(vsg::Path, ..)
            .lbl            read(vsg::Path, ..)
            .lcp            read(vsg::Path, ..)
            .map            read(vsg::Path, ..)
            .mbtiles        read(vsg::Path, ..)
            .mem            read(vsg::Path, ..)
            .mpl            read(vsg::Path, ..)
            .mpr            read(vsg::Path, ..)
            .mrf            read(vsg::Path, ..)
            .n1             read(vsg::Path, ..)
            .nat            read(vsg::Path, ..)
            .nc             read(vsg::Path, ..)
            .ntf            read(vsg::Path, ..)
            .pdf            read(vsg::Path, ..)
            .pgm            read(vsg::Path, ..)
            .pix            read(vsg::Path, ..)
            .png            read(vsg::Path, ..)
            .pnm            read(vsg::Path, ..)
            .ppi            read(vsg::Path, ..)
            .ppm            read(vsg::Path, ..)
            .prf            read(vsg::Path, ..)
            .rda            read(vsg::Path, ..)
            .rgb            read(vsg::Path, ..)
            .rik            read(vsg::Path, ..)
            .rst            read(vsg::Path, ..)
            .rsw            read(vsg::Path, ..)
            .sdat           read(vsg::Path, ..)
            .sg-grd-z       read(vsg::Path, ..)
            .sigdem         read(vsg::Path, ..)
            .sqlite         read(vsg::Path, ..)
            .ter            read(vsg::Path, ..)
            .tga            read(vsg::Path, ..)
            .tif            read(vsg::Path, ..)
            .tiff           read(vsg::Path, ..)
            .toc            read(vsg::Path, ..)
            .tpkx           read(vsg::Path, ..)
            .vrt            read(vsg::Path, ..)
            .webp           read(vsg::Path, ..)
            .xml            read(vsg::Path, ..)
            .xpm            read(vsg::Path, ..)
            .xyz            read(vsg::Path, ..)

        osg2vsg::OSG provides support for 132 extensions, and 0 protocols.
            Extensions      Supported ReaderWriter methods
            ----------      ------------------------------
            .*              read(vsg::Path, ..)
            .3dc            read(vsg::Path, ..)
            .3ds            read(vsg::Path, ..)
            .3gp            read(vsg::Path, ..)
            .ac             read(vsg::Path, ..)
            .added          read(vsg::Path, ..)
            .asc            read(vsg::Path, ..)
            .attr           read(vsg::Path, ..)
            .avi            read(vsg::Path, ..)
            .bmp            read(vsg::Path, ..)
            .bvh            read(vsg::Path, ..)
            .bw             read(vsg::Path, ..)
            .cef            read(vsg::Path, ..)
            .cff            read(vsg::Path, ..)
            .cfg            read(vsg::Path, ..)
            .cid            read(vsg::Path, ..)
            .compute        read(vsg::Path, ..)
            .cs             read(vsg::Path, ..)
            .curl           read(vsg::Path, ..)
            .dds            read(vsg::Path, ..)
            .dxf            read(vsg::Path, ..)
            .flt            read(vsg::Path, ..)
            .flv            read(vsg::Path, ..)
            .fnt            read(vsg::Path, ..)
            .fon            read(vsg::Path, ..)
            .frag           read(vsg::Path, ..)
            .fs             read(vsg::Path, ..)
            .gdal           read(vsg::Path, ..)
            .geo            read(vsg::Path, ..)
            .geom           read(vsg::Path, ..)
            .gif            read(vsg::Path, ..)
            .gl             read(vsg::Path, ..)
            .gles           read(vsg::Path, ..)
            .glsl           read(vsg::Path, ..)
            .gs             read(vsg::Path, ..)
            .gz             read(vsg::Path, ..)
            .hdr            read(vsg::Path, ..)
            .int            read(vsg::Path, ..)
            .inta           read(vsg::Path, ..)
            .ive            read(vsg::Path, ..)
            .ivez           read(vsg::Path, ..)
            .jpeg           read(vsg::Path, ..)
            .jpg            read(vsg::Path, ..)
            .ktx            read(vsg::Path, ..)
            .logo           read(vsg::Path, ..)
            .lua            read(vsg::Path, ..)
            .lw             read(vsg::Path, ..)
            .lwo            read(vsg::Path, ..)
            .lws            read(vsg::Path, ..)
            .m2ts           read(vsg::Path, ..)
            .m4v            read(vsg::Path, ..)
            .material       read(vsg::Path, ..)
            .md2            read(vsg::Path, ..)
            .mjpeg          read(vsg::Path, ..)
            .mkv            read(vsg::Path, ..)
            .modified       read(vsg::Path, ..)
            .mov            read(vsg::Path, ..)
            .mp4            read(vsg::Path, ..)
            .mpg            read(vsg::Path, ..)
            .mpv            read(vsg::Path, ..)
            .normals        read(vsg::Path, ..)
            .obj            read(vsg::Path, ..)
            .ogg            read(vsg::Path, ..)
            .ogr            read(vsg::Path, ..)
            .osc            read(vsg::Path, ..)
            .osg            read(vsg::Path, ..)
            .osg2           read(vsg::Path, ..)
            .osga           read(vsg::Path, ..)
            .osgb           read(vsg::Path, ..)
            .osgjs          read(vsg::Path, ..)
            .osgs           read(vsg::Path, ..)
            .osgshadow      read(vsg::Path, ..)
            .osgt           read(vsg::Path, ..)
            .osgterrain     read(vsg::Path, ..)
            .osgtgz         read(vsg::Path, ..)
            .osgviewer      read(vsg::Path, ..)
            .osgx           read(vsg::Path, ..)
            .osgz           read(vsg::Path, ..)
            .otf            read(vsg::Path, ..)
            .path           read(vsg::Path, ..)
            .pbm            read(vsg::Path, ..)
            .pfa            read(vsg::Path, ..)
            .pfb            read(vsg::Path, ..)
            .pgm            read(vsg::Path, ..)
            .pic            read(vsg::Path, ..)
            .pivot_path     read(vsg::Path, ..)
            .ply            read(vsg::Path, ..)
            .png            read(vsg::Path, ..)
            .pnm            read(vsg::Path, ..)
            .pov            read(vsg::Path, ..)
            .ppm            read(vsg::Path, ..)
            .pvr            read(vsg::Path, ..)
            .removed        read(vsg::Path, ..)
            .revisions      read(vsg::Path, ..)
            .rgb            read(vsg::Path, ..)
            .rgba           read(vsg::Path, ..)
            .rot            read(vsg::Path, ..)
            .rotation_path  read(vsg::Path, ..)
            .sav            read(vsg::Path, ..)
            .scale          read(vsg::Path, ..)
            .sdp            read(vsg::Path, ..)
            .sgi            read(vsg::Path, ..)
            .shadow         read(vsg::Path, ..)
            .shp            read(vsg::Path, ..)
            .sta            read(vsg::Path, ..)
            .stl            read(vsg::Path, ..)
            .tctrl          read(vsg::Path, ..)
            .terrain        read(vsg::Path, ..)
            .teval          read(vsg::Path, ..)
            .text3d         read(vsg::Path, ..)
            .tf             read(vsg::Path, ..)
            .tf-255         read(vsg::Path, ..)
            .tga            read(vsg::Path, ..)
            .tgz            read(vsg::Path, ..)
            .tif            read(vsg::Path, ..)
            .tiff           read(vsg::Path, ..)
            .trans          read(vsg::Path, ..)
            .trk            read(vsg::Path, ..)
            .ttc            read(vsg::Path, ..)
            .ttf            read(vsg::Path, ..)
            .txf            read(vsg::Path, ..)
            .txp            read(vsg::Path, ..)
            .vert           read(vsg::Path, ..)
            .view           read(vsg::Path, ..)
            .vs             read(vsg::Path, ..)
            .vsga           read(vsg::Path, ..)
            .vsgb           read(vsg::Path, ..)
            .vsgt           read(vsg::Path, ..)
            .wmv            read(vsg::Path, ..)
            .woff           read(vsg::Path, ..)
            .x              read(vsg::Path, ..)
            .zip            read(vsg::Path, ..)

            vsg::Options::Value  type
            -------------------  ----
            original_converter   bool
            read_build_options   string
            write_build_options  string
