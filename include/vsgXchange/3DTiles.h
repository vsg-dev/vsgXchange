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

#include <vsg/threading/OperationThreads.h>

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
        vsg::ref_ptr<vsg::Object> read_tiles(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const;

        vsg::Logger::Level level = vsg::Logger::LOGGER_WARN;

        bool supportedExtension(const vsg::Path& ext) const;

        bool getFeatures(Features& features) const override;

        static constexpr const char* report = "report";                 /// bool, report parsed glTF to console, defaults to false
        static constexpr const char* instancing = "instancing";         /// bool, hint for using vsg::InstanceNode/InstanceDraw for instancing where possible.
        static constexpr const char* pixel_ratio = "pixel_ratio";       /// double, sets the SceneGraphBuilder::pixelErrorToScreenHeightRatio value used for setting LOD ranges.
        static constexpr const char* pre_load_level = "pre_load_level"; /// uint, sets the SceneGraphBuilder::preLoadLevel values to control what LOD level are pre loaded when reading a tileset.

        bool readOptions(vsg::Options& options, vsg::CommandLine& arguments) const override;

    public:
        /// https://github.com/CesiumGS/3d-tiles/blob/1.0/specification/schema/boundingVolume.schema.json
        struct VSGXCHANGE_DECLSPEC BoundingVolume : public vsg::Inherit<gltf::ExtensionsExtras, BoundingVolume>
        {
            vsg::ValuesSchema<double> box;
            vsg::ValuesSchema<double> region;
            vsg::ValuesSchema<double> sphere;

            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;

            void report(vsg::LogOutput& output);
        };

        /// https://github.com/CesiumGS/3d-tiles/blob/1.0/specification/schema/tile.content.schema.json
        struct VSGXCHANGE_DECLSPEC Content : public vsg::Inherit<gltf::ExtensionsExtras, Content>
        {
            vsg::ref_ptr<BoundingVolume> boundingVolume;
            std::string uri;

            // loaded from uri
            vsg::ref_ptr<vsg::Object> object;

            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_string(vsg::JSONParser& parser, const std::string_view& property) override;

            void report(vsg::LogOutput& output);
        };

        /// https://github.com/CesiumGS/3d-tiles/blob/1.0/specification/schema/tile.schema.json
        struct VSGXCHANGE_DECLSPEC Tile : public vsg::Inherit<gltf::ExtensionsExtras, Tile>
        {
            vsg::ref_ptr<BoundingVolume> boundingVolume;
            vsg::ref_ptr<BoundingVolume> viewerRequestVolume;
            double geometricError = 0.0;
            std::string refine;
            vsg::ValuesSchema<double> transform;
            vsg::ObjectsSchema<Tile> children;
            vsg::ref_ptr<Content> content;

            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
            void read_string(vsg::JSONParser& parser, const std::string_view& property) override;

            void report(vsg::LogOutput& output);
        };

        /// https://github.com/CesiumGS/3d-tiles/blob/1.0/specification/schema/properties.schema.json
        struct VSGXCHANGE_DECLSPEC PropertyRange : public vsg::Inherit<gltf::ExtensionsExtras, PropertyRange>
        {
            double minimum = 0.0;
            double maximum = 0.0;

            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        struct VSGXCHANGE_DECLSPEC Properties : public vsg::Inherit<gltf::ExtensionsExtras, Properties>
        {
            std::map<std::string, PropertyRange> properties;

            void read_object(vsg::JSONParser& parser, const std::string_view& property_name) override;

            void report(vsg::LogOutput& output);
        };

        /// https://github.com/CesiumGS/3d-tiles/blob/main/specification/schema/asset.schema.json
        struct VSGXCHANGE_DECLSPEC Asset : public vsg::Inherit<gltf::ExtensionsExtras, Asset>
        {
            std::string version;
            std::string tilesetVersion;

            std::map<std::string, std::string> strings;
            std::map<std::string, double> numbers;

            void read_string(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
            void report(vsg::LogOutput& output);
        };

        /// https://github.com/CesiumGS/3d-tiles/blob/main/specification/schema/tileset.schema.json
        struct VSGXCHANGE_DECLSPEC Tileset : public vsg::Inherit<gltf::ExtensionsExtras, Tileset>
        {
            vsg::ref_ptr<Asset> asset;
            vsg::ref_ptr<Properties> properties;
            vsg::ref_ptr<Tile> root;
            double geometricError = 0.0;
            vsg::StringsSchema extensionsUsed;
            vsg::StringsSchema extensionsRequired;

            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;

            void report(vsg::LogOutput& output);

            virtual void resolveURIs(vsg::ref_ptr<const vsg::Options> options);
        };

        /// Template class for reading an array of values from JSON or from a binary block
        template<typename T>
        struct ArraySchema : public Inherit<vsg::JSONParser::Schema, ArraySchema<T>>
        {
            const uint32_t invalidOffset = std::numeric_limits<uint32_t>::max();
            uint32_t byteOffset = invalidOffset;
            std::vector<T> values;

            void read_number(vsg::JSONParser&, std::istream& input) override
            {
                T value;
                input >> value;
                values.push_back(value);
            }

            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override
            {
                if (property == "byteOffset")
                    input >> byteOffset;
                else
                    parser.warning();
            }

            void assign(vsg::ubyteArray& binary, uint32_t count)
            {
                if (!values.empty() || byteOffset == invalidOffset) return;

                T* ptr = reinterpret_cast<T*>(binary.data() + byteOffset);
                for (uint32_t i = 0; i < count; ++i)
                {
                    values.push_back(*(ptr++));
                }
            }

            explicit operator bool() const noexcept { return !values.empty(); }
        };

        struct VSGXCHANGE_DECLSPEC b3dm_FeatureTable : public vsg::Inherit<gltf::ExtensionsExtras, b3dm_FeatureTable>
        {
            // storage for binary section
            vsg::ref_ptr<vsg::ubyteArray> binary;

            uint32_t BATCH_LENGTH = 0;
            ArraySchema<float> RTC_CENTER;
            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;

            void convert();

            void report(vsg::LogOutput& output);
        };

        struct BatchTable;

        struct VSGXCHANGE_DECLSPEC Batch : public vsg::Inherit<vsg::JSONtoMetaDataSchema, Batch>
        {
            uint32_t byteOffset = 0;
            std::string componentType;
            std::string type;

            void convert(BatchTable& batchTable);

            // read array parts
            void read_number(vsg::JSONParser& parser, std::istream& input) override;

            // read object parts
            void read_string(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        struct VSGXCHANGE_DECLSPEC BatchTable : public vsg::Inherit<gltf::ExtensionsExtras, BatchTable>
        {
            std::map<std::string, vsg::ref_ptr<Batch>> batches;

            uint32_t length = 0;
            vsg::ref_ptr<vsg::ubyteArray> binary;

            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;

            void convert();

            void report(vsg::LogOutput& output);
        };

        // https://github.com/CesiumGS/3d-tiles/blob/main/specification/TileFormats/Instanced3DModel/README.adoc
        struct VSGXCHANGE_DECLSPEC i3dm_FeatureTable : public vsg::Inherit<gltf::ExtensionsExtras, i3dm_FeatureTable>
        {
            // storage for binary section
            vsg::ref_ptr<vsg::ubyteArray> binary;

            // Instance sematics
            ArraySchema<float> POSITION;
            ArraySchema<uint16_t> POSITION_QUANTIZED;
            ArraySchema<float> NORMAL_UP;
            ArraySchema<float> NORMAL_RIGHT;
            ArraySchema<uint16_t> NORMAL_UP_OCT32P;
            ArraySchema<uint16_t> NORMAL_RIGHT_OCT32P;
            ArraySchema<float> SCALE;
            ArraySchema<float> SCALE_NON_UNIFORM;
            ArraySchema<uint32_t> BATCH_ID;

            // Global sematics
            uint32_t INSTANCES_LENGTH = 0;
            ArraySchema<float> RTC_CENTER;
            ArraySchema<float> QUANTIZED_VOLUME_OFFSET;
            ArraySchema<float> QUANTIZED_VOLUME_SCALE;
            bool EAST_NORTH_UP = false;

            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
            void read_bool(vsg::JSONParser& parser, const std::string_view& property, bool value) override;

            void convert();

            void report(vsg::LogOutput& output);
        };

    public:
        class VSGXCHANGE_DECLSPEC SceneGraphBuilder : public vsg::Inherit<vsg::Object, SceneGraphBuilder>
        {
        public:
            SceneGraphBuilder();

            vsg::ref_ptr<vsg::Options> options;
            vsg::ref_ptr<vsg::ShaderSet> shaderSet;
            vsg::ref_ptr<vsg::SharedObjects> sharedObjects;
            vsg::ref_ptr<vsg::OperationThreads> operationThreads;

            vsg::ref_ptr<vsg::EllipsoidModel> ellipsoidModel = vsg::EllipsoidModel::create();
            vsg::CoordinateConvention source_coordinateConvention = vsg::CoordinateConvention::Y_UP;
            double pixelErrorToScreenHeightRatio = 0.016; // 0.016 looks to replicate vsgCs worldviewer transition distances
            uint32_t preLoadLevel = 1;

            virtual void assignResourceHints(vsg::ref_ptr<vsg::Node> node);

            virtual vsg::dmat4 createMatrix(const std::vector<double>& values);
            virtual vsg::dsphere createBound(vsg::ref_ptr<BoundingVolume> boundingVolume);
            virtual vsg::ref_ptr<vsg::Node> readTileChildren(vsg::ref_ptr<Tiles3D::Tile> tile, uint32_t level, const std::string& inherited_refine);
            virtual vsg::ref_ptr<vsg::Node> createTile(vsg::ref_ptr<Tiles3D::Tile> tile, uint32_t level, const std::string& inherited_refine);
            virtual vsg::ref_ptr<vsg::Object> createSceneGraph(vsg::ref_ptr<Tiles3D::Tileset> tileset, vsg::ref_ptr<const vsg::Options> in_options);
        };
    };

} // namespace vsgXchange

EVSG_type_name(vsgXchange::Tiles3D)
