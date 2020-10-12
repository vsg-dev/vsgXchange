#pragma once

#include <vsg/io/ReaderWriter.h>

#include <vsgXchange/Export.h>

#include <map>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace vsgXchange
{

    class VSGXCHANGE_DECLSPEC ReaderWriter_freetype : public vsg::Inherit<vsg::ReaderWriter, ReaderWriter_freetype>
    {
    public:
        ReaderWriter_freetype();

        void init() const;

        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;

    protected:

        virtual ~ReaderWriter_freetype();

        unsigned char nearerst_edge(const FT_Bitmap& glyph_bitmap, int c, int r, int delta) const;

        std::map<std::string, std::string> _supportedFormats;
        mutable std::mutex _mutex;
        mutable FT_Library _library = nullptr;
    };

}
