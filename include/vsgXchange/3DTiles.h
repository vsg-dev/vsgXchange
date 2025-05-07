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

    /// 3DTiles ReaderWriter, C++ won't handle class called 3DTiles so make do with Tiles3D.
    /// Specs for 3DTiles https://github.com/CesiumGS/3d-tiles
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

        /// Template class for reading an array of values from JSON or from a binary block
        template<typename T, int C>
        struct ArraySchema : public Inherit<vsg::JSONParser::Schema, ArraySchema<T, C>>
        {
            const uint32_t count = C;
            std::vector<T> values;

            void read_number(vsg::JSONParser&, std::istream& input) override
            {
                T value;
                input >> value;
                values.push_back(value);
            }

            uint32_t byteOffset = 0;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override
            {
                if (property=="byteOffset") input >> byteOffset;
                else parser.warning();
            }

            void read_and_assign(vsg::JSONParser& parser, vsg::ubyteArray& binary)
            {
                parser.read_object(*this);

                T* ptr = reinterpret_cast<T*>(binary.data() + byteOffset);
                for(uint32_t i=0; i<count; ++i)
                {
                    values.push_back(*(ptr++));
                }
            }
        };


        struct VSGXCHANGE_DECLSPEC b3dm_FeatureTable : public vsg::Inherit<gltf::ExtensionsExtras, b3dm_FeatureTable>
        {
            // storage for binary section
            vsg::ref_ptr<vsg::ubyteArray> binary;

            uint32_t BATCH_LENGTH = 0;
            ArraySchema<double, 3> RTC_CENTER;
            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        class BatchTable;

        struct VSGXCHANGE_DECLSPEC Batch : public vsg::Inherit<vsg::JSONtoMetaDataSchema, Batch>
        {
            uint32_t byteOffset = 0;
            std::string componentType;
            std::string type;

            void convert(BatchTable& batchTable);

            // read object parts
            void read_string(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        struct VSGXCHANGE_DECLSPEC BatchTable : public vsg::Inherit<gltf::ExtensionsExtras, BatchTable>
        {
            std::map<std::string, vsg::ref_ptr<Batch>> batches;

            uint32_t length = 0;
            vsg::ref_ptr<vsg::ubyteArray> buffer;

            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;

            void convert();

            void report();
        };

        struct VSGXCHANGE_DECLSPEC i3dm_FeatureTable : public vsg::Inherit<gltf::ExtensionsExtras, i3dm_FeatureTable>
        {
            // storage for binary section
            vsg::ref_ptr<vsg::ubyteArray> binary;

            // Instance sematics
            ArraySchema<float, 3> POSITION;
            ArraySchema<uint16_t, 3> POSITION_QUANTIZED;
            ArraySchema<float, 3> NORMAL_UP;
            ArraySchema<float, 3> NORMAL_RIGHT;
            ArraySchema<uint16_t, 2> NORMAL_UP_OCT32P;
            ArraySchema<uint16_t, 2> NORMAL_RIGHT_OCT32P;
            ArraySchema<float, 3> SCALE;
            ArraySchema<float, 3> SCALE_NON_UNIFORM;
            uint32_t BATCH_ID = 0;

            // Global sematics
            uint32_t INSTANCES_LENGTH = 0;
            ArraySchema<float, 3> RTC_CENTER;
            ArraySchema<float, 3> QUANTIZED_VOLUME_OFFSET;
            ArraySchema<float, 3> QUANTIZED_VOLUME_SCALE;
            bool EAST_NORTH_UP = false;

            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
            void read_bool(vsg::JSONParser& parser, const std::string_view& property, bool value) override;
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
