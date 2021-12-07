#pragma once

#include <vsg/all.h>

#include "GeometryUtils.h"
#include "ShaderUtils.h"

namespace osg2vsg
{
    struct PipelineCache : public vsg::Inherit<vsg::Object, PipelineCache>
    {
        using Key = std::tuple<uint32_t, uint32_t, std::string, std::string>;
        using PipelineMap = std::map<Key, vsg::ref_ptr<vsg::BindGraphicsPipeline>>;

        std::mutex mutex;
        PipelineMap pipelineMap;

        vsg::ref_ptr<vsg::BindGraphicsPipeline> getOrCreateBindGraphicsPipeline(uint32_t shaderModeMask, uint32_t geometryMask, const std::string& vertShaderPath = "", const std::string& fragShaderPath = "");
    };

    struct BuildOptions : public vsg::Inherit<vsg::Object, BuildOptions>
    {
        vsg::ref_ptr<const vsg::Options> options;

        virtual void read(vsg::Input& input);
        virtual void write(vsg::Output& output) const;

        bool insertCullGroups = true;
        bool insertCullNodes = true;
        bool useBindDescriptorSet = true;
        bool billboardTransform = false;

        GeometryTarget geometryTarget = VSG_VERTEXINDEXDRAW;

        uint32_t supportedGeometryAttributes = GeometryAttributes::ALL_ATTS;
        uint32_t supportedShaderModeMask = ShaderModeMask::ALL_SHADER_MODE_MASK;
        uint32_t overrideGeomAttributes = 0;
        uint32_t overrideShaderModeMask = ShaderModeMask::NONE;
        bool useDepthSorted = true;

        bool mapRGBtoRGBAHint = true;
        bool copyNames = true;

        std::string vertexShaderPath = "";
        std::string fragmentShaderPath = "";

        vsg::Path extension = "vsgb";

        vsg::ref_ptr<PipelineCache> pipelineCache;
    };
} // namespace osg2vsg

EVSG_type_name(osg2vsg::BuildOptions);
