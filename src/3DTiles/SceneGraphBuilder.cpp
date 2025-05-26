/* <editor-fold desc="MIT License">

Copyright(c) 2025 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shimages be included in images
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgXchange/3DTiles.h>

#include <vsg/io/read.h>
#include <vsg/nodes/Group.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/nodes/VertexIndexDraw.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/DepthSorted.h>
#include <vsg/nodes/Switch.h>
#include <vsg/nodes/CullNode.h>
#include <vsg/nodes/LOD.h>
#include <vsg/nodes/PagedLOD.h>
#include <vsg/maths/transform.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsg/utils/ComputeBounds.h>
#include <vsg/state/material.h>

using namespace vsgXchange;

Tiles3D::SceneGraphBuilder::SceneGraphBuilder()
{
}

vsg::dmat4 Tiles3D::SceneGraphBuilder::createMatrix(const std::vector<double>& m)
{
    if (m.size()==16)
    {
        return vsg::dmat4(m[0], m[1], m[2], m[3],
                          m[4], m[5], m[6], m[7],
                          m[8], m[9], m[10], m[11],
                          m[12], m[13], m[14], m[15]);
    }
    else
    {
        return {};
    }
}

vsg::dsphere Tiles3D::SceneGraphBuilder::createBound(vsg::ref_ptr<BoundingVolume> boundingVolume)
{
    if (boundingVolume)
    {
        if (boundingVolume->box.values.size()==12)
        {
            const auto& v = boundingVolume->box.values;
            vsg::dvec3 axis_x(v[3], v[4], v[5]);
            vsg::dvec3 axis_y(v[6], v[7], v[8]);
            vsg::dvec3 axis_z(v[9], v[10], v[11]);
            return vsg::dsphere(v[0], v[1], v[2], vsg::length(axis_x + axis_y + axis_z));
        }
        else if (boundingVolume->region.values.size()==6)
        {
            const auto& v = boundingVolume->region.values;
            double west = v[0], south = v[1], east = v[2], north = v[3], low = v[4], high = v[5];
            auto centerECEF = ellipsoidModel->convertLatLongAltitudeToECEF(vsg::dvec3(vsg::degrees(south+north)*0.5, vsg::degrees(west+east)*0.5, (high+low)*0.5));
            auto southWestLowECEF = ellipsoidModel->convertLatLongAltitudeToECEF(vsg::dvec3(vsg::degrees(south), vsg::degrees(west), low));
            auto northEastLowECEF = ellipsoidModel->convertLatLongAltitudeToECEF(vsg::dvec3(vsg::degrees(north), vsg::degrees(east), low));

            // TODO: do we need to track the accumulated transform?
            return vsg::dsphere(centerECEF, std::max(vsg::length(southWestLowECEF - centerECEF), vsg::length(northEastLowECEF - centerECEF)));
        }
        else if (boundingVolume->sphere.values.size()==4)
        {
            const auto& v = boundingVolume->box.values;
            return vsg::dsphere(v[0], v[1], v[2], v[3]);
        }
        else
        {
            vsg::info("createBound() Unhandled boundingVolume type");
            vsg::LogOutput output;
            boundingVolume->report(output);
            return {};
        }
    }
    else
    {
        return {};
    }
}

vsg::ref_ptr<vsg::Node> Tiles3D::SceneGraphBuilder::createTile(vsg::ref_ptr<Tiles3D::Tile> tile, uint32_t level)
{
#if 0
    vsg::info("Tiles3D::createTile() {");
    vsg::info("    boundingVolume = ", tile->boundingVolume);
    vsg::info("    viewerRequestVolume = ", tile->viewerRequestVolume);
    vsg::info("    tile->content = ", tile->geometricError);
    vsg::info("    refine = ", tile->refine);
    vsg::info("    transform = ", tile->transform.values);
    vsg::info("    content = ", tile->content);
#endif

    vsg::dsphere bound = createBound(tile->boundingVolume);

    vsg::ref_ptr<vsg::Node> local_subgraph;

    if (tile->content && !tile->content->uri.empty())
    {
        if (auto model = vsg::read_cast<vsg::Node>(tile->content->uri, options))
        {
            local_subgraph = model;
        }
    }

    vsg::ref_ptr<vsg::MatrixTransform> vsg_transform;
    if (!tile->transform.values.empty())
    {
        vsg_transform = vsg::MatrixTransform::create(createMatrix(tile->transform.values));
    }

    vsg::info("createTile() preLoadLevel = ", preLoadLevel, ", level = ", level);

    bool usePagedLOD = level > preLoadLevel;

    if (tile->children.values.empty())
    {
        if (local_subgraph)
        {
            if (vsg_transform)
            {
                vsg_transform->addChild(local_subgraph);
                return vsg_transform;
            }
            else
            {
                return local_subgraph;
            }
        }
        return {};
    }
    else if (usePagedLOD)
    {
        double minimumScreenHeightRatio = 0.5;
        if (tile->geometricError > 0.0)
        {
            minimumScreenHeightRatio = (bound.radius / tile->geometricError) * pixelErrorToScreenHeightRatio;
        }

        auto plod = vsg::PagedLOD::create();
        plod->bound = bound;
        plod->children[0] = vsg::PagedLOD::Child{minimumScreenHeightRatio, {}};
        plod->children[1] = vsg::PagedLOD::Child{0.0, local_subgraph};

        plod->filename = "children.tiles";

        auto load_options = vsg::clone(options);
        load_options->setObject("tile", tile);
        plod->options = load_options;

        return plod;
    }
    else // use LOD
    {
        auto group = vsg::Group::create();
        for(auto child : tile->children.values)
        {
            if (auto vsg_child = createTile(child, level+1))
            {
                group->addChild(vsg_child);
            }
        }

        vsg::ref_ptr<vsg::Node> highres_subgraph = group;
        if (group->children.size() == 1) highres_subgraph = group->children[0];

        double minimumScreenHeightRatio = 0.5;
        if (tile->geometricError > 0.0)
        {
            minimumScreenHeightRatio = (bound.radius / tile->geometricError) * pixelErrorToScreenHeightRatio;
        }

        // vsg::info("tile->geometricError = ", tile->geometricError, ", minimumScreenHeightRatio = ", minimumScreenHeightRatio);

        auto lod = vsg::LOD::create();
        lod->bound = bound;
        lod->addChild(vsg::LOD::Child{minimumScreenHeightRatio, highres_subgraph});
        if (local_subgraph) lod->addChild(vsg::LOD::Child{0.0, local_subgraph});

        if (vsg_transform)
        {
            vsg_transform->addChild(lod);
            return vsg_transform;
        }
        else
        {
            return lod;
        }
    }
    return {};
}

vsg::ref_ptr<vsg::Object> Tiles3D::SceneGraphBuilder::createSceneGraph(vsg::ref_ptr<Tiles3D::Tileset> tileset, vsg::ref_ptr<const vsg::Options> in_options)
{
    if (!tileset) return {};

    if (in_options) options = in_options;

    if (options) sharedObjects = options->sharedObjects;
    if (!sharedObjects) sharedObjects = vsg::SharedObjects::create();

    if (!shaderSet)
    {
        shaderSet = vsg::createPhysicsBasedRenderingShaderSet(options);
        if (sharedObjects) sharedObjects->share(shaderSet);
    }

    auto vsg_tileset = vsg::Group::create();

    // vsg_tileset->setObject("tileset", tileset);

    if (tileset->root)
    {
        if (auto vsg_root = createTile(tileset->root, 0))
        {
            vsg_tileset->addChild(vsg_root);
        }
    }

    return vsg_tileset;
}

