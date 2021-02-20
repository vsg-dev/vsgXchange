#pragma once

#include <vsg/io/ReaderWriter.h>
#include <vsgXchange/Export.h>

#include <map>

namespace vsgXchange
{

    class VSGXCHANGE_DECLSPEC spirv : public vsg::Inherit<vsg::ReaderWriter, spirv>
    {
    public:
        spirv();

        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;

        bool write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;

    protected:
    };

} // namespace vsgXchange

EVSG_type_name(vsgXchange::spirv);
