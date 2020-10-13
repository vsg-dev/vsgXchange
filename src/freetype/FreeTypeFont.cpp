#include "FreeTypeFont.h"

#include <vsg/core/Exception.h>
#include <vsg/state/ShaderStage.h>
#include <vsg/text/Font.h>


using namespace vsgXchange;

#include <iostream>
#include <set>

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

    FT_UInt pixel_size = 64;

    {
        error = FT_Set_Pixel_Sizes(face, pixel_size, pixel_size );
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
                static_cast<unsigned int >(ceil(double(face->glyph->metrics.width)/64.0)),
                static_cast<unsigned int >(ceil(double(face->glyph->metrics.height)/64.0))
            };

            if (charcode==32) hasSpace = true;

            sortedGlyphQuads.insert(quad);
        }
    }

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
                    static_cast<unsigned int >(ceil(double(face->glyph->metrics.width)/64.0)),
                    static_cast<unsigned int >(ceil(double(face->glyph->metrics.height)/64.0))
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

    std::cout<<"provisional_width = "<<provisional_width<<", xtop = "<<xtop<<", ytop = "<<ytop<<std::endl;

    //xtop = provisional_width;

#if 0
    auto atlas = vsg::ubyteArray2D::create(xtop, ytop, vsg::Data::Layout{VK_FORMAT_R8_UNORM});
    float max_value = std::numeric_limits<vsg::ubyteArray2D::value_type>::max() ;
    float mid_value = ceil(max_value/2.0f);
#else
    auto atlas = vsg::ushortArray2D::create(xtop, ytop, vsg::Data::Layout{VK_FORMAT_R16_UNORM});
    float max_value = std::numeric_limits<vsg::ushortArray2D::value_type>::max() ;
    float mid_value = ceil(max_value/2.0f);
