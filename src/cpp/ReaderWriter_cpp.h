#pragma once

#include <vsg/io/ReaderWriter.h>
#include <vsgXchange/Export.h>

#include <map>

namespace vsgXchange
{

    class VSGXCHANGE_DECLSPEC ReaderWriter_cpp : public vsg::Inherit<vsg::ReaderWriter, ReaderWriter_cpp>
    {
    public:
        ReaderWriter_cpp();

        bool write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;

    protected:

        void write(std::ostream& out, const std::string& str) const;
    };

}
