#pragma once

#include <vsg/all.h>

#include <osg/Array>
#include <osg/Geometry>
#include <osg/Material>

namespace osg2vsg
{
    enum GeometryAttributes : uint32_t
    {
        VERTEX = 1,
        NORMAL = 2,
        NORMAL_OVERALL = 4,
        TANGENT = 8,
        TANGENT_OVERALL = 16,
        COLOR = 32,
        COLOR_OVERALL = 64,
        TEXCOORD0 = 128,
        TEXCOORD1 = 256,
        TEXCOORD2 = 512,
        TRANSLATE = 1024,
        TRANSLATE_OVERALL = 2048,
        STANDARD_ATTS = VERTEX | NORMAL | TANGENT | COLOR | TEXCOORD0,
        ALL_ATTS = VERTEX | NORMAL | NORMAL_OVERALL | TANGENT | TANGENT_OVERALL | COLOR | COLOR_OVERALL | TEXCOORD0 | TEXCOORD1 | TEXCOORD2 | TRANSLATE | TRANSLATE_OVERALL
    };

    enum AttributeChannels : uint32_t
    {
        VERTEX_CHANNEL = 0,    // osg 0
        NORMAL_CHANNEL = 1,    // osg 1
        TANGENT_CHANNEL = 2,   //osg 6
        COLOR_CHANNEL = 3,     // osg 2
        TEXCOORD0_CHANNEL = 4, //osg 3
        TEXCOORD1_CHANNEL = 5,
        TEXCOORD2_CHANNEL = 6,
        TRANSLATE_CHANNEL = 7
    };

    enum GeometryTarget : uint32_t
    {
        VSG_GEOMETRY,
        VSG_VERTEXINDEXDRAW,
        VSG_COMMANDS
    };

    vsg::ref_ptr<vsg::vec2Array> convertToVsg(const osg::Vec2Array* inarray, uint32_t bindOverallPaddingCount);

    vsg::ref_ptr<vsg::vec3Array> convertToVsg(const osg::Vec3Array* inarray, uint32_t bindOverallPaddingCount);

    vsg::ref_ptr<vsg::vec4Array> convertToVsg(const osg::Vec4Array* inarray, uint32_t bindOverallPaddingCount);

    vsg::ref_ptr<vsg::Data> convertToVsg(const osg::Array* inarray, uint32_t bindOverallPaddingCount);

    uint32_t calculateAttributesMask(const osg::Geometry* geometry);

    VkSamplerAddressMode covertToSamplerAddressMode(osg::Texture::WrapMode wrapmode);

    std::pair<VkFilter, VkSamplerMipmapMode> convertToFilterAndMipmapMode(osg::Texture::FilterMode filtermode);

    vsg::ref_ptr<vsg::Sampler> convertToSampler(const osg::Texture* texture);

    vsg::ref_ptr<vsg::materialValue> convertToMaterialValue(const osg::Material* material);

    vsg::ref_ptr<vsg::Command> convertToVsg(osg::Geometry* geometry, uint32_t requiredAttributesMask, GeometryTarget geometryTarget);

} // namespace osg2vsg
