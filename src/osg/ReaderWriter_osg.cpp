#include "ReaderWriter_osg.h"

#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgUtil/Optimizer>
#include <osgUtil/MeshOptimizers>

#include <osg2vsg/SceneBuilder.h>
#include <osg2vsg/ImageUtils.h>
#include <osg2vsg/Optimize.h>

#include <iostream>

using namespace vsgXchange;

ReaderWriter_osg::ReaderWriter_osg()
{
}

vsg::ref_ptr<vsg::Object> ReaderWriter_osg::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options>  /*options*/) const
{

    osg::ref_ptr<osg::Object> object = osgDB::readRefObjectFile(filename);
    if (!object)
    {
        return {};
    }


    if (osg::Node* osg_scene = object->asNode(); osg_scene != nullptr)
    {
        bool optimize = true;
        if (optimize)
        {
            osgUtil::IndexMeshVisitor imv;
            #if OSG_MIN_VERSION_REQUIRED(3,6,4)
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

            osg2vsg::OptimizeOsgBillboards optimizeBillboards;
            osg_scene->accept(optimizeBillboards);
            optimizeBillboards.optimize();
        }

        auto buildOptions = osg2vsg::BuildOptions::create(); // TODO, use the vsg::Options to set buildOptions?

        osg2vsg::SceneBuilder sceneBuilder(buildOptions);
        osg_scene->accept(sceneBuilder);

        vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");  // TODO, use the vsg::Options ?

        // build VSG scene
        auto converted_vsg_scene = sceneBuilder.createVSG(searchPaths);

        return converted_vsg_scene;
    }
    else if (osg::Image* osg_image = object->asImage(); osg_image != nullptr)
    {
        return osg2vsg::convertToVsg(osg_image);
    }
    else
    {
        std::cout<<"ReaderWriter_osg::readFile("<<filename<<") cannot convert object type "<<object->className()<<"."<<std::endl;
    }

    return {};
}

bool ReaderWriter_osg::write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options>  /*options*/) const
{
    std::cout<<"ReaderWriter_osg::writeFile("<<object->className()<<", "<<filename<<") using OSG not supported yet."<<std::endl;

    return false;
}
