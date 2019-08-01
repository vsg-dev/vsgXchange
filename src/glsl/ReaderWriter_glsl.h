#pragma once

#include <vsg/io/ReaderWriter.h>
#include <vsgXchange/Export.h>

#include <map>

namespace vsgXchange
{

    class VSGXCHANGE_DECLSPEC ReaderWriter_glsl : public vsg::Inherit<vsg::ReaderWriter, ReaderWriter_glsl>
    {
    public:
        ReaderWriter_glsl();

        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;

        bool write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;

        void add(const std::string ext, VkShaderStageFlagBits stage)
        {
            extensionToStage[ext] = stage;
            stageToExtension[stage] = ext;
        }

    protected:
        std::map<std::string, VkShaderStageFlagBits> extensionToStage;
        std::map<VkShaderStageFlagBits, std::string> stageToExtension;
    };
    //VSG_type_name(vsgXchange::ReaderWriter_glsl);

}
