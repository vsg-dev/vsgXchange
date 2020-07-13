#include "Optimize.h"

#include "ImageUtils.h"
#include "GeometryUtils.h"
#include "ShaderUtils.h"

#include <vsg/nodes/MatrixTransform.h>
#include <vsg/nodes/CullGroup.h>
#include <vsg/nodes/CullNode.h>

#include <osg/io_utils>

using namespace osg2vsg;


OptimizeOsgBillboards::OptimizeOsgBillboards():
    osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ACTIVE_CHILDREN)
{
}

void OptimizeOsgBillboards::apply(osg::Node& node)
{
    osg::Transform* topTransform = transformStack.empty() ? nullptr : transformStack.top();
    transformSubgraph[topTransform].insert(&node);

    traverse(node);
}

void OptimizeOsgBillboards::apply(osg::Group& group)
{
    // osg::Transform* topTransform = transformStack.empty() ? nullptr : transformStack.top();
    // transformSubgraph[topTransform].insert(&group);

    traverse(group);
}

void OptimizeOsgBillboards::apply(osg::Transform& transform)
{
    osg::Transform* topTransform = transformStack.empty() ? nullptr : transformStack.top();

    transformStack.push(&transform);

    transformSubgraph[topTransform].insert(&transform);

    traverse(transform);

    transformStack.pop();
}

void OptimizeOsgBillboards::apply(osg::Billboard& billboard)
{
    //std::cout<<"OptimizeOsgBillboards::apply(osg::Billboard& "<<&billboard<<") \n";

    osg::Transform* topTransform = transformStack.empty() ? nullptr : transformStack.top();

    transformSubgraph[topTransform].insert(&billboard);
}

void OptimizeOsgBillboards::apply(osg::Geometry& geometry)
{
    osg::Transform* topTransform = transformStack.empty() ? nullptr : transformStack.top();

    transformSubgraph[topTransform].insert(&geometry);

    traverse(geometry);
}

void OptimizeOsgBillboards::optimize()
{
    using Billboards = std::set<osg::Billboard*>;
    using Transforms = std::set<osg::Transform*>;
    using TransformBillboardMap = std::map<osg::Transform*, Billboards>;
    using BillboardTransformMap = std::map<osg::Billboard*, Transforms>;


    TransformBillboardMap transformBillboardMap;
    BillboardTransformMap billboardTransformMap;

    for(auto[transform,nodes] : transformSubgraph)
    {
        size_t numBillboards = 0;
        for(auto& node : nodes)
        {
            osg::Billboard* billboard = dynamic_cast<osg::Billboard*>(node);
            if (billboard) ++numBillboards;
        }
        if (numBillboards>0 && numBillboards==nodes.size())
        {
            for(auto& node : nodes)
            {
                osg::Billboard* billboard = dynamic_cast<osg::Billboard*>(node);
                transformBillboardMap[transform].insert(billboard);
                billboardTransformMap[billboard].insert(transform);
            }
        }
    }

    using ReplacementMap = std::map<osg::ref_ptr<osg::Node>, osg::ref_ptr<osg::Node>>;
    ReplacementMap replacementMap;

    for(auto[billboard, transforms] : billboardTransformMap)
    {
        bool transformsUniqueMapToBillboard = true;
        for(auto& transform : transforms)
        {
            if (transformBillboardMap[transform].size()>1)
            {
                transformsUniqueMapToBillboard = false;
            }
        }
        if (transformsUniqueMapToBillboard)
        {
            osg::ref_ptr<osg::Billboard> new_billboard = new osg::Billboard;

            new_billboard->setStateSet(billboard->getStateSet());

            for(auto& transform : transforms)
            {
                if (transform)
                {
                    osg::Matrix matrix;
                    transform->computeLocalToWorldMatrix(matrix, nullptr);

                    unsigned int numPositions = std::min(static_cast<unsigned int>(billboard->getPositionList().size()), billboard->getNumDrawables());
                    for(unsigned int i=0; i<numPositions; ++i)
                    {
                        auto position = billboard->getPosition(i);
                        new_billboard->addDrawable(billboard->getDrawable(i), position * matrix);
                    }
                }
                else
                {
                    unsigned int numPositions = std::min(static_cast<unsigned int>(billboard->getPositionList().size()), billboard->getNumDrawables());
                    for(unsigned int i=0; i<numPositions; ++i)
                    {
                        auto position = billboard->getPosition(i);
                        new_billboard->addDrawable(billboard->getDrawable(i), position);
                    }
                }

                replacementMap[transform] = nullptr;
            }
            replacementMap[*transforms.begin()] = new_billboard;
        }
    }

    for(auto[nodeToReplace, replacementNode] : replacementMap)
    {
        if (replacementNode)
        {
            for(auto parent : nodeToReplace->getParents())
            {
                parent->replaceChild(nodeToReplace.get(), replacementNode.get());
            }
        }
        else
        {
            for(auto parent : nodeToReplace->getParents())
            {
                parent->removeChild(nodeToReplace.get());
            }
        }
    }

}
