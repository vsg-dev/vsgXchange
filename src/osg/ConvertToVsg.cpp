/* <editor-fold desc="MIT License">

Copyright(c) 2019 Thomas Hogarth and Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/all.h>

#include <osg/ArgumentParser>
#include <osg/PagedLOD>
#include <osgDB/ReadFile>
#include <osgUtil/MeshOptimizers>
#include <osgUtil/Optimizer>

#include "ConvertToVsg.h"

using namespace osg2vsg;

vsg::ref_ptr<vsg::BindGraphicsPipeline> ConvertToVsg::getOrCreateBindGraphicsPipeline(uint32_t shaderModeMask, uint32_t geometryMask)
{
    return buildOptions->pipelineCache->getOrCreateBindGraphicsPipeline(shaderModeMask, geometryMask, buildOptions->vertexShaderPath, buildOptions->fragmentShaderPath);
}

vsg::ref_ptr<vsg::BindDescriptorSet> ConvertToVsg::getOrCreateBindDescriptorSet(uint32_t shaderModeMask, uint32_t geometryMask, osg::StateSet* stateset)
{
    MasksAndState masksAndState(shaderModeMask, geometryMask, stateset);
    if (auto itr = bindDescriptorSetMap.find(masksAndState); itr != bindDescriptorSetMap.end())
    {
        // std::cout<<"reusing bindDescriptorSet "<<itr->second.get()<<std::endl;
        return itr->second;
    }

    auto bindGraphicsPipeline = getOrCreateBindGraphicsPipeline(shaderModeMask, geometryMask);
    if (!bindGraphicsPipeline) return {};

    auto pipeline = bindGraphicsPipeline->pipeline;
    if (!pipeline) return {};

    auto pipelineLayout = pipeline->layout;
    if (!pipelineLayout) return {};

    auto descriptorSet = createVsgStateSet(pipelineLayout->setLayouts.front(), stateset, shaderModeMask);
    if (!descriptorSet) return {};

    // std::cout<<"   We have descriptorSet "<<descriptorSet<<std::endl;

    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, descriptorSet);

    bindDescriptorSetMap[masksAndState] = bindDescriptorSet;

    return bindDescriptorSet;
}

vsg::Path ConvertToVsg::mapFileName(const std::string& filename)
{
    if (auto itr = filenameMap.find(filename); itr != filenameMap.end())
    {
        return itr->second;
    }

    vsg::Path vsg_filename = vsg::removeExtension(filename) + "." + buildOptions->extension;

    filenameMap[filename] = vsg_filename;

    return vsg_filename;
}

void ConvertToVsg::optimize(osg::Node* osg_scene)
{
#if 0
    osgUtil::IndexMeshVisitor imv;
#if OSG_MIN_VERSION_REQUIRED(3, 6, 4)
    imv.setGenerateNewIndicesOnAllGeometries(true);
#endif
    osg_scene->accept(imv);
    imv.makeMesh();

    osgUtil::VertexCacheVisitor vcv;
    osg_scene->accept(vcv);
    vcv.optimizeVertices();

    osgUtil::VertexAccessOrderVisitor vaov;
    osg_scene->accept(vaov);
    vaov.optimizeOrder();

    osgUtil::Optimizer optimizer;
    optimizer.optimize(osg_scene, osgUtil::Optimizer::DEFAULT_OPTIMIZATIONS & ~osgUtil::Optimizer::FLATTEN_STATIC_TRANSFORMS);
#endif

    osg2vsg::OptimizeOsgBillboards optimizeBillboards;
    osg_scene->accept(optimizeBillboards);
    optimizeBillboards.optimize();
}

vsg::ref_ptr<vsg::Node> ConvertToVsg::convert(osg::Node* node)
{
    root = nullptr;

    if (auto itr = nodeMap.find(node); itr != nodeMap.end())
    {
        root = itr->second;
    }
    else
    {
        if (node) node->accept(*this);

        nodeMap[node] = root;

        if (root && buildOptions->copyNames && !node->getName().empty())
        {
            root->setValue("Name", node->getName());
        }
    }

    return root;
}

vsg::ref_ptr<vsg::Data> ConvertToVsg::copy(osg::Array* src_array)
{
    if (!src_array) return {};

    switch (src_array->getType())
    {
    case (osg::Array::ByteArrayType): return {};
    case (osg::Array::ShortArrayType): return {};
    case (osg::Array::IntArrayType): return {};

    case (osg::Array::UByteArrayType): return copyArray<vsg::ubyteArray>(src_array);
    case (osg::Array::UShortArrayType): return copyArray<vsg::ushortArray>(src_array);
    case (osg::Array::UIntArrayType): return copyArray<vsg::uintArray>(src_array);

    case (osg::Array::FloatArrayType): return copyArray<vsg::floatArray>(src_array);
    case (osg::Array::DoubleArrayType): return copyArray<vsg::doubleArray>(src_array);

    case (osg::Array::Vec2bArrayType): return {};
    case (osg::Array::Vec3bArrayType): return {};
    case (osg::Array::Vec4bArrayType): return {};

    case (osg::Array::Vec2sArrayType): return {};
    case (osg::Array::Vec3sArrayType): return {};
    case (osg::Array::Vec4sArrayType): return {};

    case (osg::Array::Vec2iArrayType): return {};
    case (osg::Array::Vec3iArrayType): return {};
    case (osg::Array::Vec4iArrayType): return {};

    case (osg::Array::Vec2ubArrayType): return copyArray<vsg::ubvec2Array>(src_array);
    case (osg::Array::Vec3ubArrayType): return copyArray<vsg::ubvec3Array>(src_array);
    case (osg::Array::Vec4ubArrayType): return copyArray<vsg::ubvec4Array>(src_array);

    case (osg::Array::Vec2usArrayType): return copyArray<vsg::usvec2Array>(src_array);
    case (osg::Array::Vec3usArrayType): return copyArray<vsg::usvec3Array>(src_array);
    case (osg::Array::Vec4usArrayType): return copyArray<vsg::usvec4Array>(src_array);

    case (osg::Array::Vec2uiArrayType): return copyArray<vsg::uivec2Array>(src_array);
    case (osg::Array::Vec3uiArrayType): return copyArray<vsg::usvec3Array>(src_array);
    case (osg::Array::Vec4uiArrayType): return copyArray<vsg::usvec4Array>(src_array);

    case (osg::Array::Vec2ArrayType): return copyArray<vsg::vec2Array>(src_array);
    case (osg::Array::Vec3ArrayType): return copyArray<vsg::vec3Array>(src_array);
    case (osg::Array::Vec4ArrayType): return copyArray<vsg::vec4Array>(src_array);

    case (osg::Array::Vec2dArrayType): return copyArray<vsg::dvec2Array>(src_array);
    case (osg::Array::Vec3dArrayType): return copyArray<vsg::dvec2Array>(src_array);
    case (osg::Array::Vec4dArrayType): return copyArray<vsg::dvec2Array>(src_array);

    case (osg::Array::MatrixArrayType): return copyArray<vsg::mat4Array>(src_array);
    case (osg::Array::MatrixdArrayType): return copyArray<vsg::dmat4Array>(src_array);

#if OSG_MIN_VERSION_REQUIRED(3, 5, 7)
    case (osg::Array::QuatArrayType): return {};

    case (osg::Array::UInt64ArrayType): return {};
    case (osg::Array::Int64ArrayType): return {};
#endif
    default: return {};
    }
}

uint32_t ConvertToVsg::calculateShaderModeMask()
{
    if (statestack.empty()) return osg2vsg::ShaderModeMask::NONE;

    auto& statepair = getStatePair();

    return osg2vsg::calculateShaderModeMask(statepair.first) | osg2vsg::calculateShaderModeMask(statepair.second);
}

void ConvertToVsg::apply(osg::Geometry& geometry)
{
    ScopedPushPop spp(*this, geometry.getStateSet());

    uint32_t geometryMask = (osg2vsg::calculateAttributesMask(&geometry) | buildOptions->overrideGeomAttributes) & buildOptions->supportedGeometryAttributes;
    uint32_t shaderModeMask = (calculateShaderModeMask() | buildOptions->overrideShaderModeMask | nodeShaderModeMasks) & buildOptions->supportedShaderModeMask;
    bool requiredBlending = (shaderModeMask & BLEND) != 0;

    // std::cout<<"Have geometry with "<<statestack.size()<<" shaderModeMask="<<shaderModeMask<<", geometryMask="<<geometryMask<<std::endl;

    auto vsg_geometry = osg2vsg::convertToVsg(&geometry, geometryMask, buildOptions->geometryTarget);
    if (!vsg_geometry)
    {
        return;
    }

    auto stategroup = vsg::StateGroup::create();

    auto bindGraphicsPipeline = getOrCreateBindGraphicsPipeline(shaderModeMask, geometryMask);
    if (bindGraphicsPipeline)
    {
        if (!inheritedStateGroup || !inheritedStateGroup->contains(bindGraphicsPipeline))
        {
            stategroup->add(bindGraphicsPipeline);
        }
    }


    if (!statestack.empty())
    {
        auto stateset = getStatePair().second;
        //std::cout<<"   We have stateset "<<stateset<<", descriptorSetLayouts.size() = "<<descriptorSetLayouts.size()<<", "<<shaderModeMask<<std::endl;
        if (stateset)
        {
            auto bindDescriptorSet = getOrCreateBindDescriptorSet(shaderModeMask, geometryMask, stateset);
            if (bindDescriptorSet)
            {
                if (!inheritedStateGroup || !inheritedStateGroup->contains(bindDescriptorSet))
                {
                    stategroup->add(bindDescriptorSet);
                }
            }
        }
    }

    stategroup->addChild(vsg_geometry);

    if (requiredBlending &&  buildOptions->useDepthSorted)
    {
        auto center = geometry.getBound().center();
        auto radius = geometry.getBound().radius();

        auto depthSorted = vsg::DepthSorted::create();
        depthSorted->binNumber = 10;
        depthSorted->bound.set(center.x(), center.y(), center.z(), radius);
        depthSorted->child = stategroup;

        root = depthSorted;
    }
    else
    {
        if (buildOptions->insertCullGroups || buildOptions->insertCullNodes)
        {
            auto center = geometry.getBound().center();
            auto radius = geometry.getBound().radius();

            root = vsg::CullNode::create(vsg::dsphere(center.x(), center.y(), center.z(), radius), stategroup);
        }
        else
        {
            root = stategroup;
        }
    }
}

void ConvertToVsg::apply(osg::Group& group)
{
    if (osgTerrain::TerrainTile* tile = dynamic_cast<osgTerrain::TerrainTile*>(&group); tile)
    {
        apply(*tile);
        return;
    }

    ScopedPushPop spp(*this, group.getStateSet());

    auto vsg_group = vsg::Group::create();

    //vsg_group->setValue("class", group.className());

    for (unsigned int i = 0; i < group.getNumChildren(); ++i)
    {
        auto child = group.getChild(i);
        if (auto vsg_child = convert(child); vsg_child)
        {
            vsg_group->addChild(vsg_child);
        }
    }

    root = vsg_group;
}

void ConvertToVsg::apply(osg::MatrixTransform& transform)
{
    ScopedPushPop spp(*this, transform.getStateSet());

    auto vsg_transform = vsg::MatrixTransform::create();
    vsg_transform->matrix = osg2vsg::convert(transform.getMatrix());

    for (unsigned int i = 0; i < transform.getNumChildren(); ++i)
    {
        auto child = transform.getChild(i);
        if (auto vsg_child = convert(child); vsg_child)
        {
            vsg_transform->addChild(vsg_child);
        }
    }

    struct CheckForCullNodes : public vsg::ConstVisitor
    {
        bool containsCullNodes = false;

        void apply(const vsg::Node& node) override
        {
            node.traverse(*this);
        }

        void apply(const vsg::CullGroup&) override
        {
            containsCullNodes = true;
        }

        void apply(const vsg::CullNode&) override
        {
            containsCullNodes = true;
        }

        void apply(const vsg::LOD&) override
        {
            containsCullNodes = true;
        }

        void apply(const vsg::PagedLOD&) override
        {
            containsCullNodes = true;
        }

        void apply(const vsg::DepthSorted&) override
        {
            containsCullNodes = true;
        }
    } checkForCullNodes;

    // search the subgraph to see if any cull ndoes are present so we know whether transform the view frustum in local coords will be reuiqred.
    vsg_transform->accept(checkForCullNodes);

    // need to run visitor to seeif it contains any CullNode/LOD/PagedLOD
    vsg_transform->subgraphRequiresLocalFrustum = checkForCullNodes.containsCullNodes;

    root = vsg_transform;
}

void ConvertToVsg::apply(osg::CoordinateSystemNode& cs)
{
    apply((osg::Group&)cs);

    if (root)
    {
        auto em = cs.getEllipsoidModel();
        if (em)
        {
            root->setObject("EllipsoidModel", vsg::EllipsoidModel::create(em->getRadiusEquator(), em->getRadiusPolar()));
        }
    }
}

void ConvertToVsg::apply(osg::Billboard& billboard)
{
    ScopedPushPop spp(*this, billboard.getStateSet());

    if (buildOptions->billboardTransform)
    {
        nodeShaderModeMasks = BILLBOARD;
    }
    else
    {
        nodeShaderModeMasks = BILLBOARD | SHADER_TRANSLATE;
    }

    auto vsg_group = vsg::Group::create();

    if (nodeShaderModeMasks & SHADER_TRANSLATE)
    {
        using Positions = std::vector<osg::Vec3>;
        using ChildPositions = std::map<osg::Drawable*, Positions>;
        ChildPositions childPositions;

        for (unsigned int i = 0; i < billboard.getNumDrawables(); ++i)
        {
            childPositions[billboard.getDrawable(i)].push_back(billboard.getPosition(i));
        }

        struct ComputeBillboardBoundingBox : public osg::Drawable::ComputeBoundingBoxCallback
        {
            Positions positions;

            ComputeBillboardBoundingBox(const Positions& in_positions) :
                positions(in_positions) {}

            virtual osg::BoundingBox computeBound(const osg::Drawable& drawable) const
            {
                const osg::Geometry* geom = drawable.asGeometry();
                const osg::Vec3Array* vertices = geom ? dynamic_cast<const osg::Vec3Array*>(geom->getVertexArray()) : nullptr;
                if (vertices)
                {
                    osg::BoundingBox local_bb;
                    for (auto vertex : *vertices)
                    {
                        local_bb.expandBy(vertex);
                    }

                    osg::BoundingBox world_bb;
                    for (auto position : positions)
                    {
                        world_bb.expandBy(local_bb._min + position);
                        world_bb.expandBy(local_bb._max + position);
                    }

                    return world_bb;
                }

                return osg::BoundingBox();
            }
        };

        for (auto& [child, positions] : childPositions)
        {
            osg::Geometry* geometry = child->asGeometry();
            if (geometry)
            {
                geometry->setComputeBoundingBoxCallback(new ComputeBillboardBoundingBox(positions));

                osg::ref_ptr<osg::Vec3Array> positionArray = new osg::Vec3Array(positions.begin(), positions.end());
                positionArray->setBinding(osg::Array::BIND_OVERALL);
                geometry->setVertexAttribArray(7, positionArray);

                root = nullptr;

                geometry->accept(*this);

                if (root) vsg_group->addChild(root);
            }
        }
    }
    else
    {
        for (unsigned int i = 0; i < billboard.getNumDrawables(); ++i)
        {
            auto translate = osg::Matrixd::translate(billboard.getPosition(i));

            auto vsg_transform = vsg::MatrixTransform::create();
            vsg_transform->matrix = osg2vsg::convert(translate);

            auto vsg_child = convert(billboard.getDrawable(i));

            if (vsg_child)
            {
                vsg_transform->addChild(vsg_child);
                vsg_group->addChild(vsg_transform);
            }
        }
    }

    if (vsg_group->children.size() == 9)
        root = nullptr;
    else if (vsg_group->children.size() == 1)
        root = vsg_group->children[0];
    else
        root = vsg_group;

    nodeShaderModeMasks = NONE;
}

void ConvertToVsg::apply(osg::LOD& lod)
{
    auto vsg_lod = vsg::LOD::create();

    const osg::BoundingSphere& bs = lod.getBound();
    osg::Vec3d center = (lod.getCenterMode() == osg::LOD::USER_DEFINED_CENTER) ? lod.getCenter() : bs.center();
    double radius = (lod.getRadius() > 0.0) ? lod.getRadius() : bs.radius();

    vsg_lod->bound.set(center.x(), center.y(), center.z(), radius);

    unsigned int numChildren = std::min(lod.getNumChildren(), lod.getNumRanges());

    const double pixel_ratio = 1.0 / 1080.0;
    const double angle_ratio = 1.0 / osg::DegreesToRadians(30.0); // assume a 60 fovy for reference

    // build a map of minimum screen ration to child
    std::map<double, vsg::ref_ptr<vsg::Node>> ratioChildMap;
    for (unsigned int i = 0; i < numChildren; ++i)
    {
        if (auto vsg_child = convert(lod.getChild(i)); vsg_child)
        {
            double minimumScreenHeightRatio = (lod.getRangeMode() == osg::LOD::DISTANCE_FROM_EYE_POINT) ? (atan2(radius, static_cast<double>(lod.getMaxRange(i))) * angle_ratio) : (lod.getMinRange(i) * pixel_ratio);

            ratioChildMap[minimumScreenHeightRatio] = vsg_child;
        }
    }

    // add to vsg::LOD in reverse order - highest level of detail first
    for (auto itr = ratioChildMap.rbegin(); itr != ratioChildMap.rend(); ++itr)
    {
        vsg_lod->addChild(vsg::LOD::Child{itr->first, itr->second});
    }

    root = vsg_lod;
}

void ConvertToVsg::apply(osg::PagedLOD& plod)
{
    ++numOfPagedLOD;

    auto vsg_lod = vsg::PagedLOD::create();

    if (!plod.getDatabasePath().empty())
    {
        auto options = (buildOptions && buildOptions->options) ? vsg::Options::create(*(buildOptions->options)) : vsg::Options::create();

        options->paths.push_back(plod.getDatabasePath());

        vsg_lod->options = options;
    }

    const osg::BoundingSphere& bs = plod.getBound();
    osg::Vec3d center = (plod.getCenterMode() == osg::LOD::USER_DEFINED_CENTER) ? plod.getCenter() : bs.center();
    double radius = (plod.getRadius() > 0.0) ? plod.getRadius() : bs.radius();

    vsg_lod->bound.set(center.x(), center.y(), center.z(), radius);

    unsigned int numChildren = plod.getNumChildren();
    unsigned int numRanges = plod.getNumRanges();

    const double pixel_ratio = 1.0 / 1080.0;
    const double angle_ratio = 1.0 / osg::DegreesToRadians(30.0); // assume a 60 fovy for reference

    if (numRanges <= 0) return;

    struct Child
    {
        double minimumScreenHeightRatio;
        std::string filename;
        vsg::ref_ptr<vsg::Node> node;

        bool operator<(const Child& rhs) const
        {
            return minimumScreenHeightRatio > rhs.minimumScreenHeightRatio;
        }
    };

    using Children = std::vector<Child>;
    Children children;

    for (unsigned int i = 0; i < numRanges; ++i)
    {
        vsg::ref_ptr<vsg::Node> vsg_child;
        if (i < numChildren) vsg_child = convert(plod.getChild(i));

        double minimumScreenHeightRatio = (plod.getRangeMode() == osg::LOD::DISTANCE_FROM_EYE_POINT) ? (atan2(radius, static_cast<double>(plod.getMaxRange(i))) * angle_ratio) : (plod.getMinRange(i) * pixel_ratio);

        auto osg_filename = plod.getFileName(i);
        auto vsg_filename = osg_filename; //osg_filename.empty() ? vsg::Path() : mapFileName(osg_filename);

        children.emplace_back(Child{minimumScreenHeightRatio, vsg_filename, vsg_child});
    }

    std::sort(children.begin(), children.end());

    if (children.size() == 2)
    {
        vsg_lod->filename = children[0].filename;
        vsg_lod->children[0] = vsg::PagedLOD::Child{children[0].minimumScreenHeightRatio, children[0].node};
        vsg_lod->children[1] = vsg::PagedLOD::Child{children[1].minimumScreenHeightRatio, children[1].node};
    }

    root = vsg_lod;
}

void ConvertToVsg::apply(osg::Switch& sw)
{
    auto vsg_sw = vsg::Switch::create();

    for(unsigned int i=0; i<sw.getNumChildren(); ++i)
    {
        auto vsg_child = convert(sw.getChild(i));
        if (vsg_child)
        {
            vsg_sw->addChild(sw.getValue(i), vsg_child);
        }
    }

    root = vsg_sw;
}

void ConvertToVsg::apply(osgTerrain::TerrainTile& tile)
{
    root = nullptr;

    tile.traverse(*this);
}
