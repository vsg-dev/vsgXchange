#pragma once

#include <vsg/io/ReaderWriter.h>
#include <vsgXchange/Export.h>
#include <vsg/state/DescriptorSetLayout.h>

struct aiScene;

namespace vsg {
class PipelineLayout;
class ShaderStage;
class BindGraphicsPipeline;
}

namespace vsgXchange
{

class VSGXCHANGE_DECLSPEC ReaderWriter_assimp : public vsg::Inherit<vsg::ReaderWriter, ReaderWriter_assimp>
    {
public:
    ReaderWriter_assimp();

    vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;
    vsg::ref_ptr<vsg::Object> read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options = {}) const override;

private:

    using StateCommandPtr = vsg::ref_ptr<vsg::StateCommand>;
    using State = std::pair<StateCommandPtr, StateCommandPtr>;
    using BindState = std::vector<State>;

    vsg::ref_ptr<vsg::GraphicsPipeline> createPipeline(vsg::ref_ptr<vsg::ShaderStage> vs, vsg::ref_ptr<vsg::ShaderStage> fs, vsg::ref_ptr<vsg::DescriptorSetLayout> descriptorSetLayout, bool doubleSided=false, bool enableBlend=false) const;
    void createDefaultPipelineAndState();
    vsg::ref_ptr<vsg::Object> processScene(const aiScene *scene, vsg::ref_ptr<const vsg::Options> options) const;
    BindState processMaterials(const aiScene *scene, vsg::ref_ptr<const vsg::Options> options) const;

    vsg::ref_ptr<vsg::Options> _options;
    vsg::ref_ptr<vsg::GraphicsPipeline> _defaultPipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> _defaultState;
    const uint32_t _importFlags;
};

} // namespace vsgXchange
