#include "FreeTypeFont.h"

#include <vsg/core/Exception.h>
#include <vsg/nodes/Group.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/state/ShaderStage.h>
#include <vsg/text/Font.h>


using namespace vsgXchange;

#include <iostream>
#include <set>
#include <chrono>

ReaderWriter_freetype::ReaderWriter_freetype()
{
    _supportedFormats[ "ttf"] = "true type font format";
    _supportedFormats[ "ttc"] = "true type collection format";
    _supportedFormats[ "pfb"] = "type1 binary format";
    _supportedFormats[ "pfa"] = "type2 ascii format";
    _supportedFormats[ "cid"] = "Postscript CID-Fonts format";
    _supportedFormats[ "cff"] = "OpenType format";
    _supportedFormats[ "cef"] = "OpenType format";
    _supportedFormats[ "otf"] = "OpenType format";
    _supportedFormats[ "fon"] = "Windows bitmap fonts format";
    _supportedFormats[ "fnt"] = "Windows bitmap fonts format";
    _supportedFormats[ "woff"] = "web open font format";
}

ReaderWriter_freetype::~ReaderWriter_freetype()
{
    if (_library)
    {
        // std::cout<<"ReaderWriter_freetype::~ReaderWriter_freetype() FT_Done_FreeType library = "<<_library<<std::endl;
        FT_Done_FreeType(_library);
    }
}

void ReaderWriter_freetype::init() const
{
    if (_library) return;

    int error = FT_Init_FreeType( &_library );
    if (error)
    {
        // std::cout<<"ReaderWriter_freetype::init() failed."<<std::endl;
        return;
        //throw vsg::Exception{"FreeType an error occurred during library initialization", error};
    }

    // std::cout<<"ReaderWriter_freetype() FT_Init_FreeType library = "<<_library<<std::endl;
}

unsigned char ReaderWriter_freetype::nearerst_edge(const FT_Bitmap& glyph_bitmap, int c, int r, int delta) const
{
    unsigned char value = 0;
    if (c>=0 && c<static_cast<int>(glyph_bitmap.width) && r>=0 && r<static_cast<int>(glyph_bitmap.rows))
    {
        value = glyph_bitmap.buffer[c + r * glyph_bitmap.width];
    }

    float distance = 0.0f;
    if (value==0)
    {
        distance = float(delta);

        int begin_c = (c>delta) ? (c-delta) : 0;
        int end_c = (c+delta+1)<static_cast<int>(glyph_bitmap.width) ? (c+delta+1) : glyph_bitmap.width;
        int begin_r = (r>delta) ? (r-delta) : 0;
        int end_r = (r+delta+1)<static_cast<int>(glyph_bitmap.rows) ? (r+delta+1) : glyph_bitmap.rows;
        for(int compare_r = begin_r; compare_r<end_r; ++compare_r)
        {
            for(int compare_c = begin_c; compare_c<end_c; ++compare_c)
            {
                unsigned char local_value = glyph_bitmap.buffer[compare_c + compare_r * glyph_bitmap.width];
                if (local_value>0)
                {
                    float local_distance = sqrt(float((compare_c-c)*(compare_c-c) + (compare_r-r)*(compare_r-r))) - float(local_value)/255.0f + 0.5f;
                    if (local_distance<distance) distance = local_distance;
                }
            }
        }
        // flip the sign to signify distance outside glyph outline
        distance = -distance;
    }
    else if (value==255)
    {
        distance = float(delta);

        int begin_c = (c>delta) ? (c-delta) : 0;
        int end_c = (c+delta+1)<static_cast<int>(glyph_bitmap.width) ? (c+delta+1) : glyph_bitmap.width;
        int begin_r = (r>delta) ? (r-delta) : 0;
        int end_r = (r+delta+1)<static_cast<int>(glyph_bitmap.rows) ? (r+delta+1) : glyph_bitmap.rows;
        for(int compare_r = begin_r; compare_r<end_r; ++compare_r)
        {
            for(int compare_c = begin_c; compare_c<end_c; ++compare_c)
            {
                unsigned char local_value = glyph_bitmap.buffer[compare_c + compare_r * glyph_bitmap.width];
                if (local_value<255)
                {
                    float local_distance = sqrt(float((compare_c-c)*(compare_c-c) + (compare_r-r)*(compare_r-r))) - float(local_value)/255.0f + 0.5f;
                    if (local_distance<distance) distance = local_distance;
                }
            }
        }
        // flip the sign to signify distance outside glyph outline
        distance = distance;
    }
    else
    {
        distance = (float(value)/255.0f - 0.5f);
    }

    if (distance <= -float(delta)) return 0;
    else if (distance >= float(delta)) return 255;
    else
    {
        float scaled_distance = distance*(128.0f/float(delta)) + 128.0f;
        if (scaled_distance <= 0.0f) return 0;
        if (scaled_distance >= 255.0f) return 255;
        return static_cast<unsigned char>(scaled_distance);
    }
}

