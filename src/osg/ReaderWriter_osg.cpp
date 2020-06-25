#include "ReaderWriter_osg.h"

#include <osg/TransferFunction>
#include <osg/io_utils>
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

vsg::ref_ptr<vsg::Object> ReaderWriter_osg::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    osg::ref_ptr<osgDB::Options> osg_options;

    if (options)
    {
        if (osgDB::Registry::instance()->getOptions()) osg_options = osgDB::Registry::instance()->getOptions()->cloneOptions();
        else osg_options = new osgDB::Options();

        osg_options->getDatabasePathList().insert(osg_options->getDatabasePathList().end(), options->paths.begin(), options->paths.end());
    }

    osg::ref_ptr<osg::Object> object = osgDB::readRefObjectFile(filename, osg_options.get());
    if (!object)
    {
        return {};
    }

    if (osg::Node* osg_scene = object->asNode(); osg_scene != nullptr)
    {
        vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");  // TODO, use the vsg::Options ?
        auto buildOptions = osg2vsg::BuildOptions::create(); // TODO, use the vsg::Options to set buildOptions?

        osg2vsg::SceneBuilder sceneBuilder(buildOptions);
        auto converted_vsg_scene = sceneBuilder.optimizeAndConvertToVsg(osg_scene, searchPaths);

        return converted_vsg_scene;
    }
    else if (osg::Image* osg_image = object->asImage(); osg_image != nullptr)
    {
        return osg2vsg::convertToVsg(osg_image);
    }
    else if (osg::TransferFunction1D* tf = dynamic_cast<osg::TransferFunction1D*>(object.get()); tf != nullptr)
    {
        auto tf_image = tf->getImage();
        auto vsg_image = osg2vsg::convertToVsg(tf_image);
        vsg_image->setValue("Minimum", tf->getMinimum());
        vsg_image->setValue("Maximum", tf->getMaximum());

        return vsg_image;
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
