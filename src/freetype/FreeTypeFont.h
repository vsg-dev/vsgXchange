#pragma once

#include <vsg/io/ReaderWriter.h>

#include <vsgXchange/Export.h>

#include <map>
#include <list>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

namespace vsgXchange
{

    inline bool between(float a, float b, float c)
    {
        if (a < c) return (a <= b) && (b <= c);
        else return (c <= b) && (b <= a);
    }

#if 1
    inline bool between2(float a, float b, float c)
    {
        if (a < c) return (a <= b) && (b < c);
        else return (c <= b) && (b < a);
    }
#else
    inline bool between2(float a, float b, float c) { return between(a,b,c); }
#endif

    class VSGXCHANGE_DECLSPEC ReaderWriter_freetype : public vsg::Inherit<vsg::ReaderWriter, ReaderWriter_freetype>
    {
    public:
        ReaderWriter_freetype();

        void init() const;

        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;

    protected:

        virtual ~ReaderWriter_freetype();

        using Contour = std::vector<vsg::vec2>;
        using Contours = std::list<Contour>;

        unsigned char nearerst_edge(const FT_Bitmap& glyph_bitmap, int c, int r, int delta) const;
        vsg::ref_ptr<vsg::Group> createOutlineGeometry(const Contours& contours) const;
        bool generateOutlines(FT_Outline& outline, Contours& contours) const;
        void checkForAndFixDegenerates(Contours& contours) const;
        float nearest_contour_edge(const Contours& local_contours, int r, int c) const;
        bool outside_contours(const Contours& local_contours, int row, int col) const;

        std::map<std::string, std::string> _supportedFormats;
        mutable std::mutex _mutex;
        mutable FT_Library _library = nullptr;
    };

}
