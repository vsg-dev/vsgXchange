#pragma once

#include <vsg/io/ReaderWriter.h>
#include <vsgXchange/Export.h>
#include <unordered_set>

namespace vsgXchange
{

    class VSGXCHANGE_DECLSPEC ReaderWriter_ktx : public vsg::Inherit<vsg::ReaderWriter, ReaderWriter_ktx>
    {
    public:
        ReaderWriter_ktx();

        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;
        vsg::ref_ptr<vsg::Object> read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options = {}) const override;

    private:
        std::unordered_set<std::string> _supportedExtensions;
    };

} // namespace vsgXchange