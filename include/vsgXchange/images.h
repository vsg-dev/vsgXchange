#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2021 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shimages be included in images
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/ReaderWriter.h>
#include <vsgXchange/Version.h>

#include <memory>
#include <unordered_set>

namespace vsgXchange
{
    /// Composite ReaderWriter that holds the used 3rd party image format loaders.
    /// By default utilizes the stbi, dds and ktx ReaderWriters so that users only need to create vsgXchange::images::create() to utilize them all.
    class VSGXCHANGE_DECLSPEC images : public vsg::Inherit<vsg::CompositeReaderWriter, images>
    {
    public:
        images();
    };

    /// add png, jpeg and gif support using local build of stbi.
    class VSGXCHANGE_DECLSPEC stbi : public vsg::Inherit<vsg::ReaderWriter, stbi>
    {
    public:
        stbi();

        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;
        vsg::ref_ptr<vsg::Object> read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options = {}) const override;
        vsg::ref_ptr<vsg::Object> read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options = {}) const override;
        bool write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> = {}) const override;
        bool write(const vsg::Object* object, std::ostream& stream, vsg::ref_ptr<const vsg::Options> = {}) const override;

        bool getFeatures(Features& features) const override;

        // vsg::Options::setValue(str, value) supported options:
        static constexpr const char* jpeg_quality = "jpeg_quality"; /// set the int quality value when writing out to image as jpeg file.
        static constexpr const char* image_format = "image_format"; /// Override read image format (8bit RGB/RGBA default to sRGB) to be specified class of CoordinateSpace (sRGB or LINEAR).

        bool readOptions(vsg::Options& options, vsg::CommandLine& arguments) const override;

    private:
        std::set<vsg::Path> _supportedExtensions;
    };

    /// add dds support using local build of tinydds.
    class VSGXCHANGE_DECLSPEC dds : public vsg::Inherit<vsg::ReaderWriter, dds>
    {
    public:
        dds();

        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;
        vsg::ref_ptr<vsg::Object> read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options = {}) const override;
        vsg::ref_ptr<vsg::Object> read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options = {}) const override;

        bool getFeatures(Features& features) const override;

    private:
        std::set<vsg::Path> _supportedExtensions;
    };

    /// add ktx using using local build of libktx
    class VSGXCHANGE_DECLSPEC ktx : public vsg::Inherit<vsg::ReaderWriter, ktx>
    {
    public:
        ktx();

        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;
        vsg::ref_ptr<vsg::Object> read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options = {}) const override;
        vsg::ref_ptr<vsg::Object> read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options = {}) const override;

        bool getFeatures(Features& features) const override;

    private:
        std::set<vsg::Path> _supportedExtensions;
    };

    /// optional .exr support using OpenEXR library
    class openexr : public vsg::Inherit<vsg::ReaderWriter, openexr>
    {
    public:
        openexr();

        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;
        vsg::ref_ptr<vsg::Object> read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options = {}) const override;
        vsg::ref_ptr<vsg::Object> read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options = {}) const override;

        bool write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> = {}) const override;
        bool write(const vsg::Object* object, std::ostream& fout, vsg::ref_ptr<const vsg::Options> = {}) const override;

        bool getFeatures(Features& features) const override;

    private:
        std::set<vsg::Path> _supportedExtensions;
    };

} // namespace vsgXchange

EVSG_type_name(vsgXchange::images);
EVSG_type_name(vsgXchange::stbi);
EVSG_type_name(vsgXchange::dds);
EVSG_type_name(vsgXchange::ktx);
EVSG_type_name(vsgXchange::openexr);
