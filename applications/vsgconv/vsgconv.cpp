#include <vsg/all.h>

#include <chrono>
#include <iostream>
#include <ostream>
#include <thread>

#include <vsg/vk/ShaderCompiler.h>
#include <vsgXchange/Version.h>
#include <vsgXchange/all.h>

namespace vsgconv
{
    static std::mutex s_log_mutex;

    template<typename... Args>
    void log(Args... args)
    {
        std::scoped_lock lock(s_log_mutex);
        (std::cout << ... << args) << std::endl;
    }

    void writeAndMakeDirectoryIfRequired(vsg::ref_ptr<vsg::Object> object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options)
    {
        vsg::Path path = vsg::filePath(filename);
        if (!path.empty() && !vsg::fileExists(path))
        {
            if (!vsg::makeDirectory(path))
            {
                log("Warning: could not create directory for ", path);
                return;
            }
        }
        vsg::write(object, filename, options);
    }

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
            for (auto& array : geometry.arrays)
            {
                if (array->data) objects->addChild(array->data);
            }
            if (geometry.indices && geometry.indices->data)
            {
                objects->addChild(geometry.indices->data);
            }
        }

        void apply(vsg::VertexIndexDraw& vid) override
        {
            for (auto& array : vid.arrays)
            {
                if (array->data) objects->addChild(array->data);
            }
            if (vid.indices)
            {
                objects->addChild(vid.indices);
            }
        }

        void apply(vsg::BindVertexBuffers& bvb) override
        {
            for (auto& array : bvb.arrays)
            {
                if (array->data) objects->addChild(array->data);
            }
        }

        void apply(vsg::BindIndexBuffer& bib) override
        {
            if (bib.indices && bib.indices->data)
            {
                objects->addChild(bib.indices->data);
            }
        }

        void apply(vsg::StateGroup& stategroup) override
        {
            for (auto& command : stategroup.stateCommands)
            {
                command->accept(*this);
            }

            stategroup.traverse(*this);
        }
    };

    struct ReadRequest
    {
        vsg::ref_ptr<const vsg::Options> options;
        vsg::Path src_filename;
        vsg::Path dest_filename;
    };

    struct CollectReadRequests : public vsg::Visitor
    {
        vsg::Path dest_path;
        vsg::Path dest_extension = ".vsgb";
        std::map<vsg::Path, ReadRequest> readRequests;

        bool operator()(vsg::Object& object, const vsg::Path& dest_filename)
        {
            dest_path = vsg::filePath(dest_filename);
            dest_extension = vsg::fileExtension(dest_filename);

            object.accept(*this);
            return !readRequests.empty();
        }

        void apply(vsg::Node& node) override
        {
            node.traverse(*this);
        }

        void apply(vsg::PagedLOD& plod) override
        {
            if (!plod.filename.empty())
            {
                if (readRequests.count(plod.filename) == 0)
                {
                    auto src_filename = plod.filename;
                    auto dest_base_filename = vsg::concatPaths(vsg::filePath(src_filename), vsg::simpleFilename(src_filename)) + dest_extension;
                    auto dest_filename = vsg::concatPaths(dest_path, dest_base_filename);

                    readRequests[plod.filename] = {plod.options, src_filename, dest_filename};
                    plod.filename = dest_base_filename;
                }
            }

            plod.traverse(*this);
        }
    };

    struct ReadOperation : public vsg::Inherit<vsg::Operation, ReadOperation>
    {
        ReadOperation(vsg::observer_ptr<vsg::OperationQueue> in_queue, vsg::ref_ptr<vsg::Latch> in_latch, ReadRequest in_readRequest, size_t in_level, size_t in_max_level) :
            level(in_level),
            max_level(in_max_level),
            queue(in_queue),
            latch(in_latch),
            readRequest(in_readRequest)
        {
        }

        void run() override
        {
            auto vsg_scene = vsg::read(readRequest.src_filename, readRequest.options);
            if (vsg_scene)
            {
                log("   loaded ", readRequest.src_filename, ", writing to ", readRequest.dest_filename, ", level ", level);

                vsgconv::CollectReadRequests collectReadRequests;
                if (level < max_level && collectReadRequests(*vsg_scene, readRequest.dest_filename))
                {
                    vsg::ref_ptr<vsg::OperationQueue> ref_queue = queue;

                    for (auto itr = collectReadRequests.readRequests.begin(); itr != collectReadRequests.readRequests.end(); ++itr)
                    {
                        latch->count_up();

                        ref_queue->add(vsgconv::ReadOperation::create(queue, latch, itr->second, level + 1, max_level));
                    }
                }

                vsgconv::writeAndMakeDirectoryIfRequired(vsg_scene, readRequest.dest_filename, readRequest.options);
            }
            else
            {
                log("   failed to read ", readRequest.src_filename);
            }

            // we have finsihed this read operation so decrement the latch, which will release and threads waiting on it.
            latch->count_down();
        }

        size_t level;
        size_t max_level;
        vsg::observer_ptr<vsg::OperationQueue> queue;
        vsg::ref_ptr<vsg::Latch> latch;
        ReadRequest readRequest;
    };

    struct indent
    {
        int chars = 0;
    };

    std::ostream& operator<<(std::ostream& input, indent in)
    {
        for (int i = 0; i < in.chars; ++i) input << ' ';
        return input;
    }

    struct pad
    {
        const char* str;
        size_t chars;
    };

    std::ostream& operator<<(std::ostream& input, pad in)
    {
        input << in.str;
        for (size_t i = strlen(in.str); i < in.chars; ++i) input << ' ';
        return input;
    }

    void printFeatures(std::ostream& out, vsg::ref_ptr<vsg::ReaderWriter> rw, int indentation = 0)
    {
        if (auto cws = rw.cast<vsg::CompositeReaderWriter>(); cws)
        {
            out << cws->className() << std::endl;
            for (auto& child : cws->readerWriters)
            {
                printFeatures(out, child, indentation + 4);
            }
        }
        else
        {
            vsg::ReaderWriter::Features features;
            rw->getFeatures(features);
            out << indent{indentation} << rw->className() << " provides support for " << features.extensionFeatureMap.size() << " extensions, and " << features.protocolFeatureMap.size() << " protocols." << std::endl;

            indentation += 4;
            bool precedingNewline = false;

            if (!features.protocolFeatureMap.empty())
            {
                if (precedingNewline) out<<std::endl;

                size_t padding = 16;
                out << indent{indentation} << pad{"Protocols", padding} << "Supported ReaderWriter methods" << std::endl;
                out << indent{indentation} << pad{"----------", padding} << "------------------------------" << std::endl;
                for (auto& [protocol, featureMask] : features.protocolFeatureMap)
                {
                    out << indent{indentation} << pad{protocol.c_str(), padding};

                    if (featureMask & vsg::ReaderWriter::READ_FILENAME) out << "read(vsg::Path, ..) ";
                    if (featureMask & vsg::ReaderWriter::READ_ISTREAM) out << "read(std::istream, ..) ";
                    if (featureMask & vsg::ReaderWriter::READ_MEMORY) out << "read(uint8_t* ptr, size_t size, ..) ";

                    if (featureMask & vsg::ReaderWriter::WRITE_FILENAME) out << "write(vsg::Path, ..) ";
                    if (featureMask & vsg::ReaderWriter::WRITE_OSTREAM) out << "write(std::ostream, ..) ";
                    out << std::endl;
                }
                precedingNewline = true;
            }

            if (!features.extensionFeatureMap.empty())
            {
                if (precedingNewline) out<<std::endl;

                size_t padding = 16;
                out << indent{indentation} << pad{"Extensions", padding} << "Supported ReaderWriter methods" << std::endl;
                out << indent{indentation} << pad{"----------", padding} << "------------------------------" << std::endl;
                for (auto& [ext, featureMask] : features.extensionFeatureMap)
                {
                    out << indent{indentation} << pad{ext.c_str(), padding};

                    if (featureMask & vsg::ReaderWriter::READ_FILENAME) out << "read(vsg::Path, ..) ";
                    if (featureMask & vsg::ReaderWriter::READ_ISTREAM) out << "read(std::istream, ..) ";
                    if (featureMask & vsg::ReaderWriter::READ_MEMORY) out << "read(uint8_t* ptr, size_t size, ..) ";

                    if (featureMask & vsg::ReaderWriter::WRITE_FILENAME) out << "write(vsg::Path, ..) ";
                    if (featureMask & vsg::ReaderWriter::WRITE_OSTREAM) out << "write(std::ostream, ..) ";
                    out << std::endl;
                }
                precedingNewline = true;
            }

            if (!features.optionNameTypeMap.empty())
            {
                if (precedingNewline) out<<std::endl;

                // expand the padding to encompass any long value strings
                size_t maxValueWidth = 19;
                for (auto& vt : features.optionNameTypeMap)
                {
                    if (vt.first.length() > maxValueWidth) maxValueWidth = vt.first.length();
                }

                size_t padding = maxValueWidth+2;

                // print out the options
                out << indent{indentation} << pad{"vsg::Options::Value", padding} << "type" << std::endl;
                out << indent{indentation} << pad{"-------------------", padding} << "----" << std::endl;
                for (auto& [value, type] : features.optionNameTypeMap)
                {
                    out << indent{indentation} << pad{value.c_str(), padding}<<type<< std::endl;
                }
                precedingNewline = true;
            }
        }
        out << std::endl;
    };

    void printMatchedFeatures(std::ostream& out, const std::string& rw_name, vsg::ref_ptr<vsg::ReaderWriter> rw, int indentation = 0)
    {
        if (rw_name == rw->className())
        {
            printFeatures(out, rw, indentation);
            return;
        }

        if (auto cws = rw.cast<vsg::CompositeReaderWriter>(); cws)
        {
            for (auto& child : cws->readerWriters)
            {
                printMatchedFeatures(out, rw_name, child, indentation);
            }
        }
    };

} // namespace vsgconv

