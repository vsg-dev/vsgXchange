#pragma once

#include <vsg/io/ReaderWriter.h>
#include <vsgXchange/Export.h>

#include <map>

namespace vsgXchange
{

    class VSGXCHANGE_DECLSPEC ReaderWriter_spirv : public vsg::Inherit<vsg::ReaderWriter, ReaderWriter_spirv>
    {
    public:
        ReaderWriter_spirv();

        vsg::ref_ptr<vsg::Object> readFile(const vsg::Path& filename) const override;

        bool writeFile(const vsg::Object* object, const vsg::Path& filename) const override;

    protected:
    };

}