#endif
    std::cout<<"max_value = "<<max_value<<" mid_value ="<<mid_value<<std::endl;

    // initialize to zeros
    for(auto& c : *atlas) c = 0;

    auto font = vsg::Font::create();
    font->atlas = atlas;


    xpos = texel_margin;
    ypos = texel_margin;
    ytop = 0;


    bool useOutline = true;
    bool computeSDF = true;

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

            using Contour = std::vector<vsg::vec2>;
            using Contours = std::list<Contour>;

            //std::cout<<"charcode = "<<glyphQuad.charcode<<", width = "<<width<<", height = "<<height<<std::endl;
            //std::cout<<"   face->glyph->outline.n_contours = "<<face->glyph->outline.n_contours<<std::endl;
            //std::cout<<"   face->glyph->outline.n_points = "<<face->glyph->outline.n_points<<std::endl;
            auto moveTo = [] ( const FT_Vector* to, void* user ) -> int
            {
                Contours* contours = reinterpret_cast<Contours*>(user);
                contours->push_back(Contour());
                Contour& contour = contours->back();
                contour.emplace_back(float(to->x)/64.0f, float(to->y)/64.0f);
                return 0;
            };

            auto lineTo = [] ( const FT_Vector* to, void* user ) -> int
            {
                Contours* contours = reinterpret_cast<Contours*>(user);
                Contour& contour = contours->back();
                contour.emplace_back(float(to->x)/64.0f, float(to->y)/64.0f);
                return 0;
            };

            auto conicTo = [] ( const FT_Vector* control, const FT_Vector* to, void* user ) -> int
            {
                Contours* contours = reinterpret_cast<Contours*>(user);
                Contour& contour = contours->back();

                vsg::vec2 p0(contour.back());
                vsg::vec2 p1(float(control->x)/64.0f, float(control->y)/64.0f);
                vsg::vec2 p2(float(to->x)/64.0f, float(to->y)/64.0f);

                //contour.push_back(p2);
                //return 0;

                int numSteps = 10;

                float dt = 1.0/float(numSteps);
                float u=0;
                for (int i=0; i<=numSteps; ++i)
                {
                    float w = 1.0f;
                    float bs = 1.0f/( (1.0f-u)*(1.0f-u)+2.0f*(1.0f-u)*u*w +u*u );
                    vsg::vec2 p = (p0*((1.0f-u)*(1.0f-u)) + p1*(2.0f*(1.0f-u)*u*w) + p2*(u*u))*bs;
                    contour.push_back( p );
                    u += dt;
                }
                return 0;
            };

            auto cubicTo = [] ( const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* user ) -> int
            {
                Contours* contours = reinterpret_cast<Contours*>(user);
                Contour& contour = contours->back();

                vsg::vec2 p0(contour.back());
                vsg::vec2 p1(float(control1->x)/64.0f, float(control1->y)/64.0f);
                vsg::vec2 p2(float(control2->x)/64.0f, float(control2->y)/64.0f);
                vsg::vec2 p3(float(to->x)/64.0f, float(to->y)/64.0f);

                //contour.push_back(p3);
                //return 0;

                int numSteps = 10;

                float cx = 3.0f*(p1.x - p0.x);
                float bx = 3.0f*(p2.x - p1.x) - cx;
                float ax = p3.x - p0.x - cx - bx;
                float cy = 3.0f*(p1.y - p0.y);
                float by = 3.0f*(p2.y - p1.y) - cy;
                float ay = p3.y - p0.y - cy - by;

                float dt = 1.0f/float(numSteps);
                float u=0.0f;
                for (int i=0; i<=numSteps; ++i)
                {
                    vsg::vec2 p(ax*u*u*u + bx*u*u  + cx*u + p0.x,
                                ay*u*u*u + by*u*u  + cy*u + p0.y);
                    contour.push_back( p );
                    u += dt;
                }
                return 0;
            };

            auto& outline = face->glyph->outline;

            FT_Outline_Funcs funcs;
            funcs.move_to = moveTo;
            funcs.line_to = lineTo;
            funcs.conic_to = conicTo;
            funcs.cubic_to = cubicTo;
            funcs.shift = 0;
            funcs.delta = 0;

            Contours contours;

            // ** record description
            error = FT_Outline_Decompose(&outline, &funcs, &contours);

            if (error != 0)
            {
                std::cout<<"Warning: could not decomposs outline"<<error<<" for "<<glyphQuad.charcode<<std::endl;
                continue;
            }

            vsg::vec2 offset(float(metrics.horiBearingX)/64.0f, float(metrics.horiBearingY)/64.0f);
            for(auto& contour : contours)
            {
                for(auto& v : contour)
                {
                    v.set(v.x - offset.x, offset.y - v.y);
                }
            }
#if 0
            std::cout<<"error = "<<error<<std::endl;
            std::cout<<"contours.size() = "<<contours.size()<<std::endl;
            std::cout<<"offset = "<<offset<<std::endl;