int main(int argc, char** argv)
{
    // use the vsg::Options object to pass the ReaderWriter_all to use when reading files.
    auto options = vsg::Options::create(vsgXchange::all::create());
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    if (argc <= 1)
    {
        std::cout << "Usage:\n";
        std::cout << "    vsgconv input_filename output_filefilename\n";
        std::cout << "    vsgconv input_filename_1 input_filefilename_2 output_filefilename\n";
        return 1;
    }

    if (arguments.read("--rgb")) options->mapRGBtoRGBAHint = false;

    // read any commmand line options that the ReaderWrite support
    arguments.read(options);
    if (argc <= 1) return 0;

    if (arguments.read({"-v", "--version"}))
    {
        std::cout << "vsgXchange version = " << vsgXchangeGetVersionString() << ", so = " << vsgXchangeGetSOVersionString() << std::endl;
        if (vsgXchangeBuiltAsSharedLibrary())
            std::cout << "vsgXchange built as shared library" << std::endl;
        else
            std::cout << "vsgXchange built as static library" << std::endl;
        return 1;
    }

    std::string rw_name;
    if (arguments.read("--features", rw_name) || arguments.read("--features"))
    {
        if (rw_name.empty())
        {
            for (auto rw : options->readerWriters)
            {
                vsgconv::printFeatures(std::cout, rw);
            }
        }
        else
        {
            for (auto rw : options->readerWriters)
            {
                vsgconv::printMatchedFeatures(std::cout, rw_name, rw);
            }
        }

        return 0;
    }

    auto batchLeafData = arguments.read("--batch");
    auto levels = arguments.value(0, "-l");
    auto numThreads = arguments.value(16, "-t");
    bool compileShaders = !arguments.read({"--no-compile", "--nc"});

    vsg::Path outputFilename = arguments[argc - 1];

    using VsgObjects = std::vector<vsg::ref_ptr<vsg::Object>>;
    VsgObjects vsgObjects;

    // read any input files
    for (int i = 1; i < argc - 1; ++i)
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
            std::cout << "Failed to load " << filename << std::endl;
        }
    }

    if (vsgObjects.empty())
    {
        std::cout << "No files loaded." << std::endl;
        return 1;
    }

    unsigned int numImages = 0;
    unsigned int numShaders = 0;
    unsigned int numNodes = 0;

    for (auto& object : vsgObjects)
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

    if (numImages == vsgObjects.size())
    {
        // all images
        vsg::ref_ptr<vsg::Node> vsg_scene;

        if (numImages == 1)
        {
            auto group = vsg::Group::create();
            for (auto& object : vsgObjects)
            {
                vsg::ref_ptr<vsg::Node> node(dynamic_cast<vsg::Node*>(object.get()));
                if (node) group->addChild(node);
            }
        }

        if (!outputFilename.empty())
        {
            if (numImages == 1)
            {
                auto image = vsgObjects[0].cast<vsg::Data>();
                vsgconv::writeAndMakeDirectoryIfRequired(image, outputFilename, options);
            }
            else
            {
                auto objects = vsg::Objects::create();
                for (auto& object : vsgObjects)
                {
                    objects->addChild(object);
                }
                vsgconv::writeAndMakeDirectoryIfRequired(objects, outputFilename, options);
            }
        }
    }
    else if (numShaders == vsgObjects.size())
    {
        // all shaders
        if (compileShaders)
        {
            vsg::ShaderStages stagesToCompile;
            for (auto& object : vsgObjects)
            {
                vsg::ShaderStage* ss = dynamic_cast<vsg::ShaderStage*>(object.get());
                vsg::ShaderModule* sm = ss ? ss->module.get() : dynamic_cast<vsg::ShaderModule*>(object.get());
                if (sm && !sm->source.empty() && sm->code.empty())
                {
                    if (ss)
                        stagesToCompile.emplace_back(ss);
                    else
                        stagesToCompile.emplace_back(vsg::ShaderStage::create(VK_SHADER_STAGE_ALL, "main", vsg::ref_ptr<vsg::ShaderModule>(sm)));
                }
            }

            if (!stagesToCompile.empty())
            {
                auto shaderCompiler = vsg::ShaderCompiler::create();
                shaderCompiler->compile(stagesToCompile);
            }

            if (!outputFilename.empty() && !stagesToCompile.empty())
            {
                // TODO work out how to handle multiple input shaders when we only have one output filename.
                vsgconv::writeAndMakeDirectoryIfRequired(stagesToCompile.front(), outputFilename, options);
            }
        }
        else
        {
            // TODO work out how to handle multiple input shaders when we only have one output filename.
            vsgconv::writeAndMakeDirectoryIfRequired(vsgObjects.front(), outputFilename, options);
        }
    }
    else if (numNodes == vsgObjects.size())
    {
        // all nodes
        vsg::ref_ptr<vsg::Node> vsg_scene;
        if (numNodes == 1)
            vsg_scene = dynamic_cast<vsg::Node*>(vsgObjects.front().get());
        else
        {
            auto group = vsg::Group::create();
            for (auto& object : vsgObjects)
            {
                vsg::ref_ptr<vsg::Node> node(dynamic_cast<vsg::Node*>(object.get()));
                if (node) group->addChild(node);
            }
            vsg_scene = group;
        }

        auto shaderCompiler = vsg::ShaderCompiler::create();
        vsg_scene->accept(*shaderCompiler);

        if (batchLeafData)
        {
            vsgconv::LeafDataCollection leafDataCollection;
            vsg_scene->accept(leafDataCollection);
            vsg_scene->setObject("batch", leafDataCollection.objects);
        }

        vsgconv::CollectReadRequests collectReadRequests;

        if (levels > 0 && collectReadRequests(*vsg_scene, outputFilename))
        {
            vsgconv::writeAndMakeDirectoryIfRequired(vsg_scene, outputFilename, options);

            auto status = vsg::ActivityStatus::create();
            auto operationThreads = vsg::OperationThreads::create(numThreads, status);
            auto operationQueue = operationThreads->queue;
            auto latch = vsg::Latch::create(collectReadRequests.readRequests.size());

            vsg::observer_ptr<vsg::OperationQueue> obs_queue(operationQueue);

            for (auto itr = collectReadRequests.readRequests.begin(); itr != collectReadRequests.readRequests.end(); ++itr)
            {
                operationQueue->add(vsgconv::ReadOperation::create(obs_queue, latch, itr->second, 1, levels));
            }

            // wait until the latch goes zero i.e. all read operations have completed
            latch->wait();

            // signal that we are finished and the thread should close
            status->set(false);
        }
        else
        {
            vsgconv::writeAndMakeDirectoryIfRequired(vsg_scene, outputFilename, options);
        }
    }
    else
    {
        if (!outputFilename.empty())
        {
            if (vsgObjects.size() == 1)
            {
                vsgconv::writeAndMakeDirectoryIfRequired(vsgObjects[0], outputFilename, options);
            }
            else
            {
                auto objects = vsg::Objects::create();
                for (auto& object : vsgObjects)
                {
                    objects->addChild(object);
                }
                vsgconv::writeAndMakeDirectoryIfRequired(objects, outputFilename, options);
            }
        }
    }

    return 0;
}
