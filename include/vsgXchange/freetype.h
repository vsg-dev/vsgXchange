#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2021 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/ReaderWriter.h>
#include <vsgXchange/Export.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include <memory>

namespace vsgXchange
{

    class VSGXCHANGE_DECLSPEC freetype : public vsg::Inherit<vsg::ReaderWriter, freetype>
    {
    public:
        freetype();

        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;

        bool getFeatures(Features& features) const override;

    protected:
        class Implementation
        {
        public:
            Implementation();

            void init() const;

            vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const;

            ~Implementation();

            struct Contour
            {
                std::vector<vsg::vec2> points;
                std::vector<vsg::vec3> edges;
            };

            using Contours = std::list<Contour>;

            unsigned char nearerst_edge(const FT_Bitmap& glyph_bitmap, int c, int r, int delta) const;
            vsg::ref_ptr<vsg::Group> createOutlineGeometry(const Contours& contours) const;
            bool generateOutlines(FT_Outline& outline, Contours& contours) const;
            void checkForAndFixDegenerates(Contours& contours) const;
            float nearest_contour_edge(const Contours& local_contours, const vsg::vec2& v) const;
            bool outside_contours(const Contours& local_contours, const vsg::vec2& v) const;

            std::map<std::string, std::string> _supportedFormats;
            mutable std::mutex _mutex;
            mutable FT_Library _library = nullptr;
        };
        std::unique_ptr<Implementation> _implementation;
    };

} // namespace vsgXchange

EVSG_type_name(vsgXchange::freetype);