vsg::ref_ptr<vsg::Group> ReaderWriter_freetype::createOutlineGeometry(const Contours& contours) const
{
    auto group = vsg::Group::create();

    for(auto& contour : contours)
    {
        auto& points = contour.points;

        auto geometry = vsg::Geometry::create();
        auto vertices = vsg::vec3Array::create(points.size());
        geometry->arrays.push_back(vertices);

        for(size_t i=0; i<points.size(); ++i)
        {
            vertices->set(i, vsg::vec3(points[i].x, points[i].y, 0.0f));
        }

        geometry->commands.push_back(vsg::Draw::create(points.size(), 0, 0, 0));

        group->addChild(geometry);
    }
    return group;
}

bool ReaderWriter_freetype::generateOutlines(FT_Outline& outline, Contours& in_contours) const
{
    //std::cout<<"charcode = "<<glyphQuad.charcode<<", width = "<<width<<", height = "<<height<<std::endl;
    //std::cout<<"   face->glyph->outline.n_contours = "<<face->glyph->outline.n_contours<<std::endl;
    //std::cout<<"   face->glyph->outline.n_points = "<<face->glyph->outline.n_points<<std::endl;
    auto moveTo = [] ( const FT_Vector* to, void* user ) -> int
    {
        auto contours = reinterpret_cast<Contours*>(user);
        contours->push_back(Contour());
        auto& contour = contours->back();
        auto& points = contour.points;
        points.emplace_back(float(to->x), float(to->y));
        return 0;
    };

    auto lineTo = [] ( const FT_Vector* to, void* user ) -> int
    {
        auto contours = reinterpret_cast<Contours*>(user);
        auto& contour = contours->back();
        auto& points = contour.points;

        vsg::vec2 p(float(to->x), float(to->y));
        points.push_back(p);
        return 0;
    };

    auto conicTo = [] ( const FT_Vector* control, const FT_Vector* to, void* user ) -> int
    {
        auto contours = reinterpret_cast<Contours*>(user);
        auto& contour = contours->back();
        auto& points = contour.points;

        vsg::vec2 p0(points.back());
        vsg::vec2 p1(float(control->x), float(control->y));
        vsg::vec2 p2(float(to->x), float(to->y));

        if (p0==p1 && p1==p2)
        {
            // ignore degenate segment
            //std::cout<<"conicTo error\n";
            return 0;
        }

        int numSteps = 10;

        float dt = 1.0/float(numSteps);
        float u = dt;
        for (int i=1; i<numSteps; ++i)
        {
            float w = 1.0f;
            float bs = 1.0f/( (1.0f-u)*(1.0f-u)+2.0f*(1.0f-u)*u*w +u*u );
            vsg::vec2 p = (p0*((1.0f-u)*(1.0f-u)) + p1*(2.0f*(1.0f-u)*u*w) + p2*(u*u))*bs;
            points.push_back( p );
            u += dt;
        }
        points.push_back( p2 );

        return 0;
    };

    auto cubicTo = [] ( const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* user ) -> int
    {
        auto contours = reinterpret_cast<Contours*>(user);
        auto& contour = contours->back();
        auto& points = contour.points;

        vsg::vec2 p0(points.back());
        vsg::vec2 p1(float(control1->x), float(control1->y));
        vsg::vec2 p2(float(control2->x), float(control2->y));
        vsg::vec2 p3(float(to->x), float(to->y));

        if (p0==p1 && p1==p2 && p2==p3)
        {
            // ignore degenate segment
            // std::cout<<"cubic Error\n";
            return 0;
        }

        int numSteps = 10;

        float cx = 3.0f*(p1.x - p0.x);
        float bx = 3.0f*(p2.x - p1.x) - cx;
        float ax = p3.x - p0.x - cx - bx;
        float cy = 3.0f*(p1.y - p0.y);
        float by = 3.0f*(p2.y - p1.y) - cy;
        float ay = p3.y - p0.y - cy - by;

        float dt = 1.0f/float(numSteps);
        float u = dt;
        for (int i=1; i<numSteps; ++i)
        {
            vsg::vec2 p(ax*u*u*u + bx*u*u  + cx*u + p0.x,
                        ay*u*u*u + by*u*u  + cy*u + p0.y);

            points.push_back( p );
            u += dt;
        }

        points.push_back( p3 );
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
        std::cout<<"Warning: could not decomposs outline."<<error<<std::endl;
        return false;
    }

    return true;
}

