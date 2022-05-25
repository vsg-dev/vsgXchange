/* <editor-fold desc="MIT License">

Copyright(c) 2021 Josef Stumpfegger

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgXchange/images.h>

#include <cstring>
#include <vsg/io/FileSystem.h>

#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfRgbaFile.h>
#include <iostream>
#ifdef EXRVERSION3
#    include <Imath/half.h>
#    include <ImfInt64.h>
#else
#    include <OpenEXR/half.h>
#endif

using namespace vsgXchange;

namespace
{

    class CPP_IStream : public Imf::IStream
    {
    public:
        CPP_IStream(std::istream& file, const char fileName[]) :
            IStream(fileName), _file(file) {}
        virtual bool read(char c[], int n)
        {
            _file.read(c, n);
            return !_file.eof();
        };
        virtual Imf::Int64 tellg()
        {
            return _file.tellg();
        };
        virtual void seekg(Imf::Int64 pos)
        {
            _file.seekg(pos);
        };
        virtual void clear()
        {
            _file.clear();
        };

    private:
        std::istream& _file;
    };
    class CPP_OStream : public Imf::OStream
    {
    public:
        CPP_OStream(std::ostream& file, const char fileName[]) :
            OStream(fileName), _file(file) {}
        virtual void write(const char c[], int n)
        {
            _file.write(c, n);
        };
        virtual Imf::Int64 tellp()
        {
            return _file.tellp();
        };
        virtual void seekp(Imf::Int64 pos)
        {
            _file.seekp(pos);
        };
        virtual void clear()
        {
            _file.clear();
        };

    private:
        std::ostream& _file;
    };
    class Array_IStream : public Imf::IStream
    {
    public:
        Array_IStream(const uint8_t* in_data, size_t in_size, const char fileName[]) :
            IStream(fileName), data(in_data), size(in_size), curPlace(0) {}
        virtual bool read(char c[], int n)
        {
            if (curPlace + n > size)
                throw Iex::InputExc("Unexpected end of file.");
            std::copy_n(data + curPlace, n, c);
            curPlace += n;
            return curPlace != size;
        };
        virtual Imf::Int64 tellg()
        {
            return curPlace;
        };
        virtual void seekg(Imf::Int64 pos)
        {
            curPlace = 0;
        };
        virtual void clear(){};

    private:
        const uint8_t* data;
        size_t size;
        size_t curPlace;
    };

    static vsg::ref_ptr<vsg::Object> parseOpenExr(Imf::InputFile& file)
    {
        Imath::Box2i dw = file.header().dataWindow();
        int width = dw.max.x - dw.min.x + 1;
        int height = dw.max.y - dw.min.y + 1;
        auto begin = file.header().channels().begin();
        int channelCount = 0;
        while (begin != file.header().channels().end())
        {
            ++begin;
            channelCount++;
        }
        if (channelCount > 4) return {};

        //single element half precision float
        if (channelCount == 1 && file.header().channels().begin().channel().type == Imf::HALF)
        {
            uint16_t* pixels = new uint16_t[width * height];
            Imf::FrameBuffer frameBuffer;
            frameBuffer.insert(file.header().channels().begin().name(),
                               Imf::Slice(Imf::HALF, (char*)(pixels - dw.min.x - dw.min.y * width),
                                          sizeof(pixels[0]),
                                          sizeof(pixels[0]) * width));
            file.setFrameBuffer(frameBuffer);
            file.readPixels(dw.min.y, dw.max.y);
            return vsg::ushortArray2D::create(width, height, pixels, vsg::Data::Layout{VK_FORMAT_R16_SFLOAT});
        }

        //single element single precision float
        if (channelCount == 1 && file.header().channels().begin().channel().type == Imf::FLOAT)
        {
            float* pixels = new float[width * height];
            Imf::FrameBuffer frameBuffer;
            frameBuffer.insert(file.header().channels().begin().name(),
                               Imf::Slice(Imf::FLOAT, (char*)(pixels - dw.min.x - dw.min.y * width),
                                          sizeof(pixels[0]),
                                          sizeof(pixels[0]) * width));
            file.setFrameBuffer(frameBuffer);
            file.readPixels(dw.min.y, dw.max.y);
            return vsg::floatArray2D::create(width, height, pixels, vsg::Data::Layout{VK_FORMAT_R32_SFLOAT});
        }

        //single element single precision uint
        if (channelCount == 1 && file.header().channels().begin().channel().type == Imf::UINT)
        {
            uint32_t* pixels = new uint32_t[width * height];
            Imf::FrameBuffer frameBuffer;
            frameBuffer.insert(file.header().channels().begin().name(),
                               Imf::Slice(Imf::UINT, (char*)(pixels - dw.min.x - dw.min.y * width),
                                          sizeof(pixels[0]),
                                          sizeof(pixels[0]) * width));
            file.setFrameBuffer(frameBuffer);
            file.readPixels(dw.min.y, dw.max.y);
            return vsg::uintArray2D::create(width, height, pixels, vsg::Data::Layout{VK_FORMAT_R32_UINT});
        }

        //4 elements half precision float
        if (file.header().channels().begin().channel().type == Imf::HALF)
        {
            vsg::usvec4* pixels = new vsg::usvec4[width * height];
            Imf::FrameBuffer frameBuffer;
            frameBuffer.insert("R",
                               Imf::Slice(Imf::HALF, (char*)(&(pixels - dw.min.x - dw.min.y * width)->r),
                                          sizeof(pixels[0]),
                                          sizeof(pixels[0]) * width));
            frameBuffer.insert("G",
                               Imf::Slice(Imf::HALF, (char*)(&(pixels - dw.min.x - dw.min.y * width)->g),
                                          sizeof(pixels[0]),
                                          sizeof(pixels[0]) * width));
            frameBuffer.insert("B",
                               Imf::Slice(Imf::HALF, (char*)(&(pixels - dw.min.x - dw.min.y * width)->b),
                                          sizeof(pixels[0]),
                                          sizeof(pixels[0]) * width));
            frameBuffer.insert("A",
                               Imf::Slice(Imf::HALF, (char*)(&(pixels - dw.min.x - dw.min.y * width)->a),
                                          sizeof(pixels[0]),
                                          sizeof(pixels[0]) * width));
            file.setFrameBuffer(frameBuffer);
            file.readPixels(dw.min.y, dw.max.y);
            return vsg::usvec4Array2D::create(width, height, pixels, vsg::Data::Layout{VK_FORMAT_R16G16B16A16_SFLOAT});
        }

        //4 elements single precision float
        if (file.header().channels().begin().channel().type == Imf::FLOAT)
        {
            vsg::vec4* pixels = new vsg::vec4[width * height];
            Imf::FrameBuffer frameBuffer;
            frameBuffer.insert("R",
                               Imf::Slice(Imf::FLOAT, (char*)(&(pixels - dw.min.x - dw.min.y * width)->r),
                                          sizeof(pixels[0]),
                                          sizeof(pixels[0]) * width));

            frameBuffer.insert("G",
                               Imf::Slice(Imf::FLOAT, (char*)(&(pixels - dw.min.x - dw.min.y * width)->g),
                                          sizeof(pixels[0]),
                                          sizeof(pixels[0]) * width));
            frameBuffer.insert("B",
                               Imf::Slice(Imf::FLOAT, (char*)(&(pixels - dw.min.x - dw.min.y * width)->b),
                                          sizeof(pixels[0]),
                                          sizeof(pixels[0]) * width));
            frameBuffer.insert("A",
                               Imf::Slice(Imf::FLOAT, (char*)(&(pixels - dw.min.x - dw.min.y * width)->a),
                                          sizeof(pixels[0]),
                                          sizeof(pixels[0]) * width));
            file.setFrameBuffer(frameBuffer);
            file.readPixels(dw.min.y, dw.max.y);
            return vsg::vec4Array2D::create(width, height, pixels, vsg::Data::Layout{VK_FORMAT_R32G32B32A32_SFLOAT});
        }

        //4 elements single precision uint
        if (file.header().channels().begin().channel().type == Imf::UINT)
        {
            vsg::uivec4* pixels = new vsg::uivec4[width * height];
            Imf::FrameBuffer frameBuffer;
            frameBuffer.insert("R",
                               Imf::Slice(Imf::UINT, (char*)(&(pixels - dw.min.x - dw.min.y * width)->r),
                                          sizeof(pixels[0]),
                                          sizeof(pixels[0]) * width));
            frameBuffer.insert("G",
                               Imf::Slice(Imf::UINT, (char*)(&(pixels - dw.min.x - dw.min.y * width)->g),
                                          sizeof(pixels[0]),
                                          sizeof(pixels[0]) * width));
            frameBuffer.insert("B",
                               Imf::Slice(Imf::UINT, (char*)(&(pixels - dw.min.x - dw.min.y * width)->b),
                                          sizeof(pixels[0]),
                                          sizeof(pixels[0]) * width));
            frameBuffer.insert("A",
                               Imf::Slice(Imf::UINT, (char*)(&(pixels - dw.min.x - dw.min.y * width)->a),
                                          sizeof(pixels[0]),
                                          sizeof(pixels[0]) * width));
            file.setFrameBuffer(frameBuffer);
            file.readPixels(dw.min.y, dw.max.y);
            return vsg::uivec4Array2D::create(width, height, pixels, vsg::Data::Layout{VK_FORMAT_R32G32B32A32_UINT});
        }

        return {};
    }

    struct InitializeHeader : public vsg::ConstVisitor
    {
        std::unique_ptr<Imf::Header> header;
        int numScanLines = 0;

        void apply(const vsg::ushortArray2D& obj)
        { //single precision half float
            header.reset(new Imf::Header(obj.width(), obj.height()));
            header->channels().insert("Y", Imf::Channel(Imf::HALF));
            numScanLines = obj.height();
        }

        void apply(const vsg::floatArray2D& obj) override
        { //single precision float
            header.reset(new Imf::Header(obj.width(), obj.height()));
            header->channels().insert("Y", Imf::Channel(Imf::FLOAT));
            numScanLines = obj.height();
        }

        void apply(const vsg::uintArray2D& obj) override
        { //single precision uint
            header.reset(new Imf::Header(obj.width(), obj.height()));
            header->channels().insert("Y", Imf::Channel(Imf::UINT));
            numScanLines = obj.height();
        }

        void apply(const vsg::usvec4Array2D& obj) override
        { //single precision short
            header.reset(new Imf::Header(obj.width(), obj.height()));
            header->channels().insert("R", Imf::Channel(Imf::HALF));
            header->channels().insert("G", Imf::Channel(Imf::HALF));
            header->channels().insert("B", Imf::Channel(Imf::HALF));
            header->channels().insert("A", Imf::Channel(Imf::HALF));
            numScanLines = obj.height();
        }

        void apply(const vsg::vec4Array2D& obj) override
        { //single precision float
            header.reset(new Imf::Header(obj.width(), obj.height()));
            header->channels().insert("R", Imf::Channel(Imf::FLOAT));
            header->channels().insert("G", Imf::Channel(Imf::FLOAT));
            header->channels().insert("B", Imf::Channel(Imf::FLOAT));
            header->channels().insert("A", Imf::Channel(Imf::FLOAT));
            numScanLines = obj.height();
        }

        void apply(const vsg::uivec4Array2D& obj) override
        { //single precision uint
            header.reset(new Imf::Header(obj.width(), obj.height()));
            header->channels().insert("R", Imf::Channel(Imf::UINT));
            header->channels().insert("G", Imf::Channel(Imf::UINT));
            header->channels().insert("B", Imf::Channel(Imf::UINT));
            header->channels().insert("A", Imf::Channel(Imf::UINT));
            numScanLines = obj.height();
        }
    };

    struct InitializeFrameBuffer : public vsg::ConstVisitor
    {
        Imf::FrameBuffer frameBuffer;

        void apply(const vsg::ushortArray2D& obj)
        { //single precision half float
            frameBuffer.insert("Y", Imf::Slice(Imf::HALF,
                                               (char*)obj.data(),
                                               sizeof(*obj.data()),
                                               sizeof(*obj.data()) * obj.width()));
        }

        void apply(const vsg::floatArray2D& obj) override
        { //single precision float
            frameBuffer.insert("Y", Imf::Slice(Imf::FLOAT,
                                               (char*)obj.data(),
                                               sizeof(*obj.data()),
                                               sizeof(*obj.data()) * obj.width()));
        }

        void apply(const vsg::uintArray2D& obj) override
        { //single precision uint
            frameBuffer.insert("Y", Imf::Slice(Imf::UINT,
                                               (char*)obj.data(),
                                               sizeof(*obj.data()),
                                               sizeof(*obj.data()) * obj.width()));
        }

        void apply(const vsg::usvec4Array2D& obj) override
        { //single precision short
            frameBuffer.insert("R", Imf::Slice(Imf::HALF,
                                               (char*)(&obj.data()->r),
                                               sizeof(*obj.data()),
                                               sizeof(*obj.data()) * obj.width()));
            frameBuffer.insert("G", Imf::Slice(Imf::HALF,
                                               (char*)(&obj.data()->g),
                                               sizeof(*obj.data()),
                                               sizeof(*obj.data()) * obj.width()));
            frameBuffer.insert("B", Imf::Slice(Imf::HALF,
                                               (char*)(&obj.data()->b),
                                               sizeof(*obj.data()),
                                               sizeof(*obj.data()) * obj.width()));
            frameBuffer.insert("A", Imf::Slice(Imf::HALF,
                                               (char*)(&obj.data()->a),
                                               sizeof(*obj.data()),
                                               sizeof(*obj.data()) * obj.width()));
        }

        void apply(const vsg::vec4Array2D& obj) override
        { //single precision float
            frameBuffer.insert("R", Imf::Slice(Imf::FLOAT,
                                               (char*)(&obj.data()->r),
                                               sizeof(*obj.data()),
                                               sizeof(*obj.data()) * obj.width()));
            frameBuffer.insert("G", Imf::Slice(Imf::FLOAT,
                                               (char*)(&obj.data()->g),
                                               sizeof(*obj.data()),
                                               sizeof(*obj.data()) * obj.width()));
            frameBuffer.insert("B", Imf::Slice(Imf::FLOAT,
                                               (char*)(&obj.data()->b),
                                               sizeof(*obj.data()),
                                               sizeof(*obj.data()) * obj.width()));
            frameBuffer.insert("A", Imf::Slice(Imf::FLOAT,
                                               (char*)(&obj.data()->a),
                                               sizeof(*obj.data()),
                                               sizeof(*obj.data()) * obj.width()));
        }

        void apply(const vsg::uivec4Array2D& obj) override
        { //single precision uint
            frameBuffer.insert("R", Imf::Slice(Imf::UINT,
                                               (char*)(&obj.data()->r),
                                               sizeof(*obj.data()),
                                               sizeof(*obj.data()) * obj.width()));
            frameBuffer.insert("G", Imf::Slice(Imf::UINT,
                                               (char*)(&obj.data()->g),
                                               sizeof(*obj.data()),
                                               sizeof(*obj.data()) * obj.width()));
            frameBuffer.insert("B", Imf::Slice(Imf::UINT,
                                               (char*)(&obj.data()->b),
                                               sizeof(*obj.data()),
                                               sizeof(*obj.data()) * obj.width()));
            frameBuffer.insert("A", Imf::Slice(Imf::UINT,
                                               (char*)(&obj.data()->a),
                                               sizeof(*obj.data()),
                                               sizeof(*obj.data()) * obj.width()));
        }
    };

} // end of namespace

openexr::openexr() :
    _supportedExtensions{".exr"}
{
}

vsg::ref_ptr<vsg::Object> openexr::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    if (const auto ext = vsg::lowerCaseFileExtension(filename); _supportedExtensions.count(ext) == 0)
    {
        return {};
    }

    vsg::Path filenameToUse = findFile(filename, options);
    if (!filenameToUse) return {};

    Imf::InputFile file(filenameToUse.c_str());

    return parseOpenExr(file);
}

vsg::ref_ptr<vsg::Object> openexr::read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!options || _supportedExtensions.count(options->extensionHint) == 0)
    {
        return {};
    }

    CPP_IStream stream(fin, "");
    Imf::InputFile file(stream);

    return parseOpenExr(file);
}

vsg::ref_ptr<vsg::Object> openexr::read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!options || _supportedExtensions.count(options->extensionHint) == 0)
    {
        return {};
    }

    Array_IStream stream(ptr, size, "");
    Imf::InputFile file(stream);

    return parseOpenExr(file);
}

bool openexr::write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options>) const
{
    if (const auto ext = vsg::lowerCaseFileExtension(filename); _supportedExtensions.count(ext) == 0)
    {
        return false;
    }

    auto v = vsg::visit<InitializeHeader>(object);
    if (v.header)
    {
        Imf::OutputFile file(filename.c_str(), *v.header);
        file.setFrameBuffer(vsg::visit<InitializeFrameBuffer>(object).frameBuffer);
        file.writePixels(v.numScanLines);

        return true;
    }
    return false;
}

bool openexr::write(const vsg::Object* object, std::ostream& fout, vsg::ref_ptr<const vsg::Options> options) const
{
    // TODO need to check the options extenion hint?

    auto v = vsg::visit<InitializeHeader>(object);
    if (v.header)
    {
        CPP_OStream stream(fout, "");
        Imf::OutputFile file(stream, *v.header);
        file.setFrameBuffer(vsg::visit<InitializeFrameBuffer>(object).frameBuffer);
        file.writePixels(v.numScanLines);

        return true;
    }
    return false;
}

bool openexr::getFeatures(Features& features) const
{
    for (auto& ext : _supportedExtensions)
    {
        features.extensionFeatureMap[ext] = static_cast<vsg::ReaderWriter::FeatureMask>(vsg::ReaderWriter::READ_FILENAME | vsg::ReaderWriter::READ_ISTREAM | vsg::ReaderWriter::READ_MEMORY | vsg::ReaderWriter::WRITE_FILENAME | vsg::ReaderWriter::WRITE_OSTREAM);
    }
    return true;
}
