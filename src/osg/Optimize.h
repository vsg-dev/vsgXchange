#pragma once

#include <vsg/all.h>

#include <osg/Billboard>
#include <osg/MatrixTransform>

namespace osg2vsg
{

    class OptimizeOsgBillboards : public osg::NodeVisitor
    {
    public:
        OptimizeOsgBillboards();

        void apply(osg::Node& node);
        void apply(osg::Group& group);
        void apply(osg::Transform& transform);
        void apply(osg::Billboard& billboard);
        void apply(osg::Geometry& geometry);

        using TransformStack = std::stack<osg::ref_ptr<osg::Transform>>;
        TransformStack transformStack;

        using NodeSet = std::set<osg::Node*>;
        using TransformSubgraph = std::map<osg::Transform*, NodeSet>;

        TransformSubgraph transformSubgraph;

        void optimize();
    };
} // namespace osg2vsg
