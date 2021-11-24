/* <editor-fold desc="MIT License">

Copyright(c) 2019 Thomas Hogarth and Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include "SceneBuilder.h"

#include "GeometryUtils.h"
#include "ImageUtils.h"
#include "Optimize.h"
#include "ShaderUtils.h"

#include <vsg/nodes/CullGroup.h>
#include <vsg/nodes/CullNode.h>
#include <vsg/nodes/MatrixTransform.h>

#include <osgUtil/MeshOptimizers>

#include <osg/io_utils>

using namespace osg2vsg;

#if 0
#    define DEBUG_OUTPUT std::cout
#else
#    define DEBUG_OUTPUT \
        if (false) std::cout
#endif

osg::ref_ptr<osg::StateSet> SceneBuilderBase::uniqueState(osg::ref_ptr<osg::StateSet> stateset, bool programStateSet)
{
    if (auto itr = uniqueStateSets.find(stateset); itr != uniqueStateSets.end())
    {
        DEBUG_OUTPUT << "    uniqueState() found state" << std::endl;
        return *itr;
    }

    DEBUG_OUTPUT << "    uniqueState() inserting state" << std::endl;

    if (writeToFileProgramAndDataSetSets && stateset.valid())
    {
        if (programStateSet)
            osgDB::writeObjectFile(*(stateset), vsg::make_string("programState_", stateMap.size(), ".osgt"));
        else
            osgDB::writeObjectFile(*(stateset), vsg::make_string("dataState_", stateMap.size(), ".osgt"));
    }

    uniqueStateSets.insert(stateset);
    return stateset;
}

SceneBuilderBase::StatePair SceneBuilderBase::computeStatePair(osg::StateSet* stateset)
{
    if (!stateset) return StatePair();

    osg::ref_ptr<osg::StateSet> programState = new osg::StateSet;
    osg::ref_ptr<osg::StateSet> dataState = new osg::StateSet;

    programState->setModeList(stateset->getModeList());
    programState->setTextureModeList(stateset->getTextureModeList());
    programState->setDefineList(stateset->getDefineList());

    dataState->setAttributeList(stateset->getAttributeList());
    dataState->setTextureAttributeList(stateset->getTextureAttributeList());
    dataState->setUniformList(stateset->getUniformList());

    for (auto attribute : stateset->getAttributeList())
    {
        osg::Program* program = dynamic_cast<osg::Program*>(attribute.second.first.get());
        if (program)
        {
            DEBUG_OUTPUT << "Found program removing from dataState" << program << " inserting into programState" << std::endl;
            dataState->removeAttribute(program);
            programState->setAttribute(program, attribute.second.second);
        }
    }

    return StatePair(uniqueState(programState, true), uniqueState(dataState, false));
}

SceneBuilderBase::StatePair& SceneBuilderBase::getStatePair()
{
    auto& statepair = stateMap[statestack];

    if (!statestack.empty() && (!statepair.first || !statepair.second))
    {
        osg::ref_ptr<osg::StateSet> combined;
        if (statestack.size() == 1)
        {
            combined = statestack.back();
        }
        else
        {
            combined = new osg::StateSet;
            for (auto& stateset : statestack)
            {
                combined->merge(*stateset);
            }
        }

        statepair = computeStatePair(combined);
    }
    return statepair;
}

vsg::ref_ptr<vsg::DescriptorImage> SceneBuilderBase::convertToVsgTexture(const osg::Texture* osgtexture)
{
    if (auto itr = texturesMap.find(osgtexture); itr != texturesMap.end()) return itr->second;

    const osg::Image* image = osgtexture ? osgtexture->getImage(0) : nullptr;
    auto textureData = convertToVsg(image, buildOptions->mapRGBtoRGBAHint);
    if (!textureData)
    {
        // DEBUG_OUTPUT << "Could not convert osg image data" << std::endl;
        return vsg::ref_ptr<vsg::DescriptorImage>();
    }

    vsg::ref_ptr<vsg::Sampler> sampler = convertToSampler(osgtexture);

    auto texture = vsg::DescriptorImage::create(sampler, textureData, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    texturesMap[osgtexture] = texture;

    return texture;
}

vsg::ref_ptr<vsg::DescriptorSet> SceneBuilderBase::createVsgStateSet(vsg::ref_ptr<vsg::DescriptorSetLayout> descriptorSetLayout, const osg::StateSet* stateset, uint32_t shaderModeMask)
{
    if (!stateset) return vsg::ref_ptr<vsg::DescriptorSet>();

    vsg::Descriptors descriptors;

    auto addTexture = [&](unsigned int i) {
        const osg::StateAttribute* texatt = stateset->getTextureAttribute(i, osg::StateAttribute::TEXTURE);
        const osg::Texture* osgtex = dynamic_cast<const osg::Texture*>(texatt);
        if (osgtex)
        {
            auto vsgtex = convertToVsgTexture(osgtex);
            if (vsgtex)
            {
                // shaders are looking for textures in original units
                vsgtex->dstBinding = i;
                descriptors.push_back(vsgtex);
            }
            else
            {
                std::cout << "createVsgStateSet(..) osg::Texture, with i=" << i << " found but cannot be mapped to vsg::DescriptorImage." << std::endl;
            }
        }
    };

    // add material first
    const osg::Material* osg_material = dynamic_cast<const osg::Material*>(stateset->getAttribute(osg::StateAttribute::Type::MATERIAL));
    if ((shaderModeMask & ShaderModeMask::MATERIAL) && (osg_material != nullptr) /*&& stateset->getMode(GL_COLOR_MATERIAL) == osg::StateAttribute::Values::ON*/)
    {
        auto matdata = convertToMaterialValue(osg_material);
        auto vsg_materialUniform = vsg::DescriptorBuffer::create(matdata, MATERIAL_BINDING); // just use high value for now, should maybe put uniforms into a different descriptor set to simplify binding indexes
        descriptors.push_back(vsg_materialUniform);
    }

    // add textures
    if (shaderModeMask & ShaderModeMask::DIFFUSE_MAP) addTexture(DIFFUSE_TEXTURE_UNIT);
    if (shaderModeMask & ShaderModeMask::OPACITY_MAP) addTexture(OPACITY_TEXTURE_UNIT);
    if (shaderModeMask & ShaderModeMask::AMBIENT_MAP) addTexture(AMBIENT_TEXTURE_UNIT);
    if (shaderModeMask & ShaderModeMask::NORMAL_MAP) addTexture(NORMAL_TEXTURE_UNIT);
    if (shaderModeMask & ShaderModeMask::SPECULAR_MAP) addTexture(SPECULAR_TEXTURE_UNIT);

    if (descriptors.size() == 0) return vsg::ref_ptr<vsg::DescriptorSet>();

    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, descriptors);

    return descriptorSet;
}

