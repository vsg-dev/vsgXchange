#pragma once

#include <vsg/io/ReaderWriter.h>
#include <vsgXchange/Export.h>

#include <memory>

namespace vsgXchange
{

    class VSGXCHANGE_DECLSPEC freetype : public vsg::Inherit<vsg::ReaderWriter, freetype>
    {
    public:
        freetype();

        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;

    protected:

        class Implementation;

        std::unique_ptr<Implementation> _implementation;
    };

} // namespace vsgXchange

EVSG_type_name(vsgXchange::freetype);
