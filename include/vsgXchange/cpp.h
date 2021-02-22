#pragma once

#include <vsg/io/ReaderWriter.h>
#include <vsgXchange/Export.h>

#include <map>

namespace vsgXchange
{

    class VSGXCHANGE_DECLSPEC cpp : public vsg::Inherit<vsg::ReaderWriter, cpp>
    {
    public:
        cpp();

        bool write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;

        bool getFeatures(Features& features) const override;

    protected:
        void write(std::ostream& out, const std::string& str) const;
    };

} // namespace vsgXchange

EVSG_type_name(vsgXchange::cpp);
