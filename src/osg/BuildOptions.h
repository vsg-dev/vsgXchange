#pragma once

#include <vsg/all.h>
#include <vsgXchange/models.h>

#include "GeometryUtils.h"
#include "ShaderUtils.h"

namespace osg2vsg
{
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

        std::string vertexShaderPath = "";
        std::string fragmentShaderPath = "";

        vsg::Path extension = "vsgb";

        vsg::ref_ptr<PipelineCache> pipelineCache;
    };
} // namespace osg2vsg

EVSG_type_name(osg2vsg::BuildOptions);
