#pragma once

#include <chrono>
#include <iostream>

#include <osg/Billboard>
#include <osg/MatrixTransform>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgUtil/Optimizer>

#include "BuildOptions.h"

namespace osg2vsg
{
    class SceneBuilderBase
    {
    public:
        SceneBuilderBase() {}

        SceneBuilderBase(vsg::ref_ptr<const BuildOptions> options) :
            buildOptions(options) {}

        using StateStack = std::vector<osg::ref_ptr<osg::StateSet>>;
        using StateSets = std::set<StateStack>;
        using StatePair = std::pair<osg::ref_ptr<osg::StateSet>, osg::ref_ptr<osg::StateSet>>;
        using StateMap = std::map<StateStack, StatePair>;
        using GeometriesMap = std::map<const osg::Geometry*, vsg::ref_ptr<vsg::Command>>;

        using TexturesMap = std::map<const osg::Texture*, vsg::ref_ptr<vsg::DescriptorImage>>;

        struct UniqueStateSet
        {
            bool operator()(const osg::ref_ptr<osg::StateSet>& lhs, const osg::ref_ptr<osg::StateSet>& rhs) const
            {
                if (!lhs) return true;
                if (!rhs) return false;
                return lhs->compare(*rhs) < 0;
            }
        };

        using UniqueStats = std::set<osg::ref_ptr<osg::StateSet>, UniqueStateSet>;

        vsg::ref_ptr<const BuildOptions> buildOptions = BuildOptions::create();

        uint32_t nodeShaderModeMasks = ShaderModeMask::NONE;

        StateStack statestack;
        StateMap stateMap;
        UniqueStats uniqueStateSets;
        TexturesMap texturesMap;
        bool writeToFileProgramAndDataSetSets = false;

        osg::ref_ptr<osg::StateSet> uniqueState(osg::ref_ptr<osg::StateSet> stateset, bool programStateSet);

        StatePair computeStatePair(osg::StateSet* stateset);
        StatePair& getStatePair();

        // core VSG style usage
        vsg::ref_ptr<vsg::DescriptorImage> convertToVsgTexture(const osg::Texture* osgtexture);

        vsg::ref_ptr<vsg::DescriptorSet> createVsgStateSet(vsg::ref_ptr<vsg::DescriptorSetLayout> descriptorSetLayout, const osg::StateSet* stateset, uint32_t shaderModeMask);
    };

    class SceneBuilder : public osg::NodeVisitor, public SceneBuilderBase
    {
    public:
        SceneBuilder() :
            osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ACTIVE_CHILDREN) {}

        SceneBuilder(vsg::ref_ptr<const BuildOptions> options) :
            osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ACTIVE_CHILDREN),
            SceneBuilderBase(options) {}

        using Geometries = std::vector<osg::ref_ptr<osg::Geometry>>;
        using StateGeometryMap = std::map<osg::ref_ptr<osg::StateSet>, Geometries>;
        using TransformGeometryMap = std::map<osg::Matrix, Geometries>;
        using MatrixStack = std::vector<osg::Matrixd>;

        struct TransformStatePair
        {
            std::map<osg::Matrix, StateGeometryMap> matrixStateGeometryMap;
            std::map<osg::ref_ptr<osg::StateSet>, TransformGeometryMap> stateTransformMap;
        };

        using Masks = std::pair<uint32_t, uint32_t>;
        using MasksTransformStateMap = std::map<Masks, TransformStatePair>;

        using ProgramTransformStateMap = std::map<osg::ref_ptr<osg::StateSet>, TransformStatePair>;

        MatrixStack matrixstack;
        ProgramTransformStateMap programTransformStateMap;
        MasksTransformStateMap masksTransformStateMap;
        GeometriesMap geometriesMap;

        osg::ref_ptr<osg::Node> createStateGeometryGraphOSG(StateGeometryMap& stateGeometryMap);
        osg::ref_ptr<osg::Node> createTransformGeometryGraphOSG(TransformGeometryMap& transformGeometryMap);
        osg::ref_ptr<osg::Node> createOSG();

        vsg::ref_ptr<vsg::Node> createTransformGeometryGraphVSG(TransformGeometryMap& transformGeometryMap, vsg::Paths& searchPaths, uint32_t requiredGeomAttributesMask);

        vsg::ref_ptr<vsg::Node> createVSG(vsg::Paths& searchPaths);

        void apply(osg::Node& node);
        void apply(osg::Group& group);
        void apply(osg::Transform& transform);
        void apply(osg::Billboard& billboard);
        void apply(osg::Geometry& geometry);

        void pushStateSet(osg::StateSet& stateset);
        void popStateSet();

        void pushMatrix(const osg::Matrix& matrix);
        void popMatrix();

        void print();

        vsg::ref_ptr<vsg::Node> optimizeAndConvertToVsg(osg::ref_ptr<osg::Node> scene, vsg::Paths& searchPaths);
    };
} // namespace osg2vsg
