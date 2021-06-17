#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2021 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shimages be included in images
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/ReaderWriter.h>
#include <vsgXchange/Version.h>
#include <vsg/all.h>

#include <memory>
namespace osg2vsg
{
    struct PipelineCache : public vsg::Inherit<vsg::Object, PipelineCache>
    {
        using Key = std::tuple<uint32_t, uint32_t, std::string, std::string>;
        using PipelineMap = std::map<Key, vsg::ref_ptr<vsg::BindGraphicsPipeline>>;

        std::mutex mutex;
        PipelineMap pipelineMap;

        vsg::ref_ptr<vsg::BindGraphicsPipeline> getOrCreateBindGraphicsPipeline(uint32_t shaderModeMask, uint32_t geometryMask, const std::string& vertShaderPath = "", const std::string& fragShaderPath = "");
    };
} // namespace osg2vsg

namespace vsgXchange
{
    /// Composite ReaderWriter that holds the uses load 3rd party images formats.
    /// By defalt utilizes the stbi, dds and ktx ReaderWriters so that users only need to create vsgXchange::images::create() to utilize them all.
    class VSGXCHANGE_DECLSPEC models : public vsg::Inherit<vsg::CompositeReaderWriter, models>
    {
    public:
        models();
    };

    /// optional assimp ReaderWriter
    class VSGXCHANGE_DECLSPEC assimp : public vsg::Inherit<vsg::ReaderWriter, assimp>
    {
    public:
        assimp();

        vsg::ref_ptr<vsg::Object> read(const vsg::Path&, vsg::ref_ptr<const vsg::Options>) const;
        vsg::ref_ptr<vsg::Object> read(std::istream&, vsg::ref_ptr<const vsg::Options>) const;
        vsg::ref_ptr<vsg::Object> read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options = {}) const override;

        bool getFeatures(Features& features) const override;

    protected:
        class Implementation
        {
        public:
            Implementation();

            vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const;
            vsg::ref_ptr<vsg::Object> read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options = {}) const;
            vsg::ref_ptr<vsg::Object> read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options = {}) const;

        private:
            class aiScene;
            using StateCommandPtr = vsg::ref_ptr<vsg::StateCommand>;
            using State = std::pair<StateCommandPtr, StateCommandPtr>;
            using BindState = std::vector<State>;

            vsg::ref_ptr<vsg::GraphicsPipeline> createPipeline(vsg::ref_ptr<vsg::ShaderStage> vs, vsg::ref_ptr<vsg::ShaderStage> fs, vsg::ref_ptr<vsg::DescriptorSetLayout> descriptorSetLayout, bool doubleSided = false, bool enableBlend = false) const;
            void createDefaultPipelineAndState();
            vsg::ref_ptr<vsg::Object> processScene(const aiScene* scene, vsg::ref_ptr<const vsg::Options> options) const;
            BindState processMaterials(const aiScene* scene, vsg::ref_ptr<const vsg::Options> options) const;

            vsg::ref_ptr<vsg::GraphicsPipeline> _defaultPipeline;
            vsg::ref_ptr<vsg::BindDescriptorSet> _defaultState;
            const uint32_t _importFlags;
        };
        std::unique_ptr<Implementation> _implementation;
    };

    /// optional OSG ReaderWriter
    class VSGXCHANGE_DECLSPEC OSG : public vsg::Inherit<vsg::ReaderWriter, OSG>
    {
    public:
        OSG();

        bool readOptions(vsg::Options& options, vsg::CommandLine& arguments) const override;

        vsg::ref_ptr<vsg::Object> read(const vsg::Path&, vsg::ref_ptr<const vsg::Options>) const;

        bool getFeatures(Features& features) const override;

    protected:
        class Implementation
        {
        public:
            Implementation();

            vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const;

            bool readOptions(vsg::Options& options, vsg::CommandLine& arguments) const;

            vsg::ref_ptr<osg2vsg::PipelineCache> pipelineCache;

        protected:
        };
        std::unique_ptr<Implementation> _implementation;
    };

} // namespace vsgXchange

EVSG_type_name(vsgXchange::models);
EVSG_type_name(vsgXchange::assimp);
EVSG_type_name(vsgXchange::OSG);
