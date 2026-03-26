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
#include <vsg/threading/OperationThreads.h>
#include <vsg/utils/ComputeBounds.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsg/vk/ResourceRequirements.h>

using namespace vsgXchange;

Tiles3D::SceneGraphBuilder::SceneGraphBuilder()
{
}

vsg::dmat4 Tiles3D::SceneGraphBuilder::createMatrix(const std::vector<double>& m) const
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

vsg::dsphere Tiles3D::SceneGraphBuilder::createBound(vsg::ref_ptr<BoundingVolume> boundingVolume) const
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

    if (group->children.empty()) return {};
    else if (group->children.size() == 1) return group->children[0];
    else return group;
}

double Tiles3D::SceneGraphBuilder::computeScreenHeightRatio(const vsg::dsphere& bound, double geometricError) const
{
    if (geometricError == 0.0) return 0.0;
    if (geometricError >= std::numeric_limits<double>::max()) return 0.001;

    return (bound.radius / geometricError) * pixelErrorToScreenHeightRatio;
}

double Tiles3D::SceneGraphBuilder::computeScreenHeightRatio(const Tiles3D::Tile& tile) const
{
    return computeScreenHeightRatio(createBound(tile.boundingVolume), tile.geometricError);
}

bool Tiles3D::SceneGraphBuilder::isTripleNestedTile(vsg::ref_ptr<Tiles3D::Tile> tile) const
{
    if (tile->children.values.size()==1)
    {
        auto& child = tile->children.values[0];
        if (child->children.values.size()==1)
        {
            auto& child_child = child->children.values[0];
            return child_child->children.values.empty();
        }
    }
    return false;
}

vsg::ref_ptr<vsg::Node> Tiles3D::SceneGraphBuilder::createTripleNestedTile(vsg::ref_ptr<Tiles3D::Tile> tile, uint32_t level)
{
    auto& child = tile->children.values[0];
    auto& child_child = child->children.values[0];

    double tile_screenRatio = computeScreenHeightRatio(*tile);
    double child_screenRatio = computeScreenHeightRatio(*child);
    vsg::dsphere bound = createBound(tile->boundingVolume);
    bool usePagedLOD = level > preLoadLevel;

    vsg::ref_ptr<vsg::Node> node;

    if (usePagedLOD)
    {
        auto low_res_subgraph = vsg::read_cast<vsg::Node>(child->content->uri, options);

        auto plod = vsg::PagedLOD::create();
        plod->bound = bound;
        plod->children[0] = vsg::PagedLOD::Child{child_screenRatio, {}};
        plod->children[1] = vsg::PagedLOD::Child{tile_screenRatio, low_res_subgraph};
        plod->filename = child_child->content->uri;
        plod->options = options;

        node = plod;
    }
    else
    {
        auto low_res_subgraph = vsg::read_cast<vsg::Node>(child->content->uri, options);
        auto high_res_subgraph = vsg::read_cast<vsg::Node>(child_child->content->uri, options);

        auto lod = vsg::LOD::create();
        lod->bound = bound;
        lod->addChild(vsg::LOD::Child{child_screenRatio, high_res_subgraph});
        lod->addChild(vsg::LOD::Child{tile_screenRatio, low_res_subgraph});

        node = lod;
    }

    if (!node) return {};

    if (!tile->transform.values.empty())
    {
        auto transform = vsg::MatrixTransform::create(createMatrix(tile->transform.values));
        transform->addChild(node);
        return transform;
    }
    else
    {
        return node;
    }
}