///////////////////////////////////////////////////////////////////////////////////////
//
//   SceneBuilder
//
void SceneBuilder::apply(osg::Node& node)
{
    DEBUG_OUTPUT << "Visiting " << node.className() << " " << node.getStateSet() << std::endl;

    if (node.getStateSet()) pushStateSet(*node.getStateSet());

    traverse(node);

    if (node.getStateSet()) popStateSet();
}

void SceneBuilder::apply(osg::Group& group)
{
    DEBUG_OUTPUT << "Group " << group.className() << " " << group.getStateSet() << std::endl;

    if (group.getStateSet()) pushStateSet(*group.getStateSet());

    traverse(group);

    if (group.getStateSet()) popStateSet();
}

void SceneBuilder::apply(osg::Transform& transform)
{
    DEBUG_OUTPUT << "Transform " << transform.className() << " " << transform.getStateSet() << std::endl;

    if (transform.getStateSet()) pushStateSet(*transform.getStateSet());

    osg::Matrix matrix;
    if (!matrixstack.empty()) matrix = matrixstack.back();
    transform.computeLocalToWorldMatrix(matrix, this);

    pushMatrix(matrix);

    traverse(transform);

    popMatrix();

    if (transform.getStateSet()) popStateSet();
}

void SceneBuilder::apply(osg::Billboard& billboard)
{
    DEBUG_OUTPUT << "apply(osg::Billboard& billboard)" << std::endl;

    if (billboard.getStateSet()) pushStateSet(*billboard.getStateSet());

    if (buildOptions->billboardTransform)
    {
        nodeShaderModeMasks = BILLBOARD;
    }
    else
    {
        nodeShaderModeMasks = BILLBOARD | SHADER_TRANSLATE;
    }

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
                geometry->accept(*this);
            }
        }
    }
    else
    {
        for (unsigned int i = 0; i < billboard.getNumDrawables(); ++i)
        {
            auto translate = osg::Matrixd::translate(billboard.getPosition(i));

            if (matrixstack.empty())
                pushMatrix(translate);
            else
                pushMatrix(translate * matrixstack.back());

            billboard.getDrawable(i)->accept(*this);

            popMatrix();
        }
    }

    nodeShaderModeMasks = NONE;

    if (billboard.getStateSet()) popStateSet();
}

