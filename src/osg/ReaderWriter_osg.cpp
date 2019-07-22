#include "ReaderWriter_osg.h"

#include <osgDB/ReadFile>

#include <iostream>

using namespace vsgXchange;

ReaderWriter_osg::ReaderWriter_osg()
{
}

vsg::ref_ptr<vsg::Object> ReaderWriter_osg::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options>  /*options*/) const
{
    osg::ref_ptr<osg::Object> object = osgDB::readRefObjectFile(filename);

    std::cout<<"ReaderWriter_osg::readFile("<<filename<<") built with OSG "<<std::endl;
    if (object.valid())
    {
        std::cout<<"    loaded OSG model : "<<object->className()<<std::endl;
    }

    return vsg::ref_ptr<vsg::Object>();
}

bool ReaderWriter_osg::write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options>  /*options*/) const
{
    std::cout<<"ReaderWriter_osg::writeFile("<<object->className()<<", "<<filename<<") built with OSG"<<std::endl;

    return false;
}