void ReaderWriter_freetype::checkForAndFixDegenerates(Contours& contours) const
{
    auto contains_degenerates = [] (Contour& contour) -> bool
    {
        auto& points = contour.points;
        for(size_t i=0; i<points.size()-1; ++i)
        {
            auto& p0 = points[i];
            auto& p1 = points[i+1];
            if (p0==p1)
            {
                return true;
            }
        }

        // check if contour is closed.
        if (points.front() != points.back()) return true;

        return false;
    };

    auto fix_degenerates = [] (Contour& contour) -> void
    {
        if (contour.points.size()<2) return;

        auto& points = contour.points;

        decltype(Contour::points) clean_points;

        clean_points.push_back(points[0]);

        for(size_t i=0; i<points.size()-1; ++i)
        {
            auto& p0 = points[i];
            auto& p1 = points[i+1];
            if (p0!=p1)
            {
                clean_points.push_back(p1);
            }
        }

        if (clean_points.front() != clean_points.back())
        {
            // make sure the the contour is closed (last point equals first point.)
            clean_points.back() = clean_points.front();
        }

        points.swap(clean_points);
    };

    // fix all contours
    for(auto& contour : contours)
    {
        if (contains_degenerates(contour))
        {
            fix_degenerates(contour);
        }
    }
}

float ReaderWriter_freetype::nearest_contour_edge(const Contours& local_contours, const vsg::vec2& v) const
{
    float min_distance = std::numeric_limits<float>::max();
    for(auto& contour : local_contours)
    {
        auto& points = contour.points;
        auto& edges = contour.edges;
        for(size_t i=0; i<edges.size(); ++i)
        {
            auto& p0 = points[i];
            auto& edge = edges[i];

            vsg::vec2 v_p0 = v-p0;
            float dot_v_p0 = v_p0.x * edge.x + v_p0.y * edge.y;

            if (dot_v_p0<0.0f)
            {
                float distance = vsg::length2(v - p0);
                if (distance<min_distance) min_distance = distance;
            }
            else if (dot_v_p0 <= edge.z)
            {
                float d = v_p0.y * edge.x - v_p0.x * edge.y;
                float distance = d*d;
                if (distance<min_distance) min_distance = distance;
            }
        }
    }
    return sqrt(min_distance);
};

bool ReaderWriter_freetype::outside_contours(const Contours& local_contours, const vsg::vec2& v) const
{
    uint32_t numLeft = 0;
    for(auto& contour : local_contours)
    {
        auto& points = contour.points;
        for(size_t i=0; i<points.size()-1; ++i)
        {
            auto& p0 = points[i];
            auto& p1 = points[i+1];

            if (p0 == v || p1 == v)
            {
                // std::cout<<"v = "<<v<<" on end point p0="<<p0<<", p1"<<p1<<std::endl;
                return false;
            }

            if (p0.y == p1.y) // horizontal
            {
                if (p0.y == v.y)
                {
                    // v same height as segment
                    if (between(p0.x, v.x, p1.x))
                    {
                        // std::cout<<"Right on horizontal line v="<<v<<", p0 = "<<p0<<", p1 = "<<p1<<std::endl;
                        return false;
                    }
                }
            }
            else if (p0.x == p1.x) // vertical
            {
                if (between2(p0.y, v.y, p1.y))
                {
                    if (v.x == p0.x)
                    {
                        // std::cout<<"Right on vertical line v="<<v<<", p0 = "<<p0<<", p1 = "<<p1<<std::endl;
                        return false;
                    }
                    else if (p0.x < v.x)
                    {
                        ++numLeft;
                    }
                }
            }
            else // diagonal
            {
                if (between2(p0.y, v.y, p1.y))
                {
                    if (v.x > p0.x && v.x > p1.x)
                    {
                        // segment wholly left of v
                        ++numLeft;
                    }
                    else if (between(p0.x, v.x, p1.x))
                    {
                        // segment wholly right of v
                        // need to intersection test
                        float r = (v.y - p0.y) / (p1.y - p0.y);
                        float x_itersection = p0.x + (p1.x - p0.x)*r;
                        if (x_itersection < v.x) ++numLeft;
                    }
                }
            }

        }
    }
    return (numLeft % 2)==0;
}