void SceneBuilder::apply(osg::Geometry& geometry)
{
    if (!geometry.getVertexArray())
    {
        DEBUG_OUTPUT << "SceneBuilder::apply(osg::Geometry& geometry), ignoring geometry with null geometry.getVertexArray()" << std::endl;
        return;
    }

    if (geometry.getVertexArray()->getNumElements() == 0)
    {
        DEBUG_OUTPUT << "SceneBuilder::apply(osg::Geometry& geometry), ignoring geometry with empty geometry.getVertexArray()" << std::endl;
        return;
    }

    if (geometry.getNumPrimitiveSets() == 0)
    {
        DEBUG_OUTPUT << "SceneBuilder::apply(osg::Geometry& geometry), ignoring geometry with empty PrimitiveSetList." << std::endl;
        return;
    }

    for (auto& primitive : geometry.getPrimitiveSetList())
    {
        if (primitive->getNumPrimitives() == 0)
        {
            DEBUG_OUTPUT << "SceneBuilder::apply(osg::Geometry& geometry), ignoring geometry with as it contains an empty PrimitiveSet : " << primitive->className() << std::endl;
            return;
        }
    }

    if (geometry.getStateSet()) pushStateSet(*geometry.getStateSet());

    StatePair& statePair = getStatePair();

    osg::Matrix matrix;
    if (!matrixstack.empty()) matrix = matrixstack.back();

    // Build programTransformStateMap
    {
        TransformStatePair& transformStatePair = programTransformStateMap[statePair.first];
        StateGeometryMap& stateGeometryMap = transformStatePair.matrixStateGeometryMap[matrix];
        stateGeometryMap[statePair.second].push_back(&geometry);

        TransformGeometryMap& transformGeometryMap = transformStatePair.stateTransformMap[statePair.second];
        transformGeometryMap[matrix].push_back(&geometry);
    }

    // Build new masksTransformStateMap
    {
        Masks masks(calculateShaderModeMask(statePair.first.get()) | calculateShaderModeMask(statePair.second.get()) | nodeShaderModeMasks, calculateAttributesMask(&geometry));

        DEBUG_OUTPUT << "populating masks (" << masks.first << ", " << masks.second << ")" << std::endl;

        TransformStatePair& transformStatePair = masksTransformStateMap[masks];
        StateGeometryMap& stateGeometryMap = transformStatePair.matrixStateGeometryMap[matrix];
        stateGeometryMap[statePair.second].push_back(&geometry);

        TransformGeometryMap& transformGeometryMap = transformStatePair.stateTransformMap[statePair.second];
        transformGeometryMap[matrix].push_back(&geometry);
    }

    DEBUG_OUTPUT << "   Geometry " << geometry.className() << " ss=" << statestack.size() << " ms=" << matrixstack.size() << std::endl;

    if (geometry.getStateSet()) popStateSet();
}

void SceneBuilder::pushStateSet(osg::StateSet& stateset)
{
    statestack.push_back(&stateset);
}

void SceneBuilder::popStateSet()
{
    statestack.pop_back();
}

void SceneBuilder::SceneBuilder::pushMatrix(const osg::Matrix& matrix)
{
    matrixstack.push_back(matrix);
}

void SceneBuilder::popMatrix()
{
    matrixstack.pop_back();
}

void SceneBuilder::print()
{
    DEBUG_OUTPUT << "\nprint()\n";
    DEBUG_OUTPUT << "   programTransformStateMap.size() = " << programTransformStateMap.size() << std::endl;
    for (auto [programStateSet, transformStatePair] : programTransformStateMap)
    {
        DEBUG_OUTPUT << "       programStateSet = " << programStateSet.get() << std::endl;
        DEBUG_OUTPUT << "           transformStatePair.matrixStateGeometryMap.size() = " << transformStatePair.matrixStateGeometryMap.size() << std::endl;
        DEBUG_OUTPUT << "           transformStatePair.stateTransformMap.size() = " << transformStatePair.stateTransformMap.size() << std::endl;
    }
}

