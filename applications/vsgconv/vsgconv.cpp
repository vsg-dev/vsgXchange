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

    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "xchange";

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);
    windowTraits->debugLayer = arguments.read({"--debug","-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api","-a"});
    if (arguments.read("--IMMEDIATE")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (arguments.read("--FIFO")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (arguments.read("--FIFO_RELAXED")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    if (arguments.read("--MAILBOX")) windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    if (arguments.read({"-t", "--test"})) { windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; windowTraits->fullscreen = true; }
    if (arguments.read({"--st", "--small-test"})) { windowTraits->swapchainPreferences.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; windowTraits->width = 192, windowTraits->height = 108; windowTraits->decoration = false; }
    if (arguments.read({"--fullscreen", "--fs"})) windowTraits->fullscreen = true;
    if (arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) { windowTraits->fullscreen = false; }
    if (arguments.read({"--no-frame", "--nf"})) windowTraits->decoration = false;
    auto numFrames = arguments.value(-1, "-f");
    auto outputFilename = arguments.value(std::string(), "-o");
    auto pathFilename = arguments.value(std::string(),"-p");

    auto run_viewer = [&](vsg::ref_ptr<vsg::Node> vsg_scene)
    {
        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();

        vsg::ref_ptr<vsg::Window> window(vsg::Window::create(windowTraits));
        if (!window)
        {
            std::cout<<"Could not create windows."<<std::endl;
            return 1;
        }

        viewer->addWindow(window);


        // compute the bounds of the scene graph to help position camera
        vsg::ComputeBounds computeBounds;
        vsg_scene->accept(computeBounds);
        vsg::dvec3 centre = (computeBounds.bounds.min+computeBounds.bounds.max)*0.5;
        double radius = vsg::length(computeBounds.bounds.max-computeBounds.bounds.min)*0.6;
        double nearFarRatio = 0.0001;

        // set up the camera
        vsg::ref_ptr<vsg::Perspective> perspective(new vsg::Perspective(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio*radius, radius * 4.5));
        vsg::ref_ptr<vsg::LookAt> lookAt(new vsg::LookAt(centre+vsg::dvec3(0.0, -radius*3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0)));
        vsg::ref_ptr<vsg::Camera> camera(new vsg::Camera(perspective, lookAt, vsg::ViewportState::create(window->extent2D())));


        // add a GraphicsStage tp the Window to do dispatch of the command graph to the commnad buffer(s)
        auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

        auto before_compile = std::chrono::steady_clock::now();

        // compile the Vulkan objects
        viewer->compile();

        std::cout<<"Compile traversal time "<<std::chrono::duration<double, std::chrono::milliseconds::period>(std::chrono::steady_clock::now() - before_compile).count()<<"ms"<<std::endl;;

        // add close handler to respond the close window button and pressing esape
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));

        if (pathFilename.empty())
        {
            viewer->addEventHandler(vsg::Trackball::create(camera));
        }
        else
        {
            std::ifstream in(pathFilename);
            if (!in)
            {
                std::cout << "AnimationPat: Could not open animation path file \"" << pathFilename << "\".\n";
                return 1;
            }

            vsg::ref_ptr<vsg::AnimationPath> animationPath(new vsg::AnimationPath);
            animationPath->read(in);

            viewer->addEventHandler(vsg::AnimationPathHandler::create(camera, animationPath, viewer->start_point()));
        }

        // rendering main loop
        while (viewer->advanceToNextFrame() && (numFrames<0 || (numFrames--)>0))
        {
            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();

            viewer->update();

            viewer->recordAndSubmit();

            viewer->present();
        }

        // clean up done automatically thanks to ref_ptr<>
        return 0;
    };

    // read shaders
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    using VsgObjects = std::vector<vsg::ref_ptr<vsg::Object>>;
    VsgObjects vsgObjects;

    // read any input files
    vsgXchange::ReaderWriter_all io;

    auto options = vsg::Options::create();

    for (int i=1; i<argc; ++i)
    {
        std::string filename = arguments[i];

        auto before_vsg_load = std::chrono::steady_clock::now();

        auto loaded_object = io.read(filename, options);

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
            io.write(stagesToCompile.front(), outputFilename);
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

        if (outputFilename.empty())
        {
            return run_viewer(vsg_scene);
        }
        else
        {
            io.write(vsg_scene, outputFilename);
        }

    }
    else
    {
        std::cout<<"Mixed objects"<<std::endl;
    }

    return 1;
}