#endif
            auto nearest_contour_edge = [](const Contours& local_contours, int r, int c) -> float
            {
                vsg::vec2 v;
                v.set(float(r), float(c));
                float min_distance = std::numeric_limits<float>::max();
                for(auto& contour : local_contours)
                {

                    for(size_t i=0; i<contour.size()-1; ++i)
                    {
                        auto& p0 = contour[i];
                        auto& p1 = contour[i+1];
                        vsg::vec2 p1_p0 = p1-p0;
                        vsg::vec2 v_p0 = v-p0;
                        vsg::vec2 v_p1 = v-p1;
                        float dot_v_p0 = vsg::dot(v_p0, p1_p0);
                        float dot_v_p1 = vsg::dot(v_p1, p1_p0);

                        float distance;
                        if (dot_v_p0<=0.0f)
                        {
                            distance = vsg::length(v - p0);
                            // std::cout<<"    distance from p0 = "<<distance<<"\n";
                        }
                        else if (dot_v_p1>=0.0f)
                        {
                            distance = vsg::length(v - p1);
                            // std::cout<<"    distance from p1 = "<<distance<<"\n";
                        }
                        else
                        {
                            distance = vsg::cross(v_p0, p1_p0) / vsg::length(p1_p0);
                            if (distance<0.0f)
                            {
                                // std::cout<<"    Flipping v_p0 = "<<v_p0<<", p1_p0 = "<<p1_p0<<"\n";
                                distance = -distance;
                            }
                            else
                            {
                                // std::cout<<"    Not flipping v_p0 = "<<v_p0<<", p1_p0 = "<<p1_p0<<"\n";
                            }
                        }

                        if (distance<min_distance) min_distance = distance;
                    }
                }
                return min_distance;
            };


            auto outside_contours = [](const Contours& local_contours, int row, int col) -> bool
            {
                vsg::vec2 v;
                v.set(float(row+0.001f), float(col+0.001f));
                uint32_t numLeft = 0;
                for(auto& contour : local_contours)
                {
                    for(size_t i=0; i<contour.size()-1; ++i)
                    {
                        auto& p0 = contour[i];
                        auto& p1 = contour[i+1];
                        if (p0.y == p1.y) // horizontal
                        {
                            if ((v.y == p0.y) && between(p0.x, v.x, p1.x)) return false;
                        }
                        else if (between(p0.y, v.y, p1.y))
                        {
                            if (between(p0.x, v.x, p1.x))
                            {
                                // need to intersection test
                                float r = (v.y - p0.y) / (p1.y - p0.y);
                                float x_itersection = p0.x + (p1.x - p0.x)*r;
                                if (x_itersection < v.x) ++numLeft;
                            }
                            else if (p0.x < v.x && p1.x < v.x)
                            {
                                ++numLeft;
                            }
                        }
                    }
                }
                return (numLeft % 2)==0;
            };

            if (!contours.empty())
            {
                float scale = 1.0f/float(quad_margin);
                int delta = quad_margin-2;
                for(int r = -delta; r<static_cast<int>(height+delta); ++r)
                {
                    std::size_t index = atlas->index(xpos-delta, ypos+r);
                    for(int c = -delta; c<static_cast<int>(width + delta); ++c)
                    {
                        auto min_distance = nearest_contour_edge(contours, c, r);
                        if (outside_contours(contours, c, r)) min_distance = -min_distance;

                        //std::cout<<"nearest_contour_edge("<<r<<", "<<c<<") min_distance = "<<min_distance<<std::endl;
                        float distance_ratio = (min_distance)*scale;
                        float v = mid_value + distance_ratio*mid_value;

                        if (v<=0.0f) atlas->at(index++) = 0;
                        else if (v>=max_value) atlas->at(index++) = max_value;
                        else atlas->at(index++) = v;
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
        vsg_metrics.horiBearingX = (float(metrics.horiBearingX-quad_margin)/64.0f)/float(pixel_size);
        vsg_metrics.horiBearingY = (float(metrics.horiBearingY-quad_margin)/64.0f)/float(pixel_size);
        vsg_metrics.horiAdvance = (float(metrics.horiAdvance)/64.0f)/float(pixel_size);
        vsg_metrics.vertBearingX = (float(metrics.vertBearingX-quad_margin)/64.0f)/float(pixel_size);
        vsg_metrics.vertBearingY = (float(metrics.vertBearingY-quad_margin)/64.0f)/float(pixel_size);
        vsg_metrics.vertAdvance = (float(metrics.vertAdvance)/64.0f)/float(pixel_size);

        font->glyphs[glyphQuad.charcode] = vsg_metrics;

        unsigned int local_ytop = ypos + height + texel_margin;
        if (local_ytop > ytop) ytop = local_ytop;

        xpos += (width + texel_margin);
        if (xpos > xtop) xtop = xpos;
    }

    font->fontHeight = float(pixel_size);
    font->normalisedLineHeight = 1.25;
    font->options = const_cast<vsg::Options*>(options.get());

    std::cout<<"FreeTypeFont::read(filename = "<<filename<<") num_glyphs = "<<face->num_glyphs<<", sortedGlyphQuads.size() = "<<sortedGlyphQuads.size()<<", atlas->width() = "<<atlas->width()<< ", atlas->height() = "<<atlas->height()<<" font->glyphs.size() = "<<font->glyphs.size()<<std::endl;

    return font;
}
