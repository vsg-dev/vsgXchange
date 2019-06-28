#include <vsg/all.h>

#include <iostream>
#include <ostream>
#include <chrono>
#include <thread>

#include <vsgXchange/ShaderCompiler.h>

#include "AnimationPath.h"


namespace vsgXchange
{
    class VSGXCHANGE_DECLSPEC glslReaderWriter : public vsg::Inherit<vsg::ReaderWriter, glslReaderWriter>
    {
    public:
        glslReaderWriter();

        vsg::ref_ptr<vsg::Object> readFile(const vsg::Path& filename) const override;

        bool writeFile(const vsg::Object* object, const vsg::Path& filename) const override;

        void ext(const std::string ext, VkShaderStageFlagBits stage)
        {
            extensionToStage[ext] = stage;
            stageToExtension[stage] = ext;
        }

    protected:
        std::map<std::string, VkShaderStageFlagBits> extensionToStage;
        std::map<VkShaderStageFlagBits, std::string> stageToExtension;
    };
    //VSG_type_name(vsgXchange::glslReaderWriter);

glslReaderWriter::glslReaderWriter()
{
    ext("vert", VK_SHADER_STAGE_VERTEX_BIT);
    ext("tesc", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    ext("tese", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    ext("geom", VK_SHADER_STAGE_GEOMETRY_BIT);
    ext("frag", VK_SHADER_STAGE_FRAGMENT_BIT);
    ext("comp", VK_SHADER_STAGE_COMPUTE_BIT);
    ext("mesh", VK_SHADER_STAGE_MESH_BIT_NV);
    ext("task", VK_SHADER_STAGE_TASK_BIT_NV);
    ext("rgen", VK_SHADER_STAGE_RAYGEN_BIT_NV);
    ext("rint", VK_SHADER_STAGE_INTERSECTION_BIT_NV);
    ext("rahit", VK_SHADER_STAGE_ANY_HIT_BIT_NV);
    ext("rchit", VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
    ext("rmiss", VK_SHADER_STAGE_MISS_BIT_NV);
    ext("rcall", VK_SHADER_STAGE_CALLABLE_BIT_NV);
    ext("glsl", VK_SHADER_STAGE_ALL);
    ext("hlsl", VK_SHADER_STAGE_ALL);
}

vsg::ref_ptr<vsg::Object> glslReaderWriter::readFile(const vsg::Path& filename) const
{
    auto ext = vsg::fileExtension(filename);
    auto stage_itr = extensionToStage.find(ext);
    if (stage_itr != extensionToStage.end() && vsg::fileExists(filename))
    {
        std::cout<<"glslReaderWriter::readFile("<<filename<<") stage = "<<stage_itr->second<<std::endl;
        std::string source;

        std::ifstream fin(filename, std::ios::ate);
        size_t fileSize = fin.tellg();

        source.resize(fileSize);

        fin.seekg(0);
        fin.read(reinterpret_cast<char*>(source.data()), fileSize);
        fin.close();

        auto sm = vsg::ShaderModule::create(source);


        std::cout<<"source : "<<source<<std::endl;
        std::cout<<"sm : "<<sm<<std::endl;

        if (stage_itr->second == VK_SHADER_STAGE_ALL)
        {
            return sm;
        }
        else
        {
            return vsg::ShaderStage::create(stage_itr->second, "main", sm);
        }

        return sm;
    }
    return vsg::ref_ptr<vsg::Object>();
}

bool glslReaderWriter::writeFile(const vsg::Object* object, const vsg::Path& filename) const
{
    auto ext = vsg::fileExtension(filename);
    auto stage_itr = extensionToStage.find(ext);
    if (stage_itr != extensionToStage.end())
    {
        const vsg::ShaderStage* ss = dynamic_cast<const vsg::ShaderStage*>(object);
        const vsg::ShaderModule* sm = ss ? ss->getShaderModule() : dynamic_cast<const vsg::ShaderModule*>(object);
        if (sm)
        {
            if (!sm->source().empty())
            {
                std::ofstream fout(filename);
                fout.write(sm->source().data(), sm->source().size());
                fout.close();
                return true;
            }
        }
        std::cout<<"glslReaderWriter::wrteFile("<<filename<<") stage = "<<stage_itr->second<<std::endl;
    }
    return false;
}

};


int main(int argc, char** argv)
{
    // write out command line to aid debugging
    for(int i=0; i<argc; ++i)
    {
        std::cout<<argv[i]<<" ";
    }
    std::cout<<std::endl;

    auto windowTraits = vsg::Window::Traits::create();
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
        window->addStage(vsg::GraphicsStage::create(vsg_scene, camera));

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

            viewer->populateNextFrame();

            viewer->submitNextFrame();
        }

        // clean up done automatically thanks to ref_ptr<>
        return 0;
    };

    // read shaders
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    using VsgObjects = std::vector<vsg::ref_ptr<vsg::Object>>;
    VsgObjects vsgObjects;

    // read any vsg files

    vsg::CompositeReaderWriter io;

    io.add(vsg::vsgReaderWriter::create());
    io.add(vsgXchange::glslReaderWriter::create());

    for (int i=1; i<argc; ++i)
    {
        std::string filename = arguments[i];

        auto before_vsg_load = std::chrono::steady_clock::now();

        auto loaded_object = io.readFile(filename);

        auto vsg_loadTime = std::chrono::duration<double, std::chrono::milliseconds::period>(std::chrono::steady_clock::now() - before_vsg_load).count();

        if (loaded_object)
        {
            std::cout<<"VSG loadTime = "<<vsg_loadTime<<"ms"<<std::endl;

            vsgObjects.push_back(loaded_object);
            arguments.remove(i, 1);
            --i;
        }
        else
        {
            std::cout<<"VSG failed to load "<<filename<<std::endl;
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

            for(auto& stage : stagesToCompile)
            {
                std::cout<<"Stage : "<<stage->getShaderStageFlagBits()<<" compiled to spirv "<<stage->getShaderModule()->spirv().size()<<std::endl;
            }
        }

        vsg::Path outputFileExtension = vsg::fileExtension(outputFilename);
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
            io.writeFile(vsg_scene, outputFilename);
        }

    }
    else
    {
        std::cout<<"Mixed objects"<<std::endl;
    }

    return 1;
}
