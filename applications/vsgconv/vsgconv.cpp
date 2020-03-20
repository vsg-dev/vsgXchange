#include <vsg/all.h>

#include <iostream>
#include <ostream>
#include <chrono>
#include <thread>

#include <vsgXchange/ReaderWriter_all.h>
#include <vsgXchange/ShaderCompiler.h>

#include "AnimationPath.h"


int main(int argc, char** argv)
{
    // write out command line to aid debugging
    for(int i=0; i<argc; ++i)
    {
        std::cout<<argv[i]<<" ";
    }
    std::cout<<std::endl;

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    auto outputFilename = arguments.value(std::string(), "-o");
    auto pathFilename = arguments.value(std::string(),"-p");

    // read shaders
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    using VsgObjects = std::vector<vsg::ref_ptr<vsg::Object>>;
    VsgObjects vsgObjects;


    // ise the vsg::Options object to pass the ReaderWriter_all to use when reading files.
    auto options = vsg::Options::create();
    options->readerWriter = vsgXchange::ReaderWriter_all::create();

    // read any input files
    for (int i=1; i<argc; ++i)
    {
        std::string filename = arguments[i];

        auto before_vsg_load = std::chrono::steady_clock::now();

        auto loaded_object = vsg::read(filename, options);

        auto vsg_loadTime = std::chrono::duration<double, std::chrono::milliseconds::period>(std::chrono::steady_clock::now() - before_vsg_load).count();

        if (loaded_object)
        {
            std::cout<<"LoadTime = "<<vsg_loadTime<<"ms"<<std::endl;

            vsgObjects.push_back(loaded_object);
            arguments.remove(i, 1);
            --i;
        }
        else
        {
            std::cout<<"Failed to load "<<filename<<std::endl;
        }
    }

    if (vsgObjects.empty())
    {
        std::cout<<"No files loaded."<<std::endl;
        return 1;
    }

    unsigned int numImages = 0;
    unsigned int numShaders = 0;
    unsigned int numNodes = 0;

    for(auto& object : vsgObjects)
    {
        if (dynamic_cast<vsg::Data*>(object.get()))
        {
            ++numImages;
        }
        else if (dynamic_cast<vsg::ShaderModule*>(object.get()) || dynamic_cast<vsg::ShaderStage*>(object.get()))
        {
            ++numShaders;
        }
        else if (dynamic_cast<vsg::Node*>(object.get()))
        {
            ++numNodes;
        }
    }

    if (numImages==vsgObjects.size())
    {
        // all images
        std::cout<<"All images"<<std::endl;
        vsg::ref_ptr<vsg::Node> vsg_scene;


        if (numImages == 1)
        {
            auto group = vsg::Group::create();
            for(auto& object : vsgObjects)
            {
                vsg::ref_ptr<vsg::Node> node( dynamic_cast<vsg::Node*>(object.get()) );
                if (node) group->addChild(node);
            }
        }

        if (!outputFilename.empty())
        {
            if (numImages == 1)
            {
                auto image = vsgObjects[0].cast<vsg::Data>();
                vsg::write(image, outputFilename, options);
            }
            else
            {
                auto objects = vsg::Objects::create();
                for(auto& object : vsgObjects)
                {
                    objects->addChild(object);
                }
                vsg::write(objects, outputFilename, options);
            }
        }
    }
    else if (numShaders==vsgObjects.size())
    {
        // all shaders
        std::cout<<"All shaders"<<std::endl;

        vsg::ShaderStages stagesToCompile;
        for(auto& object : vsgObjects)
        {
            vsg::ShaderStage* ss = dynamic_cast<vsg::ShaderStage*>(object.get());
            vsg::ShaderModule* sm = ss ? ss->getShaderModule() : dynamic_cast<vsg::ShaderModule*>(object.get());
            if (sm && !sm->source().empty() && sm->spirv().empty())
            {
                if (ss) stagesToCompile.emplace_back(ss);
                else stagesToCompile.emplace_back(vsg::ShaderStage::create(VK_SHADER_STAGE_ALL, "main", vsg::ref_ptr<vsg::ShaderModule>(sm)));
            }
        }

        if (!stagesToCompile.empty())
        {
            vsg::ref_ptr<vsgXchange::ShaderCompiler> shaderCompiler(new vsgXchange::ShaderCompiler());
            shaderCompiler->compile(stagesToCompile);
        }

        if (!outputFilename.empty() && !stagesToCompile.empty())
        {
            // TODO work out how to handle multiple input shaders when we only have one output filename.
            vsg::write(stagesToCompile.front(), outputFilename, options);
        }
    }
    else if (numNodes==vsgObjects.size())
    {
        // all nodes
        std::cout<<"All nodes"<<std::endl;

        vsg::ref_ptr<vsg::Node> vsg_scene;
        if (numNodes == 1) vsg_scene = dynamic_cast<vsg::Node*>(vsgObjects.front().get());
        else
        {
            auto group = vsg::Group::create();
            for(auto& object : vsgObjects)
            {
                vsg::ref_ptr<vsg::Node> node( dynamic_cast<vsg::Node*>(object.get()) );
                if (node) group->addChild(node);
            }
        }

        if (!outputFilename.empty())
        {
            vsg::write(vsg_scene, outputFilename, options);
        }
    }
    else
    {
        std::cout<<"Mixed objects"<<std::endl;
        if (!outputFilename.empty())
        {
            if (vsgObjects.size()==1)
            {
                vsg::write(vsgObjects[0], outputFilename, options);
            }
            else
            {
                auto objects = vsg::Objects::create();
                for(auto& object : vsgObjects)
                {
                    objects->addChild(object);
                }
                vsg::write(objects, outputFilename, options);
            }
        }
    }

    return 1;
}
