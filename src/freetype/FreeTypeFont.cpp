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

    FT_UInt pixel_size = 32;

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

    // collect all the sizes of the glyphs
    {
        FT_ULong  charcode;
        FT_UInt   glyph_index;

        charcode = FT_Get_First_Char( face, &glyph_index );
        while ( glyph_index != 0 )
        {
            charcode = FT_Get_Next_Char( face, charcode, &glyph_index );

            error = FT_Load_Glyph(
                face,          /* handle to face object */
                glyph_index,   /* glyph index           */
                load_flags );

            GlyphQuad quad{
                charcode,
                glyph_index,
                static_cast<unsigned int >(ceil(double(face->glyph->metrics.width)/64.0)),
                static_cast<unsigned int >(ceil(double(face->glyph->metrics.height)/64.0))
            };

            sortedGlyphQuads.insert(quad);
        }
    }

    unsigned int provisional_cells_width = static_cast<unsigned int>(ceil(sqrt(double(face->num_glyphs))));
    unsigned int provisional_width = provisional_cells_width * pixel_size;

    unsigned int xpos = 0;
    unsigned int ypos = 0;
    unsigned int xtop = 0;
    unsigned int ytop = 0;
    unsigned int margin = 8;
    for(auto& glyphQuad : sortedGlyphQuads)
    {
        unsigned int width = glyphQuad.width;
        unsigned int height = glyphQuad.height;

        if ((xpos + width + margin) > provisional_width)
        {
            // glyph doesn't fit in present row so shift to next row.
            xpos = 0;
            ypos = ytop;
        }

        unsigned int local_ytop = ypos + height + margin;
        if (local_ytop > ytop) ytop = local_ytop;

        xpos += (width + margin);
        if (xpos > xtop) xtop = xpos;
    }

    auto atlas = vsg::ubyteArray2D::create(xtop, ytop, vsg::Data::Layout{VK_FORMAT_R8_UNORM});

    auto font = vsg::Font::create();
    font->atlas = atlas;

    xpos = 0;
    ypos = 0;
    ytop = 0;

    for(auto& glyphQuad : sortedGlyphQuads)
    {
        error = FT_Load_Glyph(face, glyphQuad.glyph_index, load_flags);
        if (error!=0) continue;

        if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP)
        {
            error = FT_Render_Glyph( face->glyph, render_mode );
            if (error!=0) continue;
        }

        if (face->glyph->format != FT_GLYPH_FORMAT_BITMAP) continue;

        const FT_Bitmap& bitmap = face->glyph->bitmap;
        unsigned int width = bitmap.width;
        unsigned int height = bitmap.rows;

        if ((xpos + width + margin) > atlas->width())
        {
            // glyph doesn't fit in present row so shift to next row.
            xpos = 0;
            ypos = ytop;
        }

        const unsigned char* ptr = bitmap.buffer;
        for(unsigned int r = 0; r<bitmap.rows; ++r)
        {
            std::size_t index = atlas->index(xpos, ypos+r);
            for(unsigned int c = 0; c<bitmap.width; ++c)
            {
                //atlas->at(index++) = c + r*glyphQuad.width;
                atlas->at(index++) = *ptr++;
            }
        }

        vsg::vec4 uvrect(
            float(xpos)/float(atlas->width()-1), float(ypos + height)/float(atlas->height()-1),
            float(xpos + width)/float(atlas->width()-1), float(ypos)/float(atlas->height()-1)
        );

        auto metrics = face->glyph->metrics;

        vsg::Font::GlyphMetrics vsg_metrics;
        vsg_metrics.charcode = glyphQuad.charcode;
        vsg_metrics.uvrect = uvrect;
        vsg_metrics.width = width/float(pixel_size);
        vsg_metrics.height = height/float(pixel_size);
        vsg_metrics.horiBearingX = (float(metrics.horiBearingX)/64.0f)/float(pixel_size);
        vsg_metrics.horiBearingY = (float(metrics.horiBearingY)/64.0f)/float(pixel_size);
        vsg_metrics.horiAdvance = (float(metrics.horiAdvance)/64.0f)/float(pixel_size);
        vsg_metrics.vertBearingX = (float(metrics.vertBearingX)/64.0f)/float(pixel_size);
        vsg_metrics.vertBearingY = (float(metrics.vertBearingY)/64.0f)/float(pixel_size);
        vsg_metrics.vertAdvance = (float(metrics.vertAdvance)/64.0f)/float(pixel_size);

        font->glyphs[glyphQuad.charcode] = vsg_metrics;

        unsigned int local_ytop = ypos + height + margin;
        if (local_ytop > ytop) ytop = local_ytop;

        xpos += (width + margin);
        if (xpos > xtop) xtop = xpos;
    }

    font->fontHeight = float(pixel_size);
    font->normalisedLineHeight = 1.25;
    font->options = const_cast<vsg::Options*>(options.get());

    std::cout<<"FreeTypeFont::read(filename = "<<filename<<") num_glyphs = "<<face->num_glyphs<<", sortedGlyphQuads.size() = "<<sortedGlyphQuads.size()<<", atlas->width() = "<<atlas->width()<< ", atlas->height() = "<<atlas->height()<<std::endl;

    return font;
}
