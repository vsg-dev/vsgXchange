#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2025 Robert Osfield

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

#include <vsgXchange/gltf.h>

namespace vsgXchange
{

    /// 3d-tiles ReaderWriter, C++ won't handle class called 3d-tiles so make do with Tiles3D
    /// specs for 3D-Tiles https://github.com/CesiumGS/3d-tiles
    class VSGXCHANGE_DECLSPEC Tiles3D : public vsg::Inherit<vsg::ReaderWriter, Tiles3D>
    {
    public:
        Tiles3D();

        vsg::ref_ptr<vsg::Object> read(const vsg::Path&, vsg::ref_ptr<const vsg::Options>) const override;
        vsg::ref_ptr<vsg::Object> read(std::istream&, vsg::ref_ptr<const vsg::Options>) const override;
        vsg::ref_ptr<vsg::Object> read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options = {}) const override;

        vsg::ref_ptr<vsg::Object> read_json(std::istream&, vsg::ref_ptr<const vsg::Options>, const vsg::Path& filename = {}) const;
        vsg::ref_ptr<vsg::Object> read_b3dm(std::istream&, vsg::ref_ptr<const vsg::Options>, const vsg::Path& filename = {}) const;
        vsg::ref_ptr<vsg::Object> read_cmpt(std::istream&, vsg::ref_ptr<const vsg::Options>, const vsg::Path& filename = {}) const;
        vsg::ref_ptr<vsg::Object> read_i3dm(std::istream&, vsg::ref_ptr<const vsg::Options>, const vsg::Path& filename = {}) const;
        vsg::ref_ptr<vsg::Object> read_pnts(std::istream&, vsg::ref_ptr<const vsg::Options>, const vsg::Path& filename = {}) const;

        vsg::Logger::Level level = vsg::Logger::LOGGER_WARN;

        bool supportedExtension(const vsg::Path& ext) const;

        bool getFeatures(Features& features) const override;

        static constexpr const char* report = "report";             /// bool, report parsed glTF to console, defaults to false

        bool readOptions(vsg::Options& options, vsg::CommandLine& arguments) const override;

    public:

        /// https://github.com/CesiumGS/3d-tiles/blob/main/specification/schema/tileset.schema.json
        struct VSGXCHANGE_DECLSPEC Tileset : public vsg::Inherit<gltf::ExtensionsExtras, Tileset>
        {
            // asset
            // properties
            // schema
            // schemaUri
            // statistics
            // group
            // metadata
            // geometricError
            // root
            // extensionsUsed
            // extensionsRequired

            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;

            void report();

            virtual void resolveURIs(vsg::ref_ptr<const vsg::Options> options);
        };

        struct VSGXCHANGE_DECLSPEC FeatureTable : public vsg::Inherit<gltf::ExtensionsExtras, FeatureTable>
        {
            uint32_t BATCH_LENGTH = 0;
            vsg::ValuesSchema<double> RTC_CENTER;
            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

    public:

        class VSGXCHANGE_DECLSPEC SceneGraphBuilder : public vsg::Inherit<vsg::Object, SceneGraphBuilder>
        {
        public:
            SceneGraphBuilder();

            vsg::ref_ptr<const vsg::Options> options;
            vsg::ref_ptr<vsg::ShaderSet> shaderSet;
            vsg::ref_ptr<vsg::SharedObjects> sharedObjects;

            vsg::ref_ptr<vsg::Object> createSceneGraph(vsg::ref_ptr<Tiles3D::Tileset> root, vsg::ref_ptr<const vsg::Options> options);
        };
    };

}

EVSG_type_name(vsgXchange::Tiles3D)
