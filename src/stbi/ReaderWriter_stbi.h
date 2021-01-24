#pragma once

#include <unordered_set>
#include <vsg/io/ReaderWriter.h>
#include <vsgXchange/Export.h>

namespace vsgXchange
{

    class VSGXCHANGE_DECLSPEC ReaderWriter_stbi : public vsg::Inherit<vsg::ReaderWriter, ReaderWriter_stbi>
    {
    public:
        ReaderWriter_stbi();

        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;
        vsg::ref_ptr<vsg::Object> read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options = {}) const override;

    private:
        std::unordered_set<std::string> _supportedExtensions;
    };

} // namespace vsgXchange