vsg::ref_ptr<vsg::Object> ReaderWriter_freetype::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    auto ext = vsg::fileExtension(filename);
    if (_supportedFormats.find(ext)== _supportedFormats.end() || !vsg::fileExists(filename))
    {
        std::cout<<"ReaderWriter_freetype::read("<<filename<<") not found or extension not support"<<std::endl;
        std::cout<<"   supported "<< (_supportedFormats.find(ext)!= _supportedFormats.end())<<", exists = "<<vsg::fileExists(filename)<<std::endl;
        return {};
    }

    std::scoped_lock<std::mutex> lock(_mutex);

    init();
    if (!_library) return {};

    FT_Face face;
    FT_Long face_index = 0;
    int error = FT_New_Face( _library, filename.c_str(), face_index, &face );
    if (error == FT_Err_Unknown_File_Format)
    {
        std::cout<<"Warning: FreeType unable to read font file : "<<filename<<", error = "<<FT_Err_Unknown_File_Format<<std::endl;
        return {};
    }
    else if (error)
    {
        std::cout<<"Warning: FreeType unable to read font file : "<<filename<<", error = "<<error<<std::endl;
        return {};
    }


    FT_UInt pixel_size = 48;
    FT_UInt freetype_pixel_size = pixel_size;
    float freetype_pixel_size_scale = float(pixel_size) / (64.0f * float(freetype_pixel_size));

    {
        error = FT_Set_Pixel_Sizes(face, freetype_pixel_size, freetype_pixel_size );
    }

    FT_Int32 load_flags = FT_LOAD_NO_BITMAP;
    FT_Render_Mode render_mode = FT_RENDER_MODE_NORMAL;

    struct GlyphQuad
    {
        FT_ULong charcode;
        FT_ULong glyph_index;
        unsigned int width;
        unsigned int height;
        inline bool operator < (const GlyphQuad& rhs) const { return height < rhs.height; }
    };

    std::multiset<GlyphQuad> sortedGlyphQuads;

    bool hasSpace = false;

    // collect all the sizes of the glyphs
#if 0
    {
        auto add_gyph = [&](FT_ULong charcode)
        {
            FT_UInt glyph_index = FT_Get_Char_Index( face, charcode);
            int load_error = FT_Load_Glyph(face, glyph_index, load_flags);

            if (load_error) return;

            GlyphQuad quad{
                charcode,
                glyph_index,
                static_cast<unsigned int >(ceil(float(face->glyph->metrics.width) * freetype_pixel_size_scale)),
                static_cast<unsigned int >(ceil(float(face->glyph->metrics.height) * freetype_pixel_size_scale))
            };
            sortedGlyphQuads.insert(quad);
        };

        std::string test("M");
        for(auto& c : test) add_gyph(c);

    }
#else
    {
        FT_ULong  charcode;
        FT_UInt   glyph_index;

        charcode = FT_Get_First_Char(face, &glyph_index);
        while ( glyph_index != 0 )
        {
            charcode = FT_Get_Next_Char(face, charcode, &glyph_index);

            error = FT_Load_Glyph(face, glyph_index, load_flags);

            if (error) continue;

            GlyphQuad quad{
                charcode,
                glyph_index,
                static_cast<unsigned int >(ceil(float(face->glyph->metrics.width) * freetype_pixel_size_scale)),
                static_cast<unsigned int >(ceil(float(face->glyph->metrics.height) * freetype_pixel_size_scale))
            };

            if (charcode==32) hasSpace = true;

            sortedGlyphQuads.insert(quad);
        }
    }
