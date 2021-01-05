#include <vsg/io/ReaderWriter.h>
#include <vsgXchange/Export.h>

namespace vsgXchange
{
    class VSGXCHANGE_DECLSPEC ReaderWriter_all : public vsg::Inherit<vsg::CompositeReaderWriter, ReaderWriter_all>
    {
    public:
        ReaderWriter_all();
    };
}

EVSG_type_name(vsgXchange::ReaderWriter_all);
