#pragma once

#include <vsg/io/ReaderWriter.h>
#include <vsgXchange/Export.h>

#include <map>

namespace osg2vsg
{
    struct PipelineCache;
}

namespace vsgXchange
{

    class VSGXCHANGE_DECLSPEC ReaderWriter_osg : public vsg::Inherit<vsg::ReaderWriter, ReaderWriter_osg>
    {
    public:
        ReaderWriter_osg();

        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;

        bool write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;

        bool readOptions(vsg::Options& options, vsg::CommandLine& arguments) const override;

        vsg::ref_ptr<osg2vsg::PipelineCache> pipelineCache;

    protected:
    };
    //VSG_type_name(vsgXchange::ReaderWriter_osg);

}