#endif

    if (!hasSpace)
    {
        FT_ULong charcode = 32;
        FT_UInt glyph_index = FT_Get_Char_Index( face, charcode);

        if (glyph_index != 0)
        {
            error = FT_Load_Glyph(face, glyph_index, load_flags);

            if (!error)
            {
                GlyphQuad quad{
                    charcode,
                    glyph_index,
                    static_cast<unsigned int >(ceil(float(face->glyph->metrics.width) * freetype_pixel_size_scale)),
                    static_cast<unsigned int >(ceil(float(face->glyph->metrics.height) * freetype_pixel_size_scale))
                };

                sortedGlyphQuads.insert(quad);

                hasSpace = true;
            }
        }
    }


    double total_width = 0.0;
    double total_height = 0.0;
    for(auto& glyph : sortedGlyphQuads)
    {
        total_width += double(glyph.width);
        total_height += double(glyph.height);
    }

    double average_width = total_width / double(sortedGlyphQuads.size());
    double average_height = total_height / double(sortedGlyphQuads.size());

    std::cout<<"average_width = "<<average_width<<", average_height ="<<average_height<<" hasSpace = "<<hasSpace<<std::endl;

    unsigned int texel_margin = pixel_size/4;
    int quad_margin = texel_margin/2;

    unsigned int provisional_cells_across = static_cast<unsigned int>(ceil(sqrt(double(face->num_glyphs))));
    unsigned int provisional_width = provisional_cells_across * (average_width+texel_margin);

    //provisional_width = 1024;

    unsigned int xpos = texel_margin;
    unsigned int ypos = texel_margin;
    unsigned int xtop = 2*texel_margin;
    unsigned int ytop = 2*texel_margin;
    for(auto& glyphQuad : sortedGlyphQuads)
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

#if 1
    auto atlas = vsg::ubyteArray2D::create(xtop, ytop, vsg::Data::Layout{VK_FORMAT_R8_UNORM});
    float max_value = std::numeric_limits<vsg::ubyteArray2D::value_type>::max() ;
    float mid_value = ceil(max_value/2.0f);
#else
    auto atlas = vsg::ushortArray2D::create(xtop, ytop, vsg::Data::Layout{VK_FORMAT_R16_UNORM});
    float max_value = std::numeric_limits<vsg::ushortArray2D::value_type>::max() ;
    float mid_value = ceil(max_value/2.0f);