osg::ref_ptr<osg::Node> SceneBuilder::createStateGeometryGraphOSG(StateGeometryMap& stateGeometryMap)
{
    DEBUG_OUTPUT << "createStateGeometryGraph()" << stateGeometryMap.size() << std::endl;

    if (stateGeometryMap.empty()) return nullptr;

    osg::ref_ptr<osg::Group> group = new osg::Group;
    for (auto [stateset, geometries] : stateGeometryMap)
    {
        osg::ref_ptr<osg::Group> stateGroup = new osg::Group;
        stateGroup->setStateSet(stateset);
        group->addChild(stateGroup);

        for (auto& geometry : geometries)
        {
            osg::ref_ptr<osg::Geometry> new_geometry = geometry; // new osg::Geometry(*geometry);
            new_geometry->setStateSet(nullptr);
            stateGroup->addChild(new_geometry);
        }
    }

    if (group->getNumChildren() == 1) return group->getChild(0);

    return group;
}

osg::ref_ptr<osg::Node> SceneBuilder::createTransformGeometryGraphOSG(TransformGeometryMap& transformGeometryMap)
{
    DEBUG_OUTPUT << "createStateGeometryGraph()" << transformGeometryMap.size() << std::endl;

    if (transformGeometryMap.empty()) return nullptr;

    osg::ref_ptr<osg::Group> group = new osg::Group;
    for (auto [matrix, geometries] : transformGeometryMap)
    {
        osg::ref_ptr<osg::MatrixTransform> transform = new osg::MatrixTransform;
        transform->setMatrix(matrix);
        group->addChild(transform);

        for (auto& geometry : geometries)
        {
            osg::ref_ptr<osg::Geometry> new_geometry = geometry; // new osg::Geometry(*geometry);
            new_geometry->setStateSet(nullptr);
            transform->addChild(new_geometry);
        }
    }

    if (group->getNumChildren() == 1) return group->getChild(0);

    return group;
}

osg::ref_ptr<osg::Node> SceneBuilder::createOSG()
{
    // clear caches
    geometriesMap.clear();
    texturesMap.clear();

    osg::ref_ptr<osg::Group> group = new osg::Group;

    for (auto [programStateSet, transformStatePair] : programTransformStateMap)
    {
        osg::ref_ptr<osg::Group> programGroup = new osg::Group;
        group->addChild(programGroup);
        programGroup->setStateSet(programStateSet.get());

        bool transformAtTop = transformStatePair.matrixStateGeometryMap.size() < transformStatePair.stateTransformMap.size();
        if (transformAtTop)
        {
            for (auto [matrix, stateGeometryMap] : transformStatePair.matrixStateGeometryMap)
            {
                osg::ref_ptr<osg::Node> stateGeometryGraph = createStateGeometryGraphOSG(stateGeometryMap);
                if (!stateGeometryGraph) continue;

                if (!matrix.isIdentity())
                {
                    osg::ref_ptr<osg::MatrixTransform> transform = new osg::MatrixTransform;
                    transform->setMatrix(matrix);
                    programGroup->addChild(transform);
                    transform->addChild(stateGeometryGraph);
                }
                else
                {
                    programGroup->addChild(stateGeometryGraph);
                }
            }
        }
        else
        {
            for (auto [stateset, transformeGeometryMap] : transformStatePair.stateTransformMap)
            {
                osg::ref_ptr<osg::Node> transformGeometryGraph = createTransformGeometryGraphOSG(transformeGeometryMap);
                if (!transformGeometryGraph) continue;

                transformGeometryGraph->setStateSet(stateset);
                programGroup->addChild(transformGeometryGraph);
            }
        }

        DEBUG_OUTPUT << "       programStateSet = " << programStateSet.get() << std::endl;
        DEBUG_OUTPUT << "           transformStatePair.matrixStateGeometryMap.size() = " << transformStatePair.matrixStateGeometryMap.size() << std::endl;
        DEBUG_OUTPUT << "           transformStatePair.stateTransformMap.size() = " << transformStatePair.stateTransformMap.size() << std::endl;
    }
    return group;
}

vsg::ref_ptr<vsg::Node> SceneBuilder::createTransformGeometryGraphVSG(TransformGeometryMap& transformGeometryMap, vsg::Paths& /*searchPaths*/, uint32_t requiredGeomAttributesMask)
{
    DEBUG_OUTPUT << "createTransformGeometryGraphVSG() " << transformGeometryMap.size() << std::endl;

    if (transformGeometryMap.empty()) return vsg::ref_ptr<vsg::Node>();

    vsg::ref_ptr<vsg::Group> group = vsg::Group::create();
    for (auto [matrix, geometries] : transformGeometryMap)
    {
        vsg::ref_ptr<vsg::Group> localGroup = group;

        bool requiresTransform = !matrix.isIdentity();

#if 1
        bool requiresTopCullGroup = (buildOptions->insertCullGroups || buildOptions->insertCullNodes) && (requiresTransform /* || geometries.size()==1*/);
        bool requiresLeafCullGroup = (buildOptions->insertCullGroups || buildOptions->insertCullNodes) && !requiresTopCullGroup;
        if (requiresTransform)
        {
            // need to insert a transform
            vsg::dmat4 vsgmatrix = vsg::dmat4(matrix(0, 0), matrix(0, 1), matrix(0, 2), matrix(0, 3),
                                              matrix(1, 0), matrix(1, 1), matrix(1, 2), matrix(1, 3),
                                              matrix(2, 0), matrix(2, 1), matrix(2, 2), matrix(2, 3),
                                              matrix(3, 0), matrix(3, 1), matrix(3, 2), matrix(3, 3));

            vsg::ref_ptr<vsg::MatrixTransform> transform = vsg::MatrixTransform::create(vsgmatrix);

            localGroup = transform;

            if (buildOptions->insertCullGroups || buildOptions->insertCullNodes)
            {
                osg::BoundingBoxd overall_bb;
                for (auto& geometry : geometries)
                {
                    osg::BoundingBox bb = geometry->getBoundingBox();
                    for (int i = 0; i < 8; ++i)
                    {
                        overall_bb.expandBy(osg::Vec3d(bb.corner(i)) * matrix);
                    }
                }

                vsg::dvec3 bb_min(overall_bb.xMin(), overall_bb.yMin(), overall_bb.zMin());
                vsg::dvec3 bb_max(overall_bb.xMax(), overall_bb.yMax(), overall_bb.zMax());
                vsg::dsphere boundingSphere((bb_min + bb_max) * 0.5, vsg::length(bb_max - bb_min) * 0.5);

                if (buildOptions->insertCullNodes)
                {
                    group->addChild(vsg::CullNode::create(boundingSphere, transform));
                }
                else
                {
                    auto cullGroup = vsg::CullGroup::create(boundingSphere);
                    cullGroup->addChild(transform);
                    group->addChild(cullGroup);
                }
            }
            else
            {
                group->addChild(transform);
            }
        }
#else
        bool requiresTopCullGroup = (insertCullGroups || insertCullNodes) && (requiresTransform || geometries.size() == 1);
        bool requiresLeafCullGroup = (insertCullGroups || insertCullNodes) && !requiresTopCullGroup;
        if (requiresTopCullGroup)
        {
            osg::BoundingBox overall_bb;
            for (auto& geometry : geometries)
            {
                osg::BoundingBox bb = geometry->getBoundingBox();
                for (int i = 0; i < 8; ++i)
                {
                    overall_bb.expandBy(bb.corner(i) * matrix);
                }
            }

            vsg::vec3 bb_min(overall_bb.xMin(), overall_bb.yMin(), overall_bb.zMin());
            vsg::vec3 bb_max(overall_bb.xMax(), overall_bb.yMax(), overall_bb.zMax());

            vsg::sphere boundingSphere((bb_min + bb_max) * 0.5f, vsg::length(bb_max - bb_min) * 0.5f);
            auto cullGroup = vsg::CullGroup::create(boundingSphere);
            localGroup->addChild(cullGroup);
            localGroup = cullGroup;
        }

        if (requiresTransform)
        {
            // need to insert a transform
            vsg::mat4 vsgmatrix = vsg::mat4(matrix(0, 0), matrix(1, 0), matrix(2, 0), matrix(3, 0),
                                            matrix(0, 1), matrix(1, 1), matrix(2, 1), matrix(3, 1),
                                            matrix(0, 2), matrix(1, 2), matrix(2, 2), matrix(3, 2),
                                            matrix(0, 3), matrix(1, 3), matrix(2, 3), matrix(3, 3));

            vsg::ref_ptr<vsg::MatrixTransform> transform = vsg::MatrixTransform::create(vsgmatrix);

            localGroup->addChild(transform);

            localGroup = transform;
        }
#endif

        for (auto& geometry : geometries)
        {
#if 1
            vsg::ref_ptr<vsg::Command> leaf;
            if (geometriesMap.find(geometry) != geometriesMap.end())
            {
                DEBUG_OUTPUT << "sharing geometry" << std::endl;
                leaf = geometriesMap[geometry];
            }
            else
            {
                leaf = convertToVsg(geometry, requiredGeomAttributesMask, buildOptions->geometryTarget);
                if (leaf)
                {
                    geometriesMap[geometry] = leaf;
                }
            }

            if (!leaf) continue;

            if (requiresLeafCullGroup)
            {
                osg::BoundingBox bb = geometry->getBoundingBox();
                vsg::dvec3 bb_min(bb.xMin(), bb.yMin(), bb.zMin());
                vsg::dvec3 bb_max(bb.xMax(), bb.yMax(), bb.zMax());

                vsg::dsphere boundingSphere((bb_min + bb_max) * 0.5, vsg::length(bb_max - bb_min) * 0.5);
                if (buildOptions->insertCullNodes)
                {
                    DEBUG_OUTPUT << "Using CullNode" << std::endl;
                    localGroup->addChild(vsg::CullNode::create(boundingSphere, leaf));
                }
                else
                {
                    DEBUG_OUTPUT << "Using CullGroupe" << std::endl;
                    auto cullGroup = vsg::CullGroup::create(boundingSphere);
                    cullGroup->addChild(leaf);
                    localGroup->addChild(cullGroup);
                }
            }
            else
            {
                localGroup->addChild(leaf);
            }
#else
            vsg::ref_ptr<vsg::Group> nestedGroup = localGroup;

            if (requiresLeafCullGroup)
            {
                osg::BoundingBox bb = geometry->getBoundingBox();
                vsg::vec3 bb_min(bb.xMin(), bb.yMin(), bb.zMin());
                vsg::vec3 bb_max(bb.xMax(), bb.yMax(), bb.zMax());

                vsg::sphere boundingSphere((bb_min + bb_max) * 0.5f, vsg::length(bb_max - bb_min) * 0.5f);
                if (insertCullNodes)
                {
                    auto cullGroup = vsg::CullGroup::create(boundingSphere);
                    nestedGroup->addChild(cullGroup);
                    nestedGroup = cullGroup;
                }
            }

            // has the geometry already been converted
            if (geometriesMap.find(geometry) != geometriesMap.end())
            {
                DEBUG_OUTPUT << "sharing geometry" << std::endl;
                nestedGroup->addChild(vsg::ref_ptr<vsg::Node>(geometriesMap[geometry]));
            }
            else
            {
                vsg::ref_ptr<vsg::Geometry> new_geometry = convertToVsg(geometry, requiredGeomAttributesMask);
                if (new_geometry)
                {
                    nestedGroup->addChild(new_geometry);
                    geometriesMap[geometry] = new_geometry;
                }
            }
#endif
        }
    }

    if (group->children.size() == 1) return vsg::ref_ptr<vsg::Node>(group->children[0]);

    return group;
}

