/* <editor-fold desc="MIT License">

Copyright(c) 2020 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgXchange/freetype.h>

#include <vsg/core/Exception.h>
#include <vsg/io/Logger.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/nodes/Group.h>
#include <vsg/state/ShaderStage.h>
#include <vsg/text/Font.h>
#include <vsg/utils/CommandLine.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include <chrono>
#include <iostream>
#include <set>

namespace vsgXchange
{

    inline bool between_or_equal(float a, float b, float c)
    {
        if (a < c)
            return (a <= b) && (b <= c);
        else
            return (c <= b) && (b <= a);
    }

    inline bool between_not_equal(float a, float b, float c)
    {
        if (a < c)
            return (a <= b) && (b < c);
        else
            return (c <= b) && (b < a);
    }

    class freetype::Implementation
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

        unsigned char nearest_edge(const FT_Bitmap& glyph_bitmap, int c, int r, int delta) const;
        vsg::ref_ptr<vsg::Group> createOutlineGeometry(const Contours& contours) const;
        bool generateOutlines(FT_Outline& outline, Contours& contours) const;
        void checkForAndFixDegenerates(Contours& contours) const;
        void scanConvertLine(const Contours& local_contours, const vsg::vec2& lineStart, std::vector<bool>& row, std::vector<float>& scratchBuffer) const;

        std::map<vsg::Path, std::string> _supportedFormats;
        mutable std::mutex _mutex;
        mutable FT_Library _library = nullptr;
    };

} // namespace vsgXchange

using namespace vsgXchange;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// freetype ReaderWriter facade
//
freetype::freetype() :
    _implementation(new freetype::Implementation())
{
}
freetype::~freetype()
{
    delete _implementation;
};
vsg::ref_ptr<vsg::Object> freetype::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    return _implementation->read(filename, options);
}

bool freetype::getFeatures(Features& features) const
{
    for (auto& ext : _implementation->_supportedFormats)
    {
        features.extensionFeatureMap[ext.first] = static_cast<vsg::ReaderWriter::FeatureMask>(vsg::ReaderWriter::READ_FILENAME);
    }

    // enumerate the supported vsg::Options::setValue(str, value) options
    features.optionNameTypeMap[freetype::texel_margin_ratio] = vsg::type_name<float>();
    features.optionNameTypeMap[freetype::quad_margin_ratio] = vsg::type_name<float>();

    return true;
}

bool freetype::readOptions(vsg::Options& options, vsg::CommandLine& arguments) const
{
    bool result = arguments.readAndAssign<float>(freetype::texel_margin_ratio, &options);
    result = arguments.readAndAssign<float>(freetype::quad_margin_ratio, &options) || result;
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// freetype ReaderWriter Implementation
//
freetype::Implementation::Implementation()
{
    _supportedFormats[".ttf"] = "true type font format";
    _supportedFormats[".ttc"] = "true type collection format";
    _supportedFormats[".pfb"] = "type1 binary format";
    _supportedFormats[".pfa"] = "type2 ascii format";
    _supportedFormats[".cid"] = "Postscript CID-Fonts format";
    _supportedFormats[".cff"] = "OpenType format";
    _supportedFormats[".cef"] = "OpenType format";
    _supportedFormats[".otf"] = "OpenType format";
    _supportedFormats[".fon"] = "Windows bitmap fonts format";
    _supportedFormats[".fnt"] = "Windows bitmap fonts format";
    _supportedFormats[".woff"] = "web open font format";
}

freetype::Implementation::~Implementation()
{
    if (_library)
    {
        // std::cout<<"freetype::Implementation::~freetype::Implementation() FT_Done_FreeType library = "<<_library<<std::endl;
        FT_Done_FreeType(_library);
    }
}

void freetype::Implementation::init() const
{
    if (_library) return;

    int error = FT_Init_FreeType(&_library);
    if (error)
    {
        // std::cout<<"freetype::Implementation::init() failed."<<std::endl;
        return;
        //throw vsg::Exception{"FreeType error occurred during library initialization", error};
    }

    // std::cout<<"freetype::Implementation() FT_Init_FreeType library = "<<_library<<std::endl;
}

unsigned char freetype::Implementation::nearest_edge(const FT_Bitmap& glyph_bitmap, int c, int r, int delta) const
{
    unsigned char value = 0;
    if (c >= 0 && c < static_cast<int>(glyph_bitmap.width) && r >= 0 && r < static_cast<int>(glyph_bitmap.rows))
    {
        value = glyph_bitmap.buffer[c + r * glyph_bitmap.width];
    }

    float distance = 0.0f;
    if (value == 0)
    {
        distance = float(delta);

        int begin_c = (c > delta) ? (c - delta) : 0;
        int end_c = (c + delta + 1) < static_cast<int>(glyph_bitmap.width) ? (c + delta + 1) : glyph_bitmap.width;
        int begin_r = (r > delta) ? (r - delta) : 0;
        int end_r = (r + delta + 1) < static_cast<int>(glyph_bitmap.rows) ? (r + delta + 1) : glyph_bitmap.rows;
        for (int compare_r = begin_r; compare_r < end_r; ++compare_r)
        {
            for (int compare_c = begin_c; compare_c < end_c; ++compare_c)
            {
                unsigned char local_value = glyph_bitmap.buffer[compare_c + compare_r * glyph_bitmap.width];
                if (local_value > 0)
                {
                    float local_distance = sqrt(float((compare_c - c) * (compare_c - c) + (compare_r - r) * (compare_r - r))) - float(local_value) / 255.0f + 0.5f;
                    if (local_distance < distance) distance = local_distance;
                }
            }
        }
        // flip the sign to signify distance outside glyph outline
        distance = -distance;
    }
    else if (value == 255)
    {
        distance = float(delta);

        int begin_c = (c > delta) ? (c - delta) : 0;
        int end_c = (c + delta + 1) < static_cast<int>(glyph_bitmap.width) ? (c + delta + 1) : glyph_bitmap.width;
        int begin_r = (r > delta) ? (r - delta) : 0;
        int end_r = (r + delta + 1) < static_cast<int>(glyph_bitmap.rows) ? (r + delta + 1) : glyph_bitmap.rows;
        for (int compare_r = begin_r; compare_r < end_r; ++compare_r)
        {
            for (int compare_c = begin_c; compare_c < end_c; ++compare_c)
            {
                unsigned char local_value = glyph_bitmap.buffer[compare_c + compare_r * glyph_bitmap.width];
                if (local_value < 255)
                {
                    float local_distance = sqrt(float((compare_c - c) * (compare_c - c) + (compare_r - r) * (compare_r - r))) - float(local_value) / 255.0f + 0.5f;
                    if (local_distance < distance) distance = local_distance;
                }
            }
        }
        // flip the sign to signify distance outside glyph outline
        distance = distance;
    }
    else
    {
        distance = (float(value) / 255.0f - 0.5f);
    }

    if (distance <= -float(delta))
        return 0;
    else if (distance >= float(delta))
        return 255;
    else
    {
        float scaled_distance = distance * (128.0f / float(delta)) + 128.0f;
        if (scaled_distance <= 0.0f) return 0;
        if (scaled_distance >= 255.0f) return 255;
        return static_cast<unsigned char>(scaled_distance);
    }
}

vsg::ref_ptr<vsg::Group> freetype::Implementation::createOutlineGeometry(const Contours& contours) const
{
    auto group = vsg::Group::create();

    for (auto& contour : contours)
    {
        auto& points = contour.points;

        auto geometry = vsg::Geometry::create();
        auto vertices = vsg::vec3Array::create(static_cast<uint32_t>(points.size()));
        geometry->assignArrays({vertices});

        for (size_t i = 0; i < points.size(); ++i)
        {
            vertices->set(i, vsg::vec3(points[i].x, points[i].y, 0.0f));
        }

        geometry->commands.push_back(vsg::Draw::create(static_cast<uint32_t>(points.size()), 0, 0, 0));

        group->addChild(geometry);
    }
    return group;
}

bool freetype::Implementation::generateOutlines(FT_Outline& outline, Contours& in_contours) const
{
    //std::cout<<"charcode = "<<glyphQuad.charcode<<", width = "<<width<<", height = "<<height<<std::endl;
    //std::cout<<"   face->glyph->outline.n_contours = "<<face->glyph->outline.n_contours<<std::endl;
    //std::cout<<"   face->glyph->outline.n_points = "<<face->glyph->outline.n_points<<std::endl;
    auto moveTo = [](const FT_Vector* to, void* user) -> int {
        auto contours = reinterpret_cast<Contours*>(user);
        contours->push_back(Contour());
        auto& contour = contours->back();
        auto& points = contour.points;
        points.emplace_back(float(to->x), float(to->y));
        return 0;
    };

    auto lineTo = [](const FT_Vector* to, void* user) -> int {
        auto contours = reinterpret_cast<Contours*>(user);
        auto& contour = contours->back();
        auto& points = contour.points;

        vsg::vec2 p(float(to->x), float(to->y));
        points.push_back(p);
        return 0;
    };

    auto conicTo = [](const FT_Vector* control, const FT_Vector* to, void* user) -> int {
        auto contours = reinterpret_cast<Contours*>(user);
        auto& contour = contours->back();
        auto& points = contour.points;

        vsg::vec2 p0(points.back());
        vsg::vec2 p1(float(control->x), float(control->y));
        vsg::vec2 p2(float(to->x), float(to->y));

        if (p0 == p1 && p1 == p2)
        {
            // ignore degenerate segment
            //std::cout<<"conicTo error\n";
            return 0;
        }

        int numSteps = 10;

        float dt = 1.0f / float(numSteps);
        float u = dt;
        for (int i = 1; i < numSteps; ++i)
        {
            float w = 1.0f;
            float bs = 1.0f / ((1.0f - u) * (1.0f - u) + 2.0f * (1.0f - u) * u * w + u * u);
            vsg::vec2 p = (p0 * ((1.0f - u) * (1.0f - u)) + p1 * (2.0f * (1.0f - u) * u * w) + p2 * (u * u)) * bs;
            points.push_back(p);
            u += dt;
        }
        points.push_back(p2);

        return 0;
    };

    auto cubicTo = [](const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* user) -> int {
        auto contours = reinterpret_cast<Contours*>(user);
        auto& contour = contours->back();
        auto& points = contour.points;

        vsg::vec2 p0(points.back());
        vsg::vec2 p1(float(control1->x), float(control1->y));
        vsg::vec2 p2(float(control2->x), float(control2->y));
        vsg::vec2 p3(float(to->x), float(to->y));

        if (p0 == p1 && p1 == p2 && p2 == p3)
        {
            // ignore degenerate segment
            // std::cout<<"cubic Error\n";
            return 0;
        }

        int numSteps = 10;

        float cx = 3.0f * (p1.x - p0.x);
        float bx = 3.0f * (p2.x - p1.x) - cx;
        float ax = p3.x - p0.x - cx - bx;
        float cy = 3.0f * (p1.y - p0.y);
        float by = 3.0f * (p2.y - p1.y) - cy;
        float ay = p3.y - p0.y - cy - by;

        float dt = 1.0f / float(numSteps);
        float u = dt;
        for (int i = 1; i < numSteps; ++i)
        {
            vsg::vec2 p(ax * u * u * u + bx * u * u + cx * u + p0.x,
                        ay * u * u * u + by * u * u + cy * u + p0.y);

            points.push_back(p);
            u += dt;
        }

        points.push_back(p3);
        return 0;
    };

    FT_Outline_Funcs funcs;
    funcs.move_to = moveTo;
    funcs.line_to = lineTo;
    funcs.conic_to = conicTo;
    funcs.cubic_to = cubicTo;
    funcs.shift = 0;
    funcs.delta = 0;

    // ** record description
    int error = FT_Outline_Decompose(&outline, &funcs, &in_contours);
    if (error != 0)
    {
        std::cout << "Warning: could not decompose outline." << error << std::endl;
        return false;
    }

    return true;
}

void freetype::Implementation::checkForAndFixDegenerates(Contours& contours) const
{
    auto contains_degenerates = [](Contour& contour) -> bool {
        auto& points = contour.points;
        for (size_t i = 0; i < points.size() - 1; ++i)
        {
            auto& p0 = points[i];
            auto& p1 = points[i + 1];
            if (p0 == p1)
            {
                return true;
            }
        }

        // check if contour is closed.
        if (points.front() != points.back()) return true;

        return false;
    };

    auto fix_degenerates = [](Contour& contour) -> void {
        if (contour.points.size() < 2) return;

        auto& points = contour.points;

        decltype(Contour::points) clean_points;

        clean_points.push_back(points[0]);

        for (size_t i = 0; i < points.size() - 1; ++i)
        {
            auto& p0 = points[i];
            auto& p1 = points[i + 1];
            if (p0 != p1)
            {
                clean_points.push_back(p1);
            }
        }

        if (clean_points.front() != clean_points.back())
        {
            // make sure the contour is closed (last point equals first point.)
            clean_points.back() = clean_points.front();
        }

        points.swap(clean_points);
    };

    // fix all contours
    for (auto& contour : contours)
    {
        if (contains_degenerates(contour))
        {
            fix_degenerates(contour);
        }
    }
}

void freetype::Implementation::scanConvertLine(const Contours& local_contours, const vsg::vec2& lineStart, std::vector<bool>& row, std::vector<float>& scratchBuffer) const
{
    scratchBuffer.clear();

    float lineEndX = lineStart.x + row.size() - 1;

    for (auto& contour : local_contours)
    {
        auto& points = contour.points;
        if (points.empty())
            continue;
        bool fromBelow = false;
        if (points[0].y == lineStart.y)
        {
            for (auto itr = points.rbegin(); itr != points.rend(); ++itr)
            {
                if (itr->y != lineStart.y)
                {
                    fromBelow = itr->y < lineStart.y;
                    break;
                }
            }
        }
        for (std::size_t i = 0; i < points.size() - 1; ++i)
        {
            auto& p0 = points[i];
            auto& p1 = points[i + 1];

            if (p0.y == p1.y)
                continue;

            if ((p0.y < lineStart.y && p1.y < lineStart.y) || (p0.y > lineStart.y && p1.y > lineStart.y))
                continue;

            if (p1.y == lineStart.y)
            {
                scratchBuffer.push_back(p1.x);
                fromBelow = p0.y < lineStart.y;
            }
            else if (p0.y == lineStart.y)
            {
                if (fromBelow == p1.y < lineStart.y)
                    scratchBuffer.push_back(p0.x);
            }
            else
            {
                float intersection = p0.x + (lineStart.y - p0.y) * (p1.x - p0.x) / (p1.y - p0.y);
                scratchBuffer.push_back(intersection);
            }
        }
    }

    std::sort(scratchBuffer.begin(), scratchBuffer.end());
    scratchBuffer.push_back(std::numeric_limits<float>::infinity());

    auto itr = scratchBuffer.begin();
    bool in = false;
    for (std::size_t i = 0; i < row.size(); ++i)
    {
        float pos = lineStart.x + i;
        while (*itr <= pos)
        {
            in = !in;
            ++itr;
        }
        row[i] = in;
    }
}

vsg::ref_ptr<vsg::Object> freetype::Implementation::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    vsg::Path ext = (options && options->extensionHint) ? options->extensionHint : vsg::lowerCaseFileExtension(filename);
    if (_supportedFormats.find(ext) == _supportedFormats.end()) return {};

    vsg::Path filenameToUse = findFile(filename, options);
    if (!filenameToUse) return {};

    std::scoped_lock<std::mutex> lock(_mutex);

    init();
    if (!_library) return {};

    FT_Face face;
    FT_Long face_index = 0;

    // Windows workaround for no wchar_t support in Freetype, convert vsg::Path's std::wstring to UTF8 std::string
    std::string filenameToUse_string = filenameToUse.string();
    int error = FT_New_Face(_library, filenameToUse_string.c_str(), face_index, &face);
    if (error == FT_Err_Unknown_File_Format)
    {
        std::cout << "Warning: FreeType unable to read font file : " << filenameToUse << ", error = " << FT_Err_Unknown_File_Format << std::endl;
        return {};
    }
    else if (error)
    {
        std::cout << "Warning: FreeType unable to read font file : " << filenameToUse << ", error = " << error << std::endl;
        return {};
    }

    FT_UInt pixel_size = 48;
    FT_UInt freetype_pixel_size = pixel_size;
    float freetype_pixel_size_scale = float(pixel_size) / (64.0f * float(freetype_pixel_size));

    {
        error = FT_Set_Pixel_Sizes(face, freetype_pixel_size, freetype_pixel_size);
    }

    FT_Int32 load_flags = FT_LOAD_NO_BITMAP;
    FT_Render_Mode render_mode = FT_RENDER_MODE_NORMAL;

    struct GlyphQuad
    {
        FT_ULong charcode;
        FT_ULong glyph_index;
        unsigned int width;
        unsigned int height;
        inline bool operator<(const GlyphQuad& rhs) const { return height < rhs.height; }
    };

    std::multiset<GlyphQuad> sortedGlyphQuads;

    bool hasSpace = false;

    // collect all the sizes of the glyphs
    FT_ULong max_charcode = 0;
    {
        FT_ULong charcode;
        FT_UInt glyph_index;

        charcode = FT_Get_First_Char(face, &glyph_index);
        while (glyph_index != 0)
        {
            charcode = FT_Get_Next_Char(face, charcode, &glyph_index);

            error = FT_Load_Glyph(face, glyph_index, load_flags);
            if (error) continue;

            if (charcode > max_charcode) max_charcode = charcode;

            GlyphQuad quad{
                charcode,
                glyph_index,
                static_cast<unsigned int>(ceil(float(face->glyph->metrics.width) * freetype_pixel_size_scale)),
                static_cast<unsigned int>(ceil(float(face->glyph->metrics.height) * freetype_pixel_size_scale))};

            if (charcode == 32) hasSpace = true;

            sortedGlyphQuads.insert(quad);
        }
    }

    if (!hasSpace)
    {
        FT_ULong charcode = 32;
        FT_UInt glyph_index = FT_Get_Char_Index(face, charcode);

        if (glyph_index != 0)
        {
            error = FT_Load_Glyph(face, glyph_index, load_flags);

            if (!error)
            {
                GlyphQuad quad{
                    charcode,
                    glyph_index,
                    static_cast<unsigned int>(ceil(float(face->glyph->metrics.width) * freetype_pixel_size_scale)),
                    static_cast<unsigned int>(ceil(float(face->glyph->metrics.height) * freetype_pixel_size_scale))};

                sortedGlyphQuads.insert(quad);

                hasSpace = true;
            }
        }
    }

    double total_width = 0.0;
    double total_height = 0.0;
    for (auto& glyph : sortedGlyphQuads)
    {
        total_width += double(glyph.width);
        total_height += double(glyph.height);
    }

    unsigned int average_width = static_cast<unsigned int>(ceil(total_width / double(sortedGlyphQuads.size())));

    auto texel_margin = static_cast<unsigned int>(static_cast<float>(pixel_size) * vsg::value<float>(0.25f, freetype::texel_margin_ratio, options));
    auto quad_margin = static_cast<unsigned int>(static_cast<float>(pixel_size) * vsg::value<float>(0.125f, freetype::quad_margin_ratio, options));

    vsg::debug("texel_margin = ", texel_margin);
    vsg::debug("quad_margin = ", quad_margin);

    unsigned int provisional_cells_across = static_cast<unsigned int>(ceil(sqrt(double(face->num_glyphs))));
    unsigned int provisional_width = provisional_cells_across * (average_width + texel_margin);

    //provisional_width = 1024;

    unsigned int xpos = texel_margin;
    unsigned int ypos = texel_margin;
    unsigned int xtop = 2 * texel_margin;
    unsigned int ytop = 2 * texel_margin;
    for (auto& glyphQuad : sortedGlyphQuads)
    {
        unsigned int width = glyphQuad.width;
        unsigned int height = glyphQuad.height;

        if ((xpos + width + texel_margin) > provisional_width)
        {
            // glyph doesn't fit in present row so shift to next row.
            xpos = texel_margin;
            ypos = ytop;
        }

        unsigned int local_ytop = ypos + height + texel_margin;
        if (local_ytop > ytop) ytop = local_ytop;

        xpos += (width + texel_margin);
        if (xpos > xtop) xtop = xpos;
    }

    //std::cout<<"provisional_width = "<<provisional_width<<", xtop = "<<xtop<<", ytop = "<<ytop<<std::endl;
    //xtop = provisional_width;

#if 0
    auto atlas = vsg::byteArray2D::create(xtop, ytop, vsg::Data::Layout{VK_FORMAT_R8_SNORM});
    using sdf_type = vsg::byteArray2D::value_type;
    float min_value = std::numeric_limits<sdf_type>::lowest();
    float max_value = std::numeric_limits<sdf_type>::max();
    float mid_value = 0.0f;
#else
    auto atlas = vsg::shortArray2D::create(xtop, ytop, vsg::Data::Properties{VK_FORMAT_R16_SNORM});
    using sdf_type = vsg::shortArray2D::value_type;
    float min_value = std::numeric_limits<sdf_type>::lowest();
    float max_value = std::numeric_limits<sdf_type>::max();
    float mid_value = 0.0f;
#endif

    // initialize to zeros
    for (auto& c : *atlas) c = static_cast<sdf_type>(min_value);

    auto font = vsg::Font::create();
    font->atlas = atlas;

    xpos = texel_margin;
    ypos = texel_margin;
    ytop = 0;

    bool useOutline = true;
    bool computeSDF = true;

    double total_nearest_edge = 0.0;
    double total_outside_edge = 0.0;

    auto glyphMetrics = vsg::GlyphMetricsArray::create(static_cast<uint32_t>(sortedGlyphQuads.size() + 1));
    auto charmap = vsg::uintArray::create(max_charcode + 1);
    uint32_t destination_glyphindex = 0;

    // first entry of glyphMetrics should be a null entry
    vsg::GlyphMetrics null_metrics;
    null_metrics.uvrect.set(0.0f, 0.0f, 0.0f, 0.0f);
    null_metrics.width = 0.0f;
    null_metrics.height = 0.0f;
    null_metrics.horiBearingX = 0.0f;
    null_metrics.horiBearingY = 0.0f;
    null_metrics.horiAdvance = 0.0f;
    null_metrics.vertBearingX = 0.0f;
    null_metrics.vertBearingY = 0.0f;
    null_metrics.vertAdvance = 0.0f;
    glyphMetrics->set(destination_glyphindex++, null_metrics);

    // initialize charmap to zeros.
    for (auto& c : *charmap) c = 0;

    std::vector<bool> scanlineBuffer;
    std::vector<float> scanlineScratchBuffer;
    for (auto& glyphQuad : sortedGlyphQuads)
    {
        error = FT_Load_Glyph(face, glyphQuad.glyph_index, load_flags);
        if (error) continue;

        unsigned int width = glyphQuad.width;
        unsigned int height = glyphQuad.height;
        auto metrics = face->glyph->metrics;

        if ((xpos + width + texel_margin) > atlas->width())
        {
            // glyph doesn't fit in present row so shift to next row.
            xpos = texel_margin;
            ypos = ytop;
        }

        if (useOutline)
        {
            Contours contours;
            generateOutlines(face->glyph->outline, contours);

            // scale and offset the outline geometry
            vsg::vec2 offset(float(metrics.horiBearingX) * freetype_pixel_size_scale, float(metrics.horiBearingY) * freetype_pixel_size_scale);
            for (auto& contour : contours)
            {
                for (auto& v : contour.points)
                {
                    // scale and translate to local origin
                    v.x = v.x * freetype_pixel_size_scale - offset.x;
                    v.y = offset.y - v.y * freetype_pixel_size_scale;
                }
            }

            // fix any degenerate segments
            checkForAndFixDegenerates(contours);

            // font->setObject(vsg::make_string(glyphQuad.glyph_index), createOutlineGeometry(contours));

            struct Extents
            {
                float min_x = std::numeric_limits<float>::max();
                float max_x = std::numeric_limits<float>::lowest();
                float min_y = std::numeric_limits<float>::max();
                float max_y = std::numeric_limits<float>::lowest();

                void add(const vsg::vec2& v)
                {
                    if (v.x < min_x) min_x = v.x;
                    if (v.y < min_y) min_y = v.y;
                    if (v.x > max_x) max_x = v.x;
                    if (v.y > max_y) max_y = v.y;
                }

                bool contains(const vsg::vec2& v) const
                {
                    return v.x >= min_x && v.x <= max_x &&
                           v.y >= min_y && v.y <= max_y;
                }
            } extents;

            // compute edges and bounding volume
            for (auto& contour : contours)
            {
                auto& points = contour.points;
                for (auto& v : points)
                {
                    extents.add(v);
                }

                auto& edges = contour.edges;
                edges.resize(points.size() - 1);
                for (size_t i = 0; i < edges.size(); ++i)
                {
                    vsg::vec2 dv = points[i + 1] - points[i];
                    float len = vsg::length(dv);
                    dv /= len;
                    edges[i].set(dv.x, dv.y, len);
                }
            }

            if (!contours.empty())
            {
                float scale = 2.0f / float(pixel_size);
                float invScale = float(pixel_size) / 2.0f;
                int delta = quad_margin - 2;
                float stripLength = invScale * (max_value - mid_value) / (max_value - min_value);
                Contours edgeStrip;
                auto& stripContour = edgeStrip.emplace_back();
                stripContour.points.reserve(5);

                // edges
                for (const auto& contour : contours)
                {
                    for (std::size_t i = 0; i < contour.points.size() - 1; ++i)
                    {
                        const auto& start = contour.points[i];
                        const auto& end = contour.points[i + 1];

                        auto side = end - start;
                        auto edgeLength2 = vsg::length2(side);
                        std::swap(side.x, side.y);
                        side.y = -side.y;
                        side = vsg::normalize(side) * stripLength;

                        stripContour.points.clear();
                        stripContour.points.push_back(start + side);
                        stripContour.points.push_back(end + side);
                        stripContour.points.push_back(end - side);
                        stripContour.points.push_back(start - side);

                        vsg::vec2 min{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
                        vsg::vec2 max{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
                        for (const auto& point : stripContour.points)
                        {
                            min.x = std::min(min.x, point.x);
                            min.y = std::min(min.y, point.y);
                            max.x = std::max(max.x, point.x);
                            max.y = std::max(max.y, point.y);
                        }

                        stripContour.points.push_back(stripContour.points.front());

                        int minR = std::max(-delta, static_cast<int>(std::floor(min.y)));
                        int maxR = std::min(static_cast<int>(height + delta), static_cast<int>(std::ceil(max.y)));
                        int minC = std::max(-delta, static_cast<int>(std::floor(min.x)));
                        int maxC = std::min(static_cast<int>(width + delta), static_cast<int>(std::ceil(max.x)));
                        scanlineBuffer.resize(maxC - minC);
                        for (int r = minR; r < maxR; ++r)
                        {
                            vsg::vec2 rowStart{float(minC), float(r)};
                            scanConvertLine(edgeStrip, rowStart, scanlineBuffer, scanlineScratchBuffer);
                            for (int i = 0; i < scanlineBuffer.size(); ++i)
                            {
                                if (scanlineBuffer[i])
                                {
                                    vsg::vec2 v{float(minC + i), float(r)};
                                    float numerator = (end.y - start.y) * v.x - (end.x - start.x) * v.y + end.x * start.y - end.y * start.x;
                                    float dist2 = numerator * numerator / edgeLength2;
                                    sdf_type& texel = atlas->at(xpos + minC + i, ypos + r);
                                    float existing = invScale * (texel - mid_value) / (max_value - min_value);
                                    existing *= existing;
                                    if (dist2 < existing)
                                    {
                                        float distance_ratio = -std::sqrt(dist2) * scale;
                                        float value = mid_value + distance_ratio * (max_value - min_value);

                                        if (value <= min_value)
                                            texel = static_cast<sdf_type>(min_value);
                                        else if (value >= max_value)
                                            texel = static_cast<sdf_type>(max_value);
                                        else
                                            texel = static_cast<sdf_type>(value);
                                    }
                                }
                            }
                        }
                    }
                }

                // vertices
                for (const auto& contour : contours)
                {
                    for (std::size_t i = 0; i < contour.points.size() - 1; ++i)
                    {
                        const auto& point = contour.points[i];
                        const auto& previous = contour.points[i == 0 ? (contour.points.size() - 2) : (i - 1)];
                        const auto& next = contour.points[i + 1];

                        auto prevNormal = point - previous;
                        std::swap(prevNormal.x, prevNormal.y);
                        prevNormal.y = -prevNormal.y;
                        prevNormal = vsg::normalize(prevNormal) * stripLength;
                        if (!std::signbit(vsg::dot(prevNormal, next - point)))
                            prevNormal = -prevNormal;

                        auto nextNormal = next - point;
                        std::swap(nextNormal.x, nextNormal.y);
                        nextNormal.y = -nextNormal.y;
                        nextNormal = vsg::normalize(nextNormal) * stripLength;
                        if (std::signbit(vsg::dot(nextNormal, point - previous)))
                            nextNormal = -nextNormal;

                        auto prev2 = point + prevNormal;
                        auto prevDir = point - previous;
                        auto next2 = point + nextNormal;
                        auto nextDir = next - point;

                        auto t = (prev2.x - next2.x) * -nextDir.y - (prev2.y - next2.y) * -nextDir.x;
                        auto denom = prevDir.x * nextDir.y - prevDir.y * nextDir.x;
                        if (denom == 0)
                            continue;
                        t /= denom;

                        stripContour.points.clear();
                        // shift this a little to avoid numerical stability issues
                        stripContour.points.push_back(point - (prevNormal + nextNormal) * 0.015625f);
                        stripContour.points.push_back(prev2);
                        stripContour.points.push_back(prev2 + prevDir * t);
                        stripContour.points.push_back(next2);

                        vsg::vec2 min{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
                        vsg::vec2 max{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
                        for (const auto& point : stripContour.points)
                        {
                            min.x = std::min(min.x, point.x);
                            min.y = std::min(min.y, point.y);
                            max.x = std::max(max.x, point.x);
                            max.y = std::max(max.y, point.y);
                        }

                        stripContour.points.push_back(stripContour.points.front());

                        int minR = std::max(-delta, static_cast<int>(std::floor(min.y)));
                        int maxR = std::min(static_cast<int>(height + delta), static_cast<int>(std::ceil(max.y)));
                        int minC = std::max(-delta, static_cast<int>(std::floor(min.x)));
                        int maxC = std::min(static_cast<int>(width + delta), static_cast<int>(std::ceil(max.x)));
                        scanlineBuffer.resize(maxC - minC);
                        for (int r = minR; r < maxR; ++r)
                        {
                            vsg::vec2 rowStart{float(minC), float(r)};
                            scanConvertLine(edgeStrip, rowStart, scanlineBuffer, scanlineScratchBuffer);
                            for (int i = 0; i < scanlineBuffer.size(); ++i)
                            {
                                if (scanlineBuffer[i])
                                {
                                    vsg::vec2 v{float(minC + i), float(r)};
                                    float dist2 = vsg::length2(v - point);
                                    sdf_type& texel = atlas->at(xpos + minC + i, ypos + r);
                                    float existing = invScale * (texel - mid_value) / (max_value - min_value);
                                    existing *= existing;
                                    if (dist2 < existing)
                                    {
                                        float distance_ratio = -std::sqrt(dist2) * scale;
                                        float value = mid_value + distance_ratio * (max_value - min_value);

                                        if (value <= min_value)
                                            texel = static_cast<sdf_type>(min_value);
                                        else if (value >= max_value)
                                            texel = static_cast<sdf_type>(max_value);
                                        else
                                            texel = static_cast<sdf_type>(value);
                                    }
                                }
                            }
                        }
                    }
                }

                scanlineBuffer.resize(width + 2 * delta);
                for (int r = -delta; r < static_cast<int>(height + delta); ++r)
                {
                    std::size_t index = atlas->index(xpos - delta, ypos + r);
                    vsg::vec2 rowStart{float(-delta), float(r)};
                    scanConvertLine(contours, rowStart, scanlineBuffer, scanlineScratchBuffer);
                    for (int i = 0; i < scanlineBuffer.size(); ++i)
                    {
                        if (scanlineBuffer[i])
                        {
                            sdf_type& texel = atlas->at(index);
                            texel = static_cast<sdf_type>(std::clamp(2 * mid_value - texel, min_value, max_value));
                        }
                        ++index;
                    }
                }
            }
        }
        else
        {
            if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP)
            {
                error = FT_Render_Glyph(face->glyph, render_mode);
                if (error) continue;
            }

            if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP) continue;

            const FT_Bitmap& bitmap = face->glyph->bitmap;

            // copy pixels
            if (computeSDF)
            {
                int delta = quad_margin - 2;
                for (int r = -delta; r < static_cast<int>(bitmap.rows + delta); ++r)
                {
                    std::size_t index = atlas->index(xpos - delta, ypos + r);
                    for (int c = -delta; c < static_cast<int>(bitmap.width + delta); ++c)
                    {
                        atlas->at(index++) = nearest_edge(bitmap, c, r, quad_margin);
                    }
                }
            }
            else
            {
                const unsigned char* ptr = bitmap.buffer;
                for (unsigned int r = 0; r < bitmap.rows; ++r)
                {
                    std::size_t index = atlas->index(xpos, ypos + r);
                    for (unsigned int c = 0; c < bitmap.width; ++c)
                    {
                        atlas->at(index++) = *ptr++;
                    }
                }
            }
        }

        vsg::vec4 uvrect(
            (float(xpos - quad_margin) - 1.0f) / float(atlas->width() - 1), float(ypos + height + quad_margin) / float(atlas->height() - 1),
            float(xpos + width + quad_margin) / float(atlas->width() - 1), float((ypos - quad_margin) - 1.0f) / float(atlas->height() - 1));

        vsg::GlyphMetrics vsg_metrics;
        vsg_metrics.uvrect = uvrect;
        vsg_metrics.width = float(width + 2 * quad_margin) / float(pixel_size);
        vsg_metrics.height = float(height + 2 * quad_margin) / float(pixel_size);
        vsg_metrics.horiBearingX = (float(metrics.horiBearingX) * freetype_pixel_size_scale - float(quad_margin)) / float(pixel_size);
        vsg_metrics.horiBearingY = (float(metrics.horiBearingY) * freetype_pixel_size_scale + float(quad_margin)) / float(pixel_size);
        vsg_metrics.horiAdvance = (float(metrics.horiAdvance) * freetype_pixel_size_scale) / float(pixel_size);
        vsg_metrics.vertBearingX = (float(metrics.vertBearingX) * freetype_pixel_size_scale - float(quad_margin)) / float(pixel_size);
        vsg_metrics.vertBearingY = (float(metrics.vertBearingY) * freetype_pixel_size_scale + float(quad_margin)) / float(pixel_size);
        vsg_metrics.vertAdvance = (float(metrics.vertAdvance) * freetype_pixel_size_scale) / float(pixel_size);

        // assign the glyph metrics and charcode/glyph_index to the VSG glyphMetrics and charmap containers.
        glyphMetrics->set(destination_glyphindex, vsg_metrics);
        charmap->set(glyphQuad.charcode, destination_glyphindex);

        ++destination_glyphindex;

        unsigned int local_ytop = ypos + height + texel_margin;
        if (local_ytop > ytop) ytop = local_ytop;

        xpos += (width + texel_margin);
        if (xpos > xtop) xtop = xpos;
    }

    font->ascender = float(face->ascender) * freetype_pixel_size_scale / float(pixel_size);
    font->descender = float(face->descender) * freetype_pixel_size_scale / float(pixel_size);
    font->height = float(face->height) * freetype_pixel_size_scale / float(pixel_size);
    font->glyphMetrics = glyphMetrics;
    font->charmap = charmap;

    return font;
}
