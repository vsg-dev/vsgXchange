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

#include <vsg/app/EllipsoidModel.h>
#include <vsg/io/read.h>
#include <vsg/maths/transform.h>
#include <vsg/nodes/CullNode.h>
#include <vsg/nodes/DepthSorted.h>
#include <vsg/nodes/Group.h>
#include <vsg/nodes/LOD.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/nodes/PagedLOD.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/Switch.h>
#include <vsg/nodes/VertexIndexDraw.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/state/material.h>
#include <vsg/vk/ResourceRequirements.h>
#include <vsg/threading/OperationThreads.h>
#include <vsg/utils/ComputeBounds.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>

using namespace vsgXchange;

Tiles3D::SceneGraphBuilder::SceneGraphBuilder()
{
}

vsg::dmat4 Tiles3D::SceneGraphBuilder::createMatrix(const std::vector<double>& m)
{
    if (m.size() == 16)
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
        if (boundingVolume->box.values.size() == 12)
        {
            const auto& v = boundingVolume->box.values;
            vsg::dvec3 axis_x(v[3], v[4], v[5]);
            vsg::dvec3 axis_y(v[6], v[7], v[8]);
            vsg::dvec3 axis_z(v[9], v[10], v[11]);
            return vsg::dsphere(v[0], v[1], v[2], vsg::length(axis_x + axis_y + axis_z));
        }
        else if (boundingVolume->region.values.size() == 6)
        {
            const auto& v = boundingVolume->region.values;
            double west = v[0], south = v[1], east = v[2], north = v[3], low = v[4], high = v[5];
            auto centerECEF = ellipsoidModel->convertLatLongAltitudeToECEF(vsg::dvec3(vsg::degrees(south + north) * 0.5, vsg::degrees(west + east) * 0.5, (high + low) * 0.5));
            auto southWestLowECEF = ellipsoidModel->convertLatLongAltitudeToECEF(vsg::dvec3(vsg::degrees(south), vsg::degrees(west), low));
            auto northEastLowECEF = ellipsoidModel->convertLatLongAltitudeToECEF(vsg::dvec3(vsg::degrees(north), vsg::degrees(east), low));

            // TODO: do we need to track the accumulated transform?
            return vsg::dsphere(centerECEF, std::max(vsg::length(southWestLowECEF - centerECEF), vsg::length(northEastLowECEF - centerECEF)));
        }
        else if (boundingVolume->sphere.values.size() == 4)
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

vsg::ref_ptr<vsg::Node> Tiles3D::SceneGraphBuilder::readTileChildren(vsg::ref_ptr<Tiles3D::Tile> tile, uint32_t level, const std::string& inherited_refine)
{
    // vsg::info("readTileChildren(", tile, ", ", level, ") ", tile->children.values.size(), ", ", operationThreads);

    auto group = vsg::Group::create();

    const std::string refine = tile->refine.empty() ? inherited_refine : tile->refine;

    if (refine == "ADD")
    {
        if (auto local_subgraph = tile->getRefObject<vsg::Node>("local_subgraph"))
        {
            group->addChild(local_subgraph);
        }
    }

    if (operationThreads && tile->children.values.size() > 1)
    {
        struct ReadTileOperation : public vsg::Inherit<vsg::Operation, ReadTileOperation>
        {
            SceneGraphBuilder* builder;
            vsg::ref_ptr<Tiles3D::Tile> tileToCreate;
            vsg::ref_ptr<vsg::Node>& subgraph;
            uint32_t level;
            std::string rto_inherited_refine;
            vsg::ref_ptr<vsg::Latch> latch;

            ReadTileOperation(SceneGraphBuilder* in_builder, vsg::ref_ptr<Tiles3D::Tile> in_tile, vsg::ref_ptr<vsg::Node>& in_subgraph, uint32_t in_level, const std::string& in_inherited_refine, vsg::ref_ptr<vsg::Latch> in_latch) :
                builder(in_builder),
                tileToCreate(in_tile),
                subgraph(in_subgraph),
                level(in_level),
                rto_inherited_refine(in_inherited_refine),
                latch(in_latch) {}

            void run() override
            {
                subgraph = builder->createTile(tileToCreate, level, rto_inherited_refine);
                // vsg::info("Tiles3D::SceneGraphBuilder::readTileChildren() createTile() ", subgraph);
                if (latch) latch->count_down();
            }
        };

        auto latch = vsg::Latch::create(static_cast<int>(tile->children.values.size()));

        std::vector<vsg::ref_ptr<vsg::Node>> children(tile->children.values.size());
        auto itr = children.begin();
        for (auto child : tile->children.values)
        {
            operationThreads->add(ReadTileOperation::create(this, child, *itr++, level + 1, refine, latch), vsg::INSERT_FRONT);
        }

        // use this thread to read the files as well
        operationThreads->run();

        // wait till all the read operations have completed
        latch->wait();

        for (auto& child : children)
        {
            if (child)
            {
                group->addChild(child);
            }
            else
                vsg::info("Failed to read child");
        }
    }
    else
    {
        for (auto child : tile->children.values)
        {
            if (auto vsg_child = createTile(child, level + 1, refine))
            {
                group->addChild(vsg_child);
            }
        }
    }

    vsg::ref_ptr<vsg::Node> root;
    if (group->children.size() == 1) root = group->children[0];
    else root = group;

    assignResourceHints(root);

    return root;
}

vsg::ref_ptr<vsg::Node> Tiles3D::SceneGraphBuilder::createTile(vsg::ref_ptr<Tiles3D::Tile> tile, uint32_t level, const std::string& inherited_refine)
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

    bool usePagedLOD = level > preLoadLevel;

    const std::string refine = tile->refine.empty() ? inherited_refine : tile->refine;

    if (refine == "ADD" && local_subgraph)
    {
        // need to pass on Tile local_subgraph to the SceneGraphBuilder::readTileChildren(..) so assign it to Tile as meta data
        tile->setObject("local_subgraph", local_subgraph);
    }

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
        load_options->setObject("builder", vsg::ref_ptr<SceneGraphBuilder>(this));
        load_options->setValue("level", level);
        load_options->setValue("refine", refine);
        plod->options = load_options;

        return plod;
    }
    else // use LOD
    {
        auto highres_subgraph = readTileChildren(tile, level, refine);

        double minimumScreenHeightRatio = 0.5;
        if (tile->geometricError > 0.0)
        {
            minimumScreenHeightRatio = (bound.radius / tile->geometricError) * pixelErrorToScreenHeightRatio;
        }

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

    if (in_options)
        options = vsg::clone(in_options);
    else
        options = vsg::Options::create();

    if (options)
    {
        sharedObjects = options->sharedObjects;
        operationThreads = options->operationThreads;
        if (operationThreads) options->operationThreads.reset();
    }

    if (!sharedObjects)
    {
        options->sharedObjects = sharedObjects = vsg::SharedObjects::create();
    }

    if (!shaderSet)
    {
        shaderSet = vsg::createPhysicsBasedRenderingShaderSet(options);
        sharedObjects->share(shaderSet);
    }

    vsg::ref_ptr<vsg::Group> vsg_tileset;

    bool inheritState = options->inheritedState.empty();
    if (inheritState)
    {
        auto stateGroup = vsg::StateGroup::create();
        auto layout = shaderSet->createPipelineLayout({}, {0, 1});

        uint32_t vds_set = 0;
        stateGroup->add(vsg::BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, vds_set));

        options->inheritedState = stateGroup->stateCommands;

        vsg_tileset = stateGroup;
    }
    else
    {
        vsg_tileset = vsg::Group::create();
    }

    // vsg_tileset->setObject("tileset", tileset);

    if (tileset->root)
    {
        if (auto vsg_root = createTile(tileset->root, 0, tileset->root->refine))
        {
            vsg_tileset->addChild(vsg_root);
        }
    }

    vsg_tileset->setObject("EllipsoidModel", vsg::EllipsoidModel::create());

    assignResourceHints(vsg_tileset);

    return vsg_tileset;
}

void Tiles3D::SceneGraphBuilder::assignResourceHints(vsg::ref_ptr<vsg::Node> node)
{
    vsg::CollectResourceRequirements collectRequirements;
    node->accept(collectRequirements);

    auto resourceHints = collectRequirements.createResourceHints();
    node->setObject("ResourceHints", resourceHints);
}