vsg::ref_ptr<vsg::Node> SceneBuilder::createVSG(vsg::Paths& searchPaths)
{
    DEBUG_OUTPUT << "SceneBuilder::createVSG(vsg::Paths& searchPaths)" << std::endl;

    // clear caches
    geometriesMap.clear();
    texturesMap.clear();

    vsg::ref_ptr<vsg::Group> group = vsg::Group::create();

    vsg::ref_ptr<vsg::Group> opaqueGroup = vsg::Group::create();
    group->addChild(opaqueGroup);

    vsg::ref_ptr<vsg::Group> transparentGroup = vsg::Group::create();
    group->addChild(transparentGroup);

    for (auto [masks, transformStatePair] : masksTransformStateMap)
    {
        unsigned int maxNumDescriptors = transformStatePair.stateTransformMap.size();
        if (maxNumDescriptors == 0)
        {
            DEBUG_OUTPUT << "  Skipping empty transformStatePair" << std::endl;
            continue;
        }
        else
        {
            DEBUG_OUTPUT << "  maxNumDescriptors = " << maxNumDescriptors << std::endl;
        }

        uint32_t geometrymask = (masks.second | buildOptions->overrideGeomAttributes) & buildOptions->supportedGeometryAttributes;
        uint32_t shaderModeMask = (masks.first | buildOptions->overrideShaderModeMask) & buildOptions->supportedShaderModeMask;
        if (shaderModeMask & NORMAL_MAP) geometrymask |= TANGENT; // mesh propably won't have tangets so force them on if we want Normal mapping

        DEBUG_OUTPUT << "  about to call createStateSetWithGraphicsPipeline(" << shaderModeMask << ", " << geometrymask << ", " << maxNumDescriptors << ")" << std::endl;

        auto graphicsPipelineGroup = vsg::StateGroup::create();

        auto bindGraphicsPipeline = buildOptions->pipelineCache->getOrCreateBindGraphicsPipeline(shaderModeMask, geometrymask, buildOptions->vertexShaderPath, buildOptions->fragmentShaderPath);
        if (!bindGraphicsPipeline) continue;

        graphicsPipelineGroup->add(bindGraphicsPipeline);

        auto graphicsPipeline = bindGraphicsPipeline->pipeline;
        auto& descriptorSetLayouts = graphicsPipeline->layout->setLayouts;

        // attach based on use of transparency
        if (shaderModeMask & BLEND)
        {
            transparentGroup->addChild(graphicsPipelineGroup);
        }
        else
        {
            opaqueGroup->addChild(graphicsPipelineGroup);
        }

        for (auto [stateset, transformeGeometryMap] : transformStatePair.stateTransformMap)
        {
            vsg::ref_ptr<vsg::Node> transformGeometryGraph = createTransformGeometryGraphVSG(transformeGeometryMap, searchPaths, geometrymask);
            if (!transformGeometryGraph) continue;

            vsg::ref_ptr<vsg::DescriptorSet> descriptorSet = createVsgStateSet(descriptorSetLayouts.front(), stateset, shaderModeMask);
            if (descriptorSet)
            {
                auto stategroup = vsg::StateGroup::create();
                graphicsPipelineGroup->addChild(stategroup);
                stategroup->addChild(transformGeometryGraph);

                if (buildOptions->useBindDescriptorSet)
                {
                    auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->layout, 0, descriptorSet);
                    stategroup->add(bindDescriptorSet);
                }
                else
                {
                    auto bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->layout, 0, vsg::DescriptorSets{descriptorSet});
                    stategroup->add(bindDescriptorSets);
                }
            }
            else
            {
                graphicsPipelineGroup->addChild(transformGeometryGraph);
            }
        }
    }

    // if we are using CullGroups then place one at the top of the created scene graph
    if (buildOptions->insertCullGroups)
    {
        vsg::ComputeBounds computeBounds;
        group->accept(computeBounds);

        vsg::dsphere boundingSphere((computeBounds.bounds.min + computeBounds.bounds.max) * 0.5, vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.5);
        auto cullGroup = vsg::CullGroup::create(boundingSphere);

        // add the groups children to the cullGroup
        cullGroup->children = group->children;

        // now use the cullGroup as the root.
        group = cullGroup;
    }

    return group;
}

vsg::ref_ptr<vsg::Node> SceneBuilder::optimizeAndConvertToVsg(osg::ref_ptr<osg::Node> osg_scene, vsg::Paths& searchPaths)
{
    bool optimize = true;
    if (optimize)
    {
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
        optimizer.optimize(osg_scene, osgUtil::Optimizer::DEFAULT_OPTIMIZATIONS);

        OptimizeOsgBillboards optimizeBillboards;
        osg_scene->accept(optimizeBillboards);
        optimizeBillboards.optimize();
    }

    osg_scene->accept(*this);

    // build VSG scene
    return createVSG(searchPaths);
}
