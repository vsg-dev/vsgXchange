/* <editor-fold desc="MIT License">

Copyright(c) 2021 Josef Stumpfegger, (C) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/FileSystem.h>
#include <vsg/io/stream.h>
#include <vsgXchange/images.h>

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
        virtual uint64_t tellg()
        {
            return _file.tellg();
        };
        virtual void seekg(uint64_t pos)
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
        virtual uint64_t tellp()
        {
            return _file.tellp();
        };
        virtual void seekp(uint64_t pos)
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
        virtual uint64_t tellg()
        {
            return curPlace;
        };
        virtual void seekg(uint64_t pos)
        {
            curPlace = pos;
        };
        virtual void clear() {};

    private:
        const uint8_t* data;
        size_t size;
        size_t curPlace;
    };

    static vsg::ref_ptr<vsg::Object> parseOpenExr(Imf::InputFile& file)
    {
        Imath::Box2i dw = file.header().dataWindow();

        int max_valid_value = 32768;
        int width = dw.max.x - dw.min.x + 1;
        int height = dw.max.y - dw.min.y + 1;
        if (std::abs(dw.max.x) > max_valid_value || std::abs(dw.max.x) > max_valid_value || std::abs(width) > max_valid_value || std::abs(height) > max_valid_value)
        {
            return vsg::ReadError::create("OpenEXR dataWindow out of bounds");
        }

        int channelCount = 0;
        auto type = file.header().channels().begin().channel().type;
        for (auto itr = file.header().channels().begin(); itr != file.header().channels().end(); ++itr)
        {
            ++channelCount;
            if (type != itr.channel().type)
            {
                // types not consistent, only consistent type channels are supported so return a null ref_ptr<>.
                return {};
            }
        }

        if (channelCount == 1)
        {
            vsg::ref_ptr<vsg::Data> image;
            if (type == Imf::HALF)
            {
                // single element half precision float
                image = vsg::ushortArray2D::create(width, height, vsg::Data::Properties{VK_FORMAT_R16_SFLOAT});
            }
            else if (type == Imf::FLOAT)
            {
                // single element single precision float
                image = vsg::floatArray2D::create(width, height, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT});
            }
            else if (type == Imf::UINT)
            {
                // single element single precision uint
                image = vsg::uintArray2D::create(width, height, vsg::Data::Properties{VK_FORMAT_R32_UINT});
            }

            if (image)
            {
                auto valueSize = image->valueSize();
                auto name = file.header().channels().begin().name();

                Imf::FrameBuffer frameBuffer;
                frameBuffer.insert(name, Imf::Slice(type, reinterpret_cast<char*>(image->dataPointer()) - valueSize * (dw.min.x + dw.min.y * width), valueSize, valueSize * width));

                file.setFrameBuffer(frameBuffer);
                file.readPixels(dw.min.y, dw.max.y);

                return image;
            }
        }
        else if (channelCount == 2)
        {
            vsg::ref_ptr<vsg::Data> image;
            size_t componentSize = 0;
            if (type == Imf::HALF)
            {
                // single element half precision float
                image = vsg::usvec2Array2D::create(width, height, vsg::Data::Properties{VK_FORMAT_R16G16_SFLOAT});
                componentSize = 2;
            }
            else if (type == Imf::FLOAT)
            {
                // single element single precision float
                image = vsg::vec2Array2D::create(width, height, vsg::Data::Properties{VK_FORMAT_R32G32_SFLOAT});
                componentSize = 4;
            }
            else if (type == Imf::UINT)
            {
                // single element single precision uint
                image = vsg::uivec2Array2D::create(width, height, vsg::Data::Properties{VK_FORMAT_R32G32_UINT});
                componentSize = 4;
            }

            if (image)
            {
                auto valueSize = image->valueSize();
                char* ptr = reinterpret_cast<char*>(image->dataPointer()) - valueSize * (dw.min.x + dw.min.y * width);

                Imf::FrameBuffer frameBuffer;
                frameBuffer.insert("R", Imf::Slice(type, ptr + 0 * componentSize, valueSize, valueSize * width));
                frameBuffer.insert("G", Imf::Slice(type, ptr + 1 * componentSize, valueSize, valueSize * width));

                file.setFrameBuffer(frameBuffer);
                file.readPixels(dw.min.y, dw.max.y);

                return image;
            }
        }
        else if (channelCount == 3)
        {
            vsg::ref_ptr<vsg::Data> image;
            size_t componentSize = 0;
            if (type == Imf::HALF)
            {
                // single element half precision float
                image = vsg::usvec3Array2D::create(width, height, vsg::Data::Properties{VK_FORMAT_R16G16B16_SFLOAT});
                componentSize = 2;
            }
            else if (type == Imf::FLOAT)
            {
                // single element single precision float
                image = vsg::vec3Array2D::create(width, height, vsg::Data::Properties{VK_FORMAT_R32G32B32_SFLOAT});
                componentSize = 4;
            }
            else if (type == Imf::UINT)
            {
                // single element single precision uint
                image = vsg::uivec3Array2D::create(width, height, vsg::Data::Properties{VK_FORMAT_R32G32B32_UINT});
                componentSize = 4;
            }

            if (image)
            {
                auto valueSize = image->valueSize();
                char* ptr = reinterpret_cast<char*>(image->dataPointer()) - valueSize * (dw.min.x + dw.min.y * width);

                Imf::FrameBuffer frameBuffer;
                frameBuffer.insert("R", Imf::Slice(type, ptr + 0 * componentSize, valueSize, valueSize * width));
                frameBuffer.insert("G", Imf::Slice(type, ptr + 1 * componentSize, valueSize, valueSize * width));
                frameBuffer.insert("B", Imf::Slice(type, ptr + 2 * componentSize, valueSize, valueSize * width));

                file.setFrameBuffer(frameBuffer);
                file.readPixels(dw.min.y, dw.max.y);

                return image;
            }
        }
        else if (channelCount == 4)
        {
            vsg::ref_ptr<vsg::Data> image;
            size_t componentSize = 0;
            if (type == Imf::HALF)
            {
                // single element half precision float
                image = vsg::usvec4Array2D::create(width, height, vsg::Data::Properties{VK_FORMAT_R16G16B16A16_SFLOAT});
                componentSize = 2;
            }
            else if (type == Imf::FLOAT)
            {
                // single element single precision float
                image = vsg::vec4Array2D::create(width, height, vsg::Data::Properties{VK_FORMAT_R32G32B32A32_SFLOAT});
                componentSize = 4;
            }
            else if (type == Imf::UINT)
            {
                // single element single precision uint
                image = vsg::uivec4Array2D::create(width, height, vsg::Data::Properties{VK_FORMAT_R32G32B32A32_UINT});
                componentSize = 4;
            }

            if (image)
            {
                auto valueSize = image->valueSize();
                char* ptr = reinterpret_cast<char*>(image->dataPointer()) - valueSize * (dw.min.x + dw.min.y * width);

                Imf::FrameBuffer frameBuffer;
                frameBuffer.insert("R", Imf::Slice(type, ptr + 0 * componentSize, valueSize, valueSize * width));
                frameBuffer.insert("G", Imf::Slice(type, ptr + 1 * componentSize, valueSize, valueSize * width));
                frameBuffer.insert("B", Imf::Slice(type, ptr + 2 * componentSize, valueSize, valueSize * width));
                frameBuffer.insert("A", Imf::Slice(type, ptr + 3 * componentSize, valueSize, valueSize * width));

                file.setFrameBuffer(frameBuffer);

                file.readPixels(dw.min.y, dw.max.y);

                return image;
            }
        }
        else
        {
            std::cout << "Unsupported channelCount = " << channelCount << std::endl;
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

        void apply(const vsg::usvec3Array2D& obj) override
        { //single precision short
            header.reset(new Imf::Header(obj.width(), obj.height()));
            header->channels().insert("R", Imf::Channel(Imf::HALF));
            header->channels().insert("G", Imf::Channel(Imf::HALF));
            header->channels().insert("B", Imf::Channel(Imf::HALF));
            numScanLines = obj.height();
        }

        void apply(const vsg::vec3Array2D& obj) override
        { //single precision float
            header.reset(new Imf::Header(obj.width(), obj.height()));
            header->channels().insert("R", Imf::Channel(Imf::FLOAT));
            header->channels().insert("G", Imf::Channel(Imf::FLOAT));
            header->channels().insert("B", Imf::Channel(Imf::FLOAT));
            numScanLines = obj.height();
        }

        void apply(const vsg::uivec3Array2D& obj) override
        { //single precision uint
            header.reset(new Imf::Header(obj.width(), obj.height()));
            header->channels().insert("R", Imf::Channel(Imf::UINT));
            header->channels().insert("G", Imf::Channel(Imf::UINT));
            header->channels().insert("B", Imf::Channel(Imf::UINT));
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
            frameBuffer.insert("Y", Imf::Slice(Imf::HALF, (char*)obj.dataPointer(), obj.valueSize(), obj.valueSize() * obj.width()));
        }

        void apply(const vsg::floatArray2D& obj) override
        { //single precision float
            frameBuffer.insert("Y", Imf::Slice(Imf::FLOAT, (char*)obj.dataPointer(), obj.valueSize(), obj.valueSize() * obj.width()));
        }

        void apply(const vsg::uintArray2D& obj) override
        { //single precision uint
            frameBuffer.insert("Y", Imf::Slice(Imf::UINT, (char*)obj.dataPointer(), obj.valueSize(), obj.valueSize() * obj.width()));
        }

        void apply(const vsg::usvec3Array2D& obj) override
        { //single precision short
            frameBuffer.insert("R", Imf::Slice(Imf::HALF, (char*)obj.dataPointer(), obj.valueSize(), obj.valueSize() * obj.width()));
            frameBuffer.insert("G", Imf::Slice(Imf::HALF, (char*)obj.dataPointer() + 2, obj.valueSize(), obj.valueSize() * obj.width()));
            frameBuffer.insert("B", Imf::Slice(Imf::HALF, (char*)obj.dataPointer() + 4, obj.valueSize(), obj.valueSize() * obj.width()));
        }

        void apply(const vsg::vec3Array2D& obj) override
        { //single precision float
            frameBuffer.insert("R", Imf::Slice(Imf::FLOAT, (char*)obj.dataPointer(), obj.valueSize(), obj.valueSize() * obj.width()));
            frameBuffer.insert("G", Imf::Slice(Imf::FLOAT, (char*)obj.dataPointer() + 4, obj.valueSize(), obj.valueSize() * obj.width()));
            frameBuffer.insert("B", Imf::Slice(Imf::FLOAT, (char*)obj.dataPointer() + 8, obj.valueSize(), obj.valueSize() * obj.width()));
        }

        void apply(const vsg::uivec3Array2D& obj) override
        { //single precision uint
            frameBuffer.insert("R", Imf::Slice(Imf::UINT, (char*)obj.dataPointer(), obj.valueSize(), obj.valueSize() * obj.width()));
            frameBuffer.insert("G", Imf::Slice(Imf::UINT, (char*)obj.dataPointer() + 4, obj.valueSize(), obj.valueSize() * obj.width()));
            frameBuffer.insert("B", Imf::Slice(Imf::UINT, (char*)obj.dataPointer() + 8, obj.valueSize(), obj.valueSize() * obj.width()));
        }

        void apply(const vsg::usvec4Array2D& obj) override
        { //single precision short
            frameBuffer.insert("R", Imf::Slice(Imf::HALF, (char*)obj.dataPointer(), obj.valueSize(), obj.valueSize() * obj.width()));
            frameBuffer.insert("G", Imf::Slice(Imf::HALF, (char*)obj.dataPointer() + 2, obj.valueSize(), obj.valueSize() * obj.width()));
            frameBuffer.insert("B", Imf::Slice(Imf::HALF, (char*)obj.dataPointer() + 4, obj.valueSize(), obj.valueSize() * obj.width()));
            frameBuffer.insert("A", Imf::Slice(Imf::HALF, (char*)obj.dataPointer() + 6, obj.valueSize(), obj.valueSize() * obj.width()));
        }

        void apply(const vsg::vec4Array2D& obj) override
        { //single precision float
            frameBuffer.insert("R", Imf::Slice(Imf::FLOAT, (char*)obj.dataPointer(), obj.valueSize(), obj.valueSize() * obj.width()));
            frameBuffer.insert("G", Imf::Slice(Imf::FLOAT, (char*)obj.dataPointer() + 4, obj.valueSize(), obj.valueSize() * obj.width()));
            frameBuffer.insert("B", Imf::Slice(Imf::FLOAT, (char*)obj.dataPointer() + 8, obj.valueSize(), obj.valueSize() * obj.width()));
            frameBuffer.insert("A", Imf::Slice(Imf::FLOAT, (char*)obj.dataPointer() + 12, obj.valueSize(), obj.valueSize() * obj.width()));
        }

        void apply(const vsg::uivec4Array2D& obj) override
        { //single precision uint
            frameBuffer.insert("R", Imf::Slice(Imf::UINT, (char*)obj.dataPointer(), obj.valueSize(), obj.valueSize() * obj.width()));
            frameBuffer.insert("G", Imf::Slice(Imf::UINT, (char*)obj.dataPointer() + 4, obj.valueSize(), obj.valueSize() * obj.width()));
            frameBuffer.insert("B", Imf::Slice(Imf::UINT, (char*)obj.dataPointer() + 8, obj.valueSize(), obj.valueSize() * obj.width()));
            frameBuffer.insert("A", Imf::Slice(Imf::UINT, (char*)obj.dataPointer() + 12, obj.valueSize(), obj.valueSize() * obj.width()));
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

    try
    {
        vsg::Path filenameToUse = findFile(filename, options);
        if (!filenameToUse) return {};

        Imf::InputFile file(filenameToUse.string().c_str());

        return parseOpenExr(file);
    }
    catch (Iex::BaseExc& e)
    {
        return vsg::ReadError::create(e.what());
    }
    catch (...)
    {
        return vsg::ReadError::create("Caught OpenEXR exception.");
    }
}

vsg::ref_ptr<vsg::Object> openexr::read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!vsg::compatibleExtension(options, _supportedExtensions)) return {};

    try
    {
        CPP_IStream stream(fin, "");
        Imf::InputFile file(stream);

        return parseOpenExr(file);
    }
    catch (Iex::BaseExc& e)
    {
        return vsg::ReadError::create(e.what());
    }
    catch (...)
    {
        return vsg::ReadError::create("Caught OpenEXR exception.");
    }
}

vsg::ref_ptr<vsg::Object> openexr::read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!vsg::compatibleExtension(options, _supportedExtensions)) return {};

    try
    {
        Array_IStream stream(ptr, size, "");
        Imf::InputFile file(stream);

        return parseOpenExr(file);
    }
    catch (Iex::BaseExc& e)
    {
        return vsg::ReadError::create(e.what());
    }
    catch (...)
    {
        return vsg::ReadError::create("Caught OpenEXR exception.");
    }
}

bool openexr::write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!compatibleExtension(filename, options, _supportedExtensions)) return {};

    try
    {
        auto v = vsg::visit<InitializeHeader>(object);
        if (v.header)
        {
            Imf::OutputFile file(filename.string().c_str(), *v.header);
            file.setFrameBuffer(vsg::visit<InitializeFrameBuffer>(object).frameBuffer);
            file.writePixels(v.numScanLines);

            return true;
        }
    }
    catch (Iex::BaseExc& e)
    {
        std::cout << "Caught OpenEXR exception: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "Caught OpenEXR exception." << std::endl;
    }
    return false;
}

bool openexr::write(const vsg::Object* object, std::ostream& fout, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!vsg::compatibleExtension(options, _supportedExtensions)) return {};

    try
    {
        auto v = vsg::visit<InitializeHeader>(object);
        if (v.header)
        {
            CPP_OStream stream(fout, "");
            Imf::OutputFile file(stream, *v.header);
            file.setFrameBuffer(vsg::visit<InitializeFrameBuffer>(object).frameBuffer);
            file.writePixels(v.numScanLines);

            return true;
        }
    }
    catch (Iex::BaseExc& e)
    {
        std::cout << "Caught OpenEXR exception: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "Caught OpenEXR exception." << std::endl;
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