#endif

    // initialize to zeros
    for(auto& c : *atlas) c = 0;

    auto font = vsg::Font::create();
    font->atlas = atlas;


    xpos = texel_margin;
    ypos = texel_margin;
    ytop = 0;


    bool useOutline = true;
    bool computeSDF = true;

    double total_nearest_edge = 0.0;
    double total_outside_edge = 0.0;

    for(auto& glyphQuad : sortedGlyphQuads)
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
            for(auto& contour : contours)
            {
                for(auto& v : contour.points)
                {
                    // scale and translate to local origin
                    v.x = v.x * freetype_pixel_size_scale - offset.x;
                    v.y = offset.y - v.y * freetype_pixel_size_scale;
                }
            }

            // fix any degernate segments
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
            for(auto& contour : contours)
            {
                auto& points = contour.points;
                for(auto& v : points)
                {
                    extents.add(v);
                }

                auto& edges = contour.edges;
                edges.resize(points.size()-1);
                for(size_t i=0; i<edges.size(); ++i)
                {
                    vsg::vec2 dv = points[i+1] - points[i];
                    float len = vsg::length(dv);
                    dv /= len;
                    edges[i].set(dv.x, dv.y, len);
                }
            }

            if (!contours.empty())
            {
                float scale = 1.0f/float(quad_margin);
                int delta = quad_margin-2;
                for(int r = -delta; r<static_cast<int>(height+delta); ++r)
                {
                    std::size_t index = atlas->index(xpos-delta, ypos+r);
                    for(int c = -delta; c<static_cast<int>(width + delta); ++c)
                    {
                        vsg::vec2 v;
                        v.set(float(c), float(r));

                        auto before_nearest_edge =std::chrono::steady_clock::now();

                        auto min_distance = nearest_contour_edge(contours, v);

                        auto after_nearest_edge =std::chrono::steady_clock::now();

                        if (!extents.contains(v) || outside_contours(contours, v)) min_distance = -min_distance;

                        auto after_outside_edge =std::chrono::steady_clock::now();

                        total_nearest_edge += std::chrono::duration<double, std::chrono::seconds::period>(after_nearest_edge - before_nearest_edge).count();
                        total_outside_edge += std::chrono::duration<double, std::chrono::seconds::period>(after_outside_edge - after_nearest_edge).count();

                        //std::cout<<"nearest_contour_edge("<<r<<", "<<c<<") min_distance = "<<min_distance<<std::endl;
                        float distance_ratio = (min_distance)*scale;
                        float value = mid_value + distance_ratio*mid_value;

                        if (value<=0.0f) atlas->at(index++) = 0;
                        else if (value>=max_value) atlas->at(index++) = max_value;
                        else atlas->at(index++) = value;
                    }
                }
            }
        }
        else
        {
            if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP)
            {
                error = FT_Render_Glyph( face->glyph, render_mode );
                if (error) continue;
            }

            if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP) continue;

            const FT_Bitmap& bitmap = face->glyph->bitmap;


            // copy pixels
            if (computeSDF)
            {
                int delta = quad_margin-2;
                for(int r = -delta; r<static_cast<int>(bitmap.rows+delta); ++r)
                {
                    std::size_t index = atlas->index(xpos-delta, ypos+r);
                    for(int c = -delta; c<static_cast<int>(bitmap.width + delta); ++c)
                    {
                        atlas->at(index++) = nearerst_edge(bitmap, c, r, quad_margin);
                    }
                }
            }
            else
            {
                const unsigned char* ptr = bitmap.buffer;
                for(unsigned int r = 0; r<bitmap.rows; ++r)
                {
                    std::size_t index = atlas->index(xpos, ypos+r);
                    for(unsigned int c = 0; c<bitmap.width; ++c)
                    {
                        atlas->at(index++) = *ptr++;
                    }
                }
            }
        }

        vsg::vec4 uvrect(
            (float(xpos-quad_margin)-1.0f)/float(atlas->width()-1), float(ypos + height+quad_margin)/float(atlas->height()-1),
            float(xpos + width + quad_margin)/float(atlas->width()-1), float((ypos-quad_margin)-1.0f)/float(atlas->height()-1)
        );

        vsg::Font::GlyphMetrics vsg_metrics;
        vsg_metrics.charcode = glyphQuad.charcode;
        vsg_metrics.uvrect = uvrect;
        vsg_metrics.width = float(width+2*quad_margin)/float(pixel_size);
        vsg_metrics.height = float(height+2*quad_margin)/float(pixel_size);
        vsg_metrics.horiBearingX = (float(metrics.horiBearingX) * freetype_pixel_size_scale - float(quad_margin))/float(pixel_size);
        vsg_metrics.horiBearingY = (float(metrics.horiBearingY) * freetype_pixel_size_scale + float(quad_margin))/float(pixel_size);
        vsg_metrics.horiAdvance = (float(metrics.horiAdvance) * freetype_pixel_size_scale)/float(pixel_size);
        vsg_metrics.vertBearingX = (float(metrics.vertBearingX) * freetype_pixel_size_scale - float(quad_margin))/float(pixel_size);
        vsg_metrics.vertBearingY = (float(metrics.vertBearingY) * freetype_pixel_size_scale + float(quad_margin))/float(pixel_size);
        vsg_metrics.vertAdvance = (float(metrics.vertAdvance) * freetype_pixel_size_scale)/float(pixel_size);

        font->glyphs[glyphQuad.charcode] = vsg_metrics;

        unsigned int local_ytop = ypos + height + texel_margin;
        if (local_ytop > ytop) ytop = local_ytop;

        xpos += (width + texel_margin);
        if (xpos > xtop) xtop = xpos;
    }

    font->ascender = float(face->ascender) * freetype_pixel_size_scale / float(pixel_size);
    font->descender = float(face->descender) * freetype_pixel_size_scale / float(pixel_size);
    font->height = float(face->height) * freetype_pixel_size_scale / float(pixel_size);

    font->options = const_cast<vsg::Options*>(options.get());

    return font;
}
