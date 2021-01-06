#include "SceneAnalysis.h"

using namespace osg2vsg;

void SceneStats::print(std::ostream& out)
{
    out << "SceneStats class count: " << typeFrequencyMap.size() << "\n";
    size_t longestClassName = 0;
    for (auto& entry : typeFrequencyMap)
    {
        longestClassName = std::max(strlen(entry.first), longestClassName);
    }

    out << "\nUnique objects:\n";
    out << "    className";
    for (size_t i = strlen("className"); i < longestClassName; ++i) out << " ";
    out << "\tObjects\n";

    for (auto& [className, objectFrequencyMap] : typeFrequencyMap)
    {
        uint32_t totalInstances = 0;
        for (auto& entry : objectFrequencyMap)
        {
            totalInstances += entry.second;
        }

        if (objectFrequencyMap.size() == totalInstances)
        {
            out << "    " << className << "";
            for (size_t i = strlen(className); i < longestClassName; ++i) out << " ";
            out << "\t" << objectFrequencyMap.size() << "\n";
        }
    }

    out << "\nShared objects:\n";
    out << "    className";
    for (size_t i = strlen("className"); i < longestClassName; ++i) out << " ";
    out << "\tObjects\tInstances\n";

    for (auto& [className, objectFrequencyMap] : typeFrequencyMap)
    {
        uint32_t totalInstances = 0;
        for (auto& entry : objectFrequencyMap)
        {
            totalInstances += entry.second;
        }

        if (objectFrequencyMap.size() != totalInstances)
        {
            out << "    " << className << "";
            for (size_t i = strlen(className); i < longestClassName; ++i) out << " ";
            out << "\t" << objectFrequencyMap.size() << "\t" << totalInstances << "\n";
        }
    }
    out << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////
//
// OsgSceneAnalysis
//
OsgSceneAnalysis::OsgSceneAnalysis() :
    osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN),
    _sceneStats(new SceneStats)
{
}

OsgSceneAnalysis::OsgSceneAnalysis(SceneStats* sceneStats) :
    osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN),
    _sceneStats(sceneStats)
{
}

void OsgSceneAnalysis::apply(osg::Node& node)
{
    if (node.getStateSet()) apply(*node.getStateSet());

    _sceneStats->insert(&node);

    node.traverse(*this);
}

void OsgSceneAnalysis::apply(osg::Geometry& geometry)
{
    if (geometry.getStateSet()) apply(*geometry.getStateSet());

    _sceneStats->insert(&geometry);

    osg::Geometry::ArrayList arrayList;
    geometry.getArrayList(arrayList);
    for (auto& array : arrayList)
    {
        _sceneStats->insert(array.get());
    }

    for (auto& primitiveSet : geometry.getPrimitiveSetList())
    {
        _sceneStats->insert(primitiveSet.get());
    }
}

void OsgSceneAnalysis::apply(osg::StateSet& stateset)
{
    _sceneStats->insert(&stateset);

    for (auto& attribute : stateset.getAttributeList())
    {
        _sceneStats->insert(attribute.second.first.get());
    }

    for (auto& textureList : stateset.getTextureAttributeList())
    {
        for (auto& attribute : textureList)
        {
            _sceneStats->insert(attribute.second.first.get());
        }
    }

    for (auto& uniform : stateset.getUniformList())
    {
        _sceneStats->insert(uniform.second.first.get());
    }
}

///////////////////////////////////////////////////////////////////////////////////
//
// VsgSceneAnalysis
//
VsgSceneAnalysis::VsgSceneAnalysis() :
    _sceneStats(new SceneStats)
{
}

VsgSceneAnalysis::VsgSceneAnalysis(SceneStats* sceneStats) :
    _sceneStats(sceneStats)
{
}

void VsgSceneAnalysis::apply(const vsg::Object& object)
{
    _sceneStats->insert(&object);

    object.traverse(*this);
}

void VsgSceneAnalysis::apply(const vsg::Geometry& geometry)
{
    _sceneStats->insert(&geometry);

    for (auto& array : geometry.arrays)
    {
        _sceneStats->insert(array.get());
    }

    if (geometry.indices)
    {
        _sceneStats->insert(geometry.indices.get());
    }

    for (auto& command : geometry.commands)
    {
        command->accept(*this);
    }
}

void VsgSceneAnalysis::apply(const vsg::StateGroup& stategroup)
{
    _sceneStats->insert(&stategroup);

    for (auto& command : stategroup.getStateCommands())
    {
        command->accept(*this);
    }

    stategroup.traverse(*this);
}

#if 0
void VsgSceneAnalysis::apply(const vsg::Commands& commands) override
{
    _sceneStats->insert(&commands);
    for(auto& command : stategroup.getChildren())
    {
        _sceneStats->insert(command.get());
    }
}
#endif
