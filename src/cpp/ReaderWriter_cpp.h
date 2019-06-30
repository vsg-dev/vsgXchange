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

        bool writeFile(const vsg::Object* object, const vsg::Path& filename) const override;

    protected:

        void write(std::ostream& out, const std::string& str) const;
    };

}
