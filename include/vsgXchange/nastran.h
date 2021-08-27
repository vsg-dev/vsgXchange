#pragma once

#include <vsg/io/ReaderWriter.h>
#include <vsgXchange/Export.h>

#include <map>

namespace vsgXchange
{

    class VSGXCHANGE_DECLSPEC nastran : public vsg::Inherit<vsg::ReaderWriter, nastran>
    {
    public:
        nastran();

        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;

        vsg::ref_ptr<vsg::Object> read(std::istream& /*fin*/, vsg::ref_ptr<const vsg::Options> = {}) const override;

        bool getFeatures(Features& features) const override;
 
    protected:
        //change to true to get parsing information
        const bool enableDebugOutput = false;
    };

} // namespace vsgXchange

EVSG_type_name(vsgXchange::nastran);