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
    /// Composite ReaderWriter that holds the uses load 3rd party images formats.
    /// By defalt utilizes the stbi, dds and ktx ReaderWriters so that users only need to create vsgXchange::images::create() to utilize them all.
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

        bool getFeatures(Features& features) const override;

    private:
        std::unordered_set<std::string> _supportedExtensions;
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
        std::unordered_set<std::string> _supportedExtensions;
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
        std::unordered_set<std::string> _supportedExtensions;
    };

    /// optional GDAL ReaderWriter
    class VSGXCHANGE_DECLSPEC GDAL : public vsg::Inherit<vsg::ReaderWriter, GDAL>
    {
    public:
        GDAL();
        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const override;

        bool getFeatures(Features& features) const override;

    protected:
        ~GDAL();

        class Implementation;
        Implementation* _implementation;
    };

} // namespace vsgXchange

EVSG_type_name(vsgXchange::images);
EVSG_type_name(vsgXchange::stbi);
EVSG_type_name(vsgXchange::dds);
EVSG_type_name(vsgXchange::ktx);
EVSG_type_name(vsgXchange::GDAL);
