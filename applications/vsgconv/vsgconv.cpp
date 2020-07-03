#include <vsg/all.h>

#include <iostream>
#include <ostream>
#include <chrono>
#include <thread>

#include <vsgXchange/ReaderWriter_all.h>
#include <vsgXchange/ShaderCompiler.h>

namespace vsgconv
{
    class LeafDataCollection : public vsg::Visitor
    {
    public:

        vsg::ref_ptr<vsg::Objects> objects;

        LeafDataCollection()
        {
            objects = new vsg::Objects;
        }

        void apply(vsg::Object& object) override
        {
            object.traverse(*this);
        }

        void apply(vsg::DescriptorSet& descriptorSet) override
        {
            objects->addChild(vsg::ref_ptr<Object>(&descriptorSet));
        }

        void apply(vsg::Geometry& geometry) override
        {
            for(auto& data : geometry.arrays)
            {
                objects->addChild(data);
            }
            if (geometry.indices)
            {
                objects->addChild(geometry.indices);
            }
        }

        void apply(vsg::VertexIndexDraw& vid) override
        {
            for(auto& data : vid.arrays)
            {
                objects->addChild(data);
            }
            if (vid.indices)
            {
                objects->addChild(vid.indices);
            }
        }

        void apply(vsg::BindVertexBuffers& bvb) override
        {
            for(auto& data : bvb.getArrays())
            {
                objects->addChild(data);
            }
        }

        void apply(vsg::BindIndexBuffer& bib) override
        {
            if (bib.getIndices())
            {
                objects->addChild(vsg::ref_ptr<vsg::Data>(bib.getIndices()));
            }
        }

        void apply(vsg::StateGroup& stategroup) override
        {
            for(auto& command : stategroup.getStateCommands())
            {
                command->accept(*this);
            }

            stategroup.traverse(*this);
        }
    };
}


int main(int argc, char** argv)
{
    // TODO:
    //   Add option for passing on controls to ReaderWriter's such as osg2vsg's controls for toggling lighting etc.

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    auto batchLeafData = arguments.read("--batch");

    // read shaders
    vsg::Paths searchPaths = vsg::getEnvPaths("VSG_FILE_PATH");

    // ise the vsg::Options object to pass the ReaderWriter_all to use when reading files.
    auto options = vsg::Options::create();
    options->readerWriter = vsgXchange::ReaderWriter_all::create();

    if (argc <= 1)
    {
        std::cout<<"Usage:\n";
        std::cout<<"    vsgconv input_filename output_filefilename\n";
        std::cout<<"    vsgconv input_filename_1 input_filefilename_2 output_filefilename\n";
        return 1;
    }

    vsg::Path outputFilename = arguments[argc-1];

    using VsgObjects = std::vector<vsg::ref_ptr<vsg::Object>>;
    VsgObjects vsgObjects;

    // read any input files
    for (int i=1; i<argc-1; ++i)
    {
        vsg::Path filename = arguments[i];

        auto loaded_object = vsg::read(filename, options);
        if (loaded_object)
        {
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
            vsg_scene = group;
        }

        if (batchLeafData)
        {
            vsgconv::LeafDataCollection leafDataCollection;
            vsg_scene->accept(leafDataCollection);
            vsg_scene->setObject("batch", leafDataCollection.objects);
        }

        struct FindPagedLOD : public vsg::ConstVisitor
        {
            bool found = false;
            bool operator () (const vsg::Node& node) { node.accept(*this); return found; }
            void apply(const vsg::Node& node) override { if (!found) node.traverse(*this); }
            void apply(const vsg::PagedLOD&) override { found = true; }
        } containsPagedLOD;

        if (containsPagedLOD(*vsg_scene))
        {
            std::cout<<"Scene graph contains PagedLOD, need implement copying of children across"<<std::endl;
        }

        if (!outputFilename.empty())
        {
            vsg::write(vsg_scene, outputFilename, options);
        }
    }
    else
    {
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
