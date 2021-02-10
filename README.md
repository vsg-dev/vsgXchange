# vsgXchange
Utility library for converting data+materials to/from [VulkanSceneGraph](https://github.com/vsg-dev/VulkanSceneGraph)

## Provides Builtin read support for:

* reading DDS, JPEG, PNG, GIF, BMP, TGA and PSD (Photoshop Document) image formats as vsg::Data objects.
* reading GLSL shader files as vsg::ShaderStage objects.
* reading and writing SPIRV shader files as vsg::ShaderModule.
* writeng vsg::Object of all types to .cpp source files that can be directly compiled into applications

## Optional support:

cmake automatically finds which depedencies are available and builds the approprioate components:

* reading TrueType etc. fonts support by FreeType as vsg::Font objects.
* reading 3d model formats ( GLTF, OBJ, 3DS, LWO) etc. supported by Assimp as vsg::Node objects.
* reading 3d modul formats (OpenSceneGraph, OpenFlight etc.) supported by OpenSceneGraph as vsg::Node objecs.
* Comming soon : reading KTX images supported by libktx as vsg::Data objects.

## Required dependencies:

* [VulkanSDK](https://www.lunarg.com/vulkan-sdk/)
* [VulkanSceneGraph](https://github.com/vsg-dev/VulkanSceneGraph)
* [CMake](https://cmake.org/) minimum version 3.7
* C++17 capable compiler

## Options depdendencies:

* [Freetype](https://www.freetype.org/)
* [Assimp](https://www.assimp.org/) & [Assimp on githib](https://github.com/assimp/assimp)
* [OpenSceneGraph](http://www.openscenegraph.org/) & [OpenSceneGraph on githib](https://github.com/openscenegraph/OpenSceneGraph)

## Building vsgXchange:

### Unix:

In source build:

    cmake .
    make -j 8
    sudo make install

### Windows:

To be filled in by a kindly Window dev :-)

## How to use vsgXchange in your own applications

CMake additions:

    find_package(vsgXchange)

    target_link_libraries(myapplication vsgXchange::vsgXchange)

C++ additions:

    #include <vsgXchange/ReaderWriter_all.h>

    ...

    // assign a composite ReaderWriter that includes all supported formats
    auto options = vsg::Options::create(vsgXchange::ReaderWriter_all::create());

    // pass in the options that provides the link to the ReaderWriter of interest.
    auto object = vsg::read("myimage.dds", options);

    // read file and cast to vsg::Data if possible, returns vsg::ref_ptr<vsg::Data>
    auto image = vsg::read_cast<vsg::Data>("myimage.dds", options);

    // read file and cast to vsg::Node if possible, returns vsg::ref_ptr<vsg::Node>
    auto model = vsg::read_cast<vsg::Object>("mymodel.gltf", options);

## How to use vsgconv utility that is built as part of vsgXchange

To convert shaders to SPIRV, native VSG format or source file:

    vsgconv myshader.vert myshader.vsgb
    vsgconv myshader.frag myshader.spv
    vsgconv myshader.comp myshader_comp.cpp

To convet 3rd part image formats to native VSG format or source file:

    vsgconv image.jpg image.vsgb
    vsgconv image.jpg image.cpp

To convet 3rd part model formats to native VSG format or source file:

    vsgconv mymodel.obj mymodel.vsgt # convert OBJ model to VSG ascii text format (requires Assump)
    vsgconv mymodel.gltif mymodel.vsgt # convert GLTF model to VSG ascii text format  (requires Assump)
    vsgocnv mymodel.osgb mymodel.vsgb # convert OSG binary format to VSG binary format (requires OpenSceneGraph)
    vsgocnv mymodel.flt mymodel.vsgb # convert OpenFlight format to VSG binary format (requires OpenSceneGraph)
    vsgocnv mymodel.vsgb mymodel.cpp # convert native VSG binary format to source file.


To convert a OpenSceneGraph Paged database:

    vsgconv OsgDatabase/earth.osgb VsgDatabase/earth.vsgb -l 30 -n 50000 # convert up to level 30, and default to needing 50,000 Vk Descriptors when rendering