vsg::ref_ptr<vsg::Node> Tiles3D::SceneGraphBuilder::createTile(vsg::ref_ptr<Tiles3D::Tile> tile, uint32_t level, const std::string& inherited_refine)
{
    if (isTripleNestedTile(tile))
    {
        return createTripleNestedTile(tile, level);
    }

    const std::string refine = tile->refine.empty() ? inherited_refine : tile->refine;
    bool usePagedLOD = level > preLoadLevel;
    bool addRefinement = refine == "ADD";

    vsg::ref_ptr<vsg::Node> local_subgraph;
    if (tile->content && !tile->content->uri.empty())
    {
        local_subgraph = vsg::read_cast<vsg::Node>(tile->content->uri, options);
    }

    double tile_screenRatio = 0.001;
    double child_screenRatio = 0.125;
    double add_screenRatio = 0.001;

    vsg::ref_ptr<vsg::Node> node;
    if (tile->children.values.empty()) // leaf Tile
    {
        node = local_subgraph;
    }
    else if (usePagedLOD)  // Use PageLOD to load Tile children
    {
        auto load_options = vsg::clone(options);
        load_options->setObject("tile", tile);
        load_options->setObject("builder", vsg::ref_ptr<SceneGraphBuilder>(this));
        load_options->setValue("level", level);
        load_options->setValue("refine", refine);

        if (addRefinement && local_subgraph)
        {
            auto plod = vsg::PagedLOD::create();
            plod->filename = "children.tiles";
            plod->bound = createBound(tile->boundingVolume);
            plod->children[0] = vsg::PagedLOD::Child{add_screenRatio, {}};
            plod->options = load_options;

            auto group = vsg::Group::create();
            group->addChild(local_subgraph);
            group->addChild(plod);
            node = group;
        }
        else
        {
            child_screenRatio = computeScreenHeightRatio(*tile);

            auto plod = vsg::PagedLOD::create();
            plod->bound = createBound(tile->boundingVolume);
            plod->children[0] = vsg::PagedLOD::Child{child_screenRatio, {}};
            plod->children[1] = vsg::PagedLOD::Child{tile_screenRatio, local_subgraph};
            plod->options = load_options;

            plod->filename = "children.tiles";
            node = plod;
        }
    }
    else // use LOD to directly reference children
    {
        if (addRefinement)
        {
            size_t childCount = (local_subgraph ? 1 : 0) + tile->children.values.size();

            if (childCount > 1)
            {
                auto group = vsg::Group::create();
                if (local_subgraph) group->addChild(local_subgraph);

                for(auto& child : tile->children.values)
                {
                    if (auto child_node = createTile(child, level+1, refine))
                    {
                        auto lod = vsg::LOD::create();
                        lod->bound = createBound(child->boundingVolume);
                        lod->addChild(vsg::LOD::Child{add_screenRatio, child_node});

                        group->addChild(lod);
                    }
                }

                node = group;
            }
            else if (!tile->children.values.empty())  // local_subgraph must be null
            {
                if (auto highres_subgraph = readTileChildren(tile, level, refine))
                {
                    auto lod = vsg::LOD::create();
                    lod->bound = createBound(tile->boundingVolume);
                    lod->addChild(vsg::LOD::Child{child_screenRatio, highres_subgraph});

                    // return if nothing assigned to LOD.
                    if (lod->children.empty()) return {};

                    node = lod;
                }
            }
            else
            {
                node = local_subgraph;
            }
        }
        else
        {
            child_screenRatio = computeScreenHeightRatio(*tile);

            auto highres_subgraph = readTileChildren(tile, level, refine);

            auto lod = vsg::LOD::create();
            lod->bound = createBound(tile->boundingVolume);
            if (highres_subgraph) lod->addChild(vsg::LOD::Child{child_screenRatio, highres_subgraph});
            if (local_subgraph) lod->addChild(vsg::LOD::Child{tile_screenRatio, local_subgraph});

            // return if nothing assigned to LOD.
            if (lod->children.empty()) return {};

            node = lod;
        }

    }

    if (!node) return {};

    if (!tile->transform.values.empty())
    {
        auto transform = vsg::MatrixTransform::create(createMatrix(tile->transform.values));
        transform->addChild(node);
        return transform;
    }
    else
    {
        return node;
    }
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
