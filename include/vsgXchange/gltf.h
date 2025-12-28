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

#include <vsg/animation/AnimationGroup.h>
#include <vsg/animation/JointSampler.h>
#include <vsg/app/Camera.h>
#include <vsg/io/JSONParser.h>
#include <vsg/io/ReaderWriter.h>
#include <vsg/lighting/Light.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsgXchange/Version.h>

namespace vsgXchange
{

    /// gltf ReaderWriter
    class VSGXCHANGE_DECLSPEC gltf : public vsg::Inherit<vsg::ReaderWriter, gltf>
    {
    public:
        gltf();

        vsg::ref_ptr<vsg::Object> read(const vsg::Path&, vsg::ref_ptr<const vsg::Options>) const override;
        vsg::ref_ptr<vsg::Object> read(std::istream&, vsg::ref_ptr<const vsg::Options>) const override;
        vsg::ref_ptr<vsg::Object> read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options = {}) const override;

        vsg::ref_ptr<vsg::Object> read_gltf(std::istream&, vsg::ref_ptr<const vsg::Options>, const vsg::Path& filename = {}) const;
        vsg::ref_ptr<vsg::Object> read_glb(std::istream&, vsg::ref_ptr<const vsg::Options>, const vsg::Path& filename = {}) const;

        vsg::Logger::Level level = vsg::Logger::LOGGER_WARN;

        bool supportedExtension(const vsg::Path& ext) const;

        bool getFeatures(Features& features) const override;

        static constexpr const char* report = "report";                   /// bool, report parsed glTF to console, defaults to false
        static constexpr const char* culling = "culling";                 /// bool, insert cull nodes, defaults to true
        static constexpr const char* disable_gltf = "disable_gltf";       /// bool, disable vsgXchange::gltf so vsgXchange::assimp will be used instead, defaults to false
        static constexpr const char* clone_accessors = "clone_accessors"; /// bool, hint to clone the data associated with accessors, defaults to false
        static constexpr const char* maxAnisotropy = "maxAnisotropy";     /// float, default setting of vsg::Sampler::maxAnisotropy to use.

        bool readOptions(vsg::Options& options, vsg::CommandLine& arguments) const override;

    public:
        struct glTFid
        {
            static const uint32_t invalid_value = std::numeric_limits<uint32_t>::max();

            uint32_t value = invalid_value;

            bool valid() const { return value != invalid_value; }

            explicit operator bool() const noexcept { return valid(); }
        };

        struct VSGXCHANGE_DECLSPEC Extensions : public vsg::Inherit<vsg::JSONtoMetaDataSchema, Extensions>
        {
            std::map<std::string, vsg::ref_ptr<vsg::JSONParser::Schema>> values;

            void report(vsg::LogOutput& output);

            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
        };

        using Extras = vsg::JSONtoMetaDataSchema;

        struct VSGXCHANGE_DECLSPEC ExtensionsExtras : public vsg::Inherit<vsg::JSONParser::Schema, ExtensionsExtras>
        {
            vsg::ref_ptr<Extensions> extensions;
            vsg::ref_ptr<Extras> extras;

            virtual bool requireMetaData() const { return extras.valid(); }

            void report(vsg::LogOutput& output);

            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;

            template<class T>
            vsg::ref_ptr<T> extension(const char* name) const
            {
                if (!extensions) return {};

                auto itr = extensions->values.find(name);
                if (itr != extensions->values.end())
                    return itr->second.cast<T>();
                else
                    return {};
            }
        };

        struct VSGXCHANGE_DECLSPEC NameExtensionsExtras : public vsg::Inherit<ExtensionsExtras, NameExtensionsExtras>
        {
            std::string name;

            bool requireMetaData() const override { return !name.empty() || extras.valid(); }

            void report(vsg::LogOutput& output);

            void read_string(vsg::JSONParser& parser, const std::string_view& property) override;
        };

        struct VSGXCHANGE_DECLSPEC SparseIndices : public vsg::Inherit<NameExtensionsExtras, SparseIndices>
        {
            glTFid bufferView;
            uint32_t byteOffset = 0;
            uint32_t componentType = 0;

            void report(vsg::LogOutput& output);
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        struct VSGXCHANGE_DECLSPEC SparseValues : public vsg::Inherit<NameExtensionsExtras, SparseValues>
        {
            glTFid bufferView;
            uint32_t byteOffset = 0;

            void report(vsg::LogOutput& output);
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        struct VSGXCHANGE_DECLSPEC Sparse : public vsg::Inherit<NameExtensionsExtras, Sparse>
        {
            uint32_t count = 0;
            vsg::ref_ptr<SparseIndices> indices;
            vsg::ref_ptr<SparseValues> values;

            void report(vsg::LogOutput& output);
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
        };

        enum ComponentType : uint32_t
        {
            COMPONENT_TYPE_UNDEFINED = 0,
            COMPONENT_TYPE_BYTE = 5120,
            COMPONENT_TYPE_UNSIGNED_BYTE = 5121,
            COMPONENT_TYPE_SHORT = 5122,
            COMPONENT_TYPE_UNSIGNED_SHORT = 5123,
            COMPONENT_TYPE_INT = 5124,
            COMPONENT_TYPE_UNSIGNED_INT = 5125,
            COMPONENT_TYPE_FLOAT = 5126,
            COMPONENT_TYPE_DOUBLE = 5130
        };

        struct DataProperties
        {
            uint32_t componentType = 0;
            uint32_t componentSize = 0;
            uint32_t componentCount = 0;
        };

        struct VSGXCHANGE_DECLSPEC Accessor : public vsg::Inherit<NameExtensionsExtras, Accessor>
        {
            glTFid bufferView;
            uint32_t byteOffset = 0;
            uint32_t componentType = 0;
            bool normalized = false;
            uint32_t count = 0;
            std::string type;
            vsg::ValuesSchema<double> max;
            vsg::ValuesSchema<double> min;
            vsg::ref_ptr<Sparse> sparse;

            DataProperties getDataProperties() const;

            void report(vsg::LogOutput& output);
            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_string(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
            void read_bool(vsg::JSONParser& parser, const std::string_view& property, bool value) override;
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
        };

        struct VSGXCHANGE_DECLSPEC Asset : public vsg::Inherit<ExtensionsExtras, Asset>
        {
            std::string copyright;
            std::string version;
            std::string generator;
            std::string minVersion;

            void report(vsg::LogOutput& output);
            void read_string(vsg::JSONParser& parser, const std::string_view& property) override;
        };

        struct VSGXCHANGE_DECLSPEC BufferView : public vsg::Inherit<NameExtensionsExtras, BufferView>
        {
            glTFid buffer;
            uint32_t byteOffset = 0;
            uint32_t byteLength = 0;
            uint32_t byteStride = 1;
            uint32_t target = 0;

            void report(vsg::LogOutput& output);
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        struct VSGXCHANGE_DECLSPEC Buffer : public vsg::Inherit<NameExtensionsExtras, Buffer>
        {
            std::string_view uri;
            uint32_t byteLength = 0;

            // loaded from uri
            vsg::ref_ptr<vsg::Data> data;

            void report(vsg::LogOutput& output);
            void read_string(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        struct VSGXCHANGE_DECLSPEC Image : public vsg::Inherit<NameExtensionsExtras, Image>
        {
            std::string_view uri;
            std::string mimeType;
            glTFid bufferView;

            // loaded from uri
            vsg::ref_ptr<vsg::Data> data;

            void report(vsg::LogOutput& output);
            void read_string(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        struct VSGXCHANGE_DECLSPEC TextureInfo : public vsg::Inherit<ExtensionsExtras, TextureInfo>
        {
            glTFid index;
            uint32_t texCoord = 0;

            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        // https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_texture_transform
        struct VSGXCHANGE_DECLSPEC KHR_texture_transform : public vsg::Inherit<vsg::JSONParser::Schema, KHR_texture_transform>
        {
            vsg::ValuesSchema<float> offset;
            float rotation = 0.0f;
            vsg::ValuesSchema<float> scale;
            uint32_t texCoord = 0;

            // extention prototype will be cloned when it's used.
            vsg::ref_ptr<vsg::Object> clone(const vsg::CopyOp&) const override { return KHR_texture_transform::create(*this); }

            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        struct VSGXCHANGE_DECLSPEC PbrMetallicRoughness : public vsg::Inherit<ExtensionsExtras, PbrMetallicRoughness>
        {
            vsg::ValuesSchema<float> baseColorFactor; // default { 1.0, 1.0, 1.0, 1.0 }
            TextureInfo baseColorTexture;
            float metallicFactor = 1.0f;
            float roughnessFactor = 1.0f;
            TextureInfo metallicRoughnessTexture;

            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        struct VSGXCHANGE_DECLSPEC NormalTextureInfo : public vsg::Inherit<TextureInfo, NormalTextureInfo>
        {
            float scale = 1.0f;

            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        struct VSGXCHANGE_DECLSPEC OcclusionTextureInfo : public vsg::Inherit<TextureInfo, OcclusionTextureInfo>
        {
            float strength = 1.0f;

            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        /// https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_specular
        struct VSGXCHANGE_DECLSPEC KHR_materials_specular : public vsg::Inherit<vsg::JSONParser::Schema, KHR_materials_specular>
        {
            float specularFactor = 1.0f;
            TextureInfo specularTexture;
            vsg::ValuesSchema<float> specularColorFactor;
            TextureInfo specularColorTexture;

            // extention prototype will be cloned when it's used.
            vsg::ref_ptr<vsg::Object> clone(const vsg::CopyOp&) const override { return KHR_materials_specular::create(*this); }

            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        /// https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Archived/KHR_materials_pbrSpecularGlossiness
        struct VSGXCHANGE_DECLSPEC KHR_materials_pbrSpecularGlossiness : public vsg::Inherit<ExtensionsExtras, KHR_materials_pbrSpecularGlossiness>
        {
            vsg::ValuesSchema<float> diffuseFactor;
            TextureInfo diffuseTexture;
            vsg::ValuesSchema<float> specularFactor;
            float glossinessFactor = 1.0f;
            TextureInfo specularGlossinessTexture;

            // extention prototype will be cloned when it's used.
            vsg::ref_ptr<vsg::Object> clone(const vsg::CopyOp&) const override { return KHR_materials_pbrSpecularGlossiness::create(*this); }

            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        // https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_emissive_strength
        struct VSGXCHANGE_DECLSPEC KHR_materials_emissive_strength : public vsg::Inherit<ExtensionsExtras, KHR_materials_emissive_strength>
        {
            float emissiveStrength = 1.0f;

            // extention prototype will be cloned when it's used.
            vsg::ref_ptr<vsg::Object> clone(const vsg::CopyOp&) const override { return KHR_materials_emissive_strength::create(*this); }

            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        /// index of refraction : https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_ior
        struct VSGXCHANGE_DECLSPEC KHR_materials_ior : public vsg::Inherit<vsg::JSONParser::Schema, KHR_materials_ior>
        {
            float ior = 1.5f;

            // extention prototype will be cloned when it's used.
            vsg::ref_ptr<vsg::Object> clone(const vsg::CopyOp&) const override { return KHR_materials_ior::create(*this); }

            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        // https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_unlit
        struct VSGXCHANGE_DECLSPEC KHR_materials_unlit : public vsg::Inherit<vsg::JSONParser::Schema, KHR_materials_unlit>
        {
            // extention prototype will be cloned when it's used.
            vsg::ref_ptr<vsg::Object> clone(const vsg::CopyOp&) const override { return KHR_materials_unlit::create(*this); }
        };

        // Extensions used in glTF-Sample-Assets/Models
        // https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_anisotropy
        // https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_clearcoat
        // https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_diffuse_transmission
        // https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_dispersion
        // https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_iridescence
        // https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_sheen
        // https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_transmission
        // https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_volume

        struct VSGXCHANGE_DECLSPEC Sampler : public vsg::Inherit<NameExtensionsExtras, Sampler>
        {
            uint32_t minFilter = 0;
            uint32_t magFilter = 0;
            uint32_t wrapS = 10497; // default REPEAT
            uint32_t wrapT = 10497; // default REPEAT
            uint32_t wrapR = 10497; // default REPEAT, not part of official glTF spec but occurs in select .gltf/.glb files.

            // extensions
            // extras

            void report(vsg::LogOutput& output);
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        struct VSGXCHANGE_DECLSPEC Texture : public vsg::Inherit<NameExtensionsExtras, Texture>
        {
            glTFid sampler;
            glTFid source;

            void report(vsg::LogOutput& output);
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        struct VSGXCHANGE_DECLSPEC Material : public vsg::Inherit<NameExtensionsExtras, Material>
        {
            PbrMetallicRoughness pbrMetallicRoughness;
            NormalTextureInfo normalTexture;
            OcclusionTextureInfo occlusionTexture;
            TextureInfo emissiveTexture;
            vsg::ValuesSchema<float> emissiveFactor; // default { 0.0, 0.0, 0.0 }
            std::string alphaMode = "OPAQUE";
            float alphaCutoff = 0.5f;
            bool doubleSided = false;

            void report(vsg::LogOutput& output);
            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_string(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
            void read_bool(vsg::JSONParser& parser, const std::string_view& property, bool value) override;
        };

        struct VSGXCHANGE_DECLSPEC Attributes : public vsg::Inherit<vsg::JSONParser::Schema, Attributes>
        {
            std::map<std::string, glTFid> values;

            void read_number(vsg::JSONParser&, const std::string_view& property, std::istream& input) override;
        };

        struct VSGXCHANGE_DECLSPEC Primitive : public vsg::Inherit<ExtensionsExtras, Primitive>
        {
            Attributes attributes;
            glTFid indices;
            glTFid material;
            uint32_t mode = 4;
            vsg::ObjectsSchema<Attributes> targets;

            void report(vsg::LogOutput& output);
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
        };

        struct VSGXCHANGE_DECLSPEC Mesh : public vsg::Inherit<NameExtensionsExtras, Mesh>
        {
            vsg::ObjectsSchema<Primitive> primitives;
            vsg::ValuesSchema<double> weights;

            void report(vsg::LogOutput& output);
            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
        };

        /// https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_draco_mesh_compression
        struct VSGXCHANGE_DECLSPEC KHR_draco_mesh_compression : public vsg::Inherit<ExtensionsExtras, KHR_draco_mesh_compression>
        {
            glTFid bufferView;
            Attributes attributes;

            // extention prototype will be cloned when it's used.
            vsg::ref_ptr<vsg::Object> clone(const vsg::CopyOp&) const override { return KHR_draco_mesh_compression::create(*this); }

            void report(vsg::LogOutput& output);
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        /// https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Vendor/EXT_mesh_gpu_instancing/README.md
        struct VSGXCHANGE_DECLSPEC EXT_mesh_gpu_instancing : public vsg::Inherit<ExtensionsExtras, EXT_mesh_gpu_instancing>
        {
            vsg::ref_ptr<Attributes> attributes;

            // extention prototype will be cloned when it's used.
            vsg::ref_ptr<vsg::Object> clone(const vsg::CopyOp&) const override { return EXT_mesh_gpu_instancing::create(*this); }

            void report(vsg::LogOutput& output);
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
        };

        struct VSGXCHANGE_DECLSPEC Node : public vsg::Inherit<NameExtensionsExtras, Node>
        {
            glTFid camera;
            glTFid skin;
            glTFid mesh;
            vsg::ValuesSchema<glTFid> children;
            vsg::ValuesSchema<double> matrix;
            vsg::ValuesSchema<double> rotation;
            vsg::ValuesSchema<double> scale;
            vsg::ValuesSchema<double> translation;
            vsg::ValuesSchema<double> weights;

            void report(vsg::LogOutput& output);
            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        struct VSGXCHANGE_DECLSPEC Scene : public vsg::Inherit<NameExtensionsExtras, Scene>
        {
            vsg::ValuesSchema<glTFid> nodes;

            void report(vsg::LogOutput& output);
            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
        };

        struct VSGXCHANGE_DECLSPEC AnimationTarget : public vsg::Inherit<ExtensionsExtras, AnimationTarget>
        {
            glTFid node;
            std::string path;

            void report(vsg::LogOutput& output);
            void read_string(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        struct VSGXCHANGE_DECLSPEC AnimationChannel : public vsg::Inherit<ExtensionsExtras, AnimationChannel>
        {
            glTFid sampler;
            AnimationTarget target;

            void report(vsg::LogOutput& output);
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
        };

        struct VSGXCHANGE_DECLSPEC AnimationSampler : public vsg::Inherit<ExtensionsExtras, AnimationSampler>
        {
            glTFid input;
            std::string interpolation;
            glTFid output;

            void report(vsg::LogOutput& output);
            void read_string(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        struct VSGXCHANGE_DECLSPEC Animation : public vsg::Inherit<NameExtensionsExtras, Animation>
        {
            vsg::ObjectsSchema<AnimationChannel> channels;
            vsg::ObjectsSchema<AnimationSampler> samplers;

            void report(vsg::LogOutput& output);
            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
        };

        struct VSGXCHANGE_DECLSPEC Skins : public vsg::Inherit<NameExtensionsExtras, Skins>
        {
            glTFid inverseBindMatrices;
            glTFid skeleton;
            vsg::ValuesSchema<glTFid> joints;

            void report(vsg::LogOutput& output);
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
        };

        struct VSGXCHANGE_DECLSPEC Orthographic : public vsg::Inherit<ExtensionsExtras, Orthographic>
        {
            double xmag = 1.0;
            double ymag = 1.0;
            double znear = 1.0;
            double zfar = 1000.0;

            void report(vsg::LogOutput& output);
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        struct VSGXCHANGE_DECLSPEC Perspective : public vsg::Inherit<ExtensionsExtras, Perspective>
        {
            double aspectRatio = 1.0;
            double yfov = 1.0;
            double znear = 1.0;
            double zfar = 1000.0;

            void report(vsg::LogOutput& output);
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        struct VSGXCHANGE_DECLSPEC Camera : public vsg::Inherit<NameExtensionsExtras, Camera>
        {
            vsg::ref_ptr<Orthographic> orthographic;
            vsg::ref_ptr<Perspective> perspective;
            std::string type;

            void report(vsg::LogOutput& output);
            void read_string(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
        };

        struct VSGXCHANGE_DECLSPEC Spot : public vsg::Inherit<ExtensionsExtras, Spot>
        {
            double innerConeAngle = 0.0;
            double outerConeAngle = 0.7853981633974483;

            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
        };

        struct VSGXCHANGE_DECLSPEC Light : public vsg::Inherit<NameExtensionsExtras, Light>
        {
            vsg::ValuesSchema<float> color;
            float intensity = 1.0f;
            vsg::ref_ptr<Spot> spot;
            std::string type;
            float range = std::numeric_limits<float>::max();

            void read_string(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
        };

        struct VSGXCHANGE_DECLSPEC KHR_lights_punctual : vsg::Inherit<ExtensionsExtras, KHR_lights_punctual>
        {
            vsg::ObjectsSchema<Light> lights;
            glTFid light;

            // extention prototype will be cloned when it's used.
            vsg::ref_ptr<vsg::Object> clone(const vsg::CopyOp&) const override { return KHR_lights_punctual::create(*this); }

            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;
            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
        };

        struct VSGXCHANGE_DECLSPEC glTF : public vsg::Inherit<ExtensionsExtras, glTF>
        {
            // filename that the glTF was read from.
            vsg::Path filename;

            // data structures read by the parser
            vsg::StringsSchema extensionsUsed;
            vsg::StringsSchema extensionsRequired;
            vsg::ref_ptr<Asset> asset;
            vsg::ObjectsSchema<Accessor> accessors;
            vsg::ObjectsSchema<BufferView> bufferViews;
            vsg::ObjectsSchema<Buffer> buffers;
            vsg::ObjectsSchema<Image> images;
            vsg::ObjectsSchema<Material> materials;
            vsg::ObjectsSchema<Mesh> meshes;
            vsg::ObjectsSchema<Node> nodes;
            vsg::ObjectsSchema<Sampler> samplers;
            glTFid scene;
            vsg::ObjectsSchema<Scene> scenes;
            vsg::ObjectsSchema<Texture> textures;
            vsg::ObjectsSchema<Animation> animations;
            vsg::ObjectsSchema<Camera> cameras;
            vsg::ObjectsSchema<Skins> skins;

            void read_array(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_object(vsg::JSONParser& parser, const std::string_view& property) override;
            void read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input) override;

            void report(vsg::LogOutput& output);

            virtual void resolveURIs(vsg::ref_ptr<const vsg::Options> options);
        };

        class VSGXCHANGE_DECLSPEC SceneGraphBuilder : public vsg::Inherit<vsg::Object, SceneGraphBuilder>
        {
        public:
            SceneGraphBuilder();

            struct SamplerImage
            {
                vsg::ref_ptr<vsg::Sampler> sampler;
                vsg::ref_ptr<vsg::Data> image;
            };

            vsg::ref_ptr<const vsg::Options> options;
            vsg::ref_ptr<vsg::ShaderSet> flatShaderSet;
            vsg::ref_ptr<vsg::ShaderSet> pbrShaderSet;
            vsg::ref_ptr<vsg::SharedObjects> sharedObjects;

            vsg::CoordinateConvention source_coordinateConvention = vsg::CoordinateConvention::Y_UP;
            int instanceNodeHint = vsg::Options::INSTANCE_NONE;
            bool cloneAccessors = false;
            float maxAnisotropy = 16.0f;

            vsg::ref_ptr<glTF> model;

            std::vector<vsg::ref_ptr<vsg::Data>> vsg_buffers;
            std::vector<vsg::ref_ptr<vsg::Data>> vsg_bufferViews;
            std::vector<vsg::ref_ptr<vsg::Data>> vsg_accessors;
            std::vector<vsg::ref_ptr<vsg::Camera>> vsg_cameras;
            std::vector<vsg::ref_ptr<vsg::JointSampler>> vsg_skins;
            std::vector<vsg::ref_ptr<vsg::Sampler>> vsg_samplers;
            std::vector<vsg::ref_ptr<vsg::Data>> vsg_images;
            std::vector<SamplerImage> vsg_textures;
            std::vector<vsg::ref_ptr<vsg::DescriptorConfigurator>> vsg_materials;
            std::vector<vsg::ref_ptr<vsg::Node>> vsg_meshes;
            std::vector<vsg::ref_ptr<vsg::Light>> vsg_lights;
            std::vector<vsg::ref_ptr<vsg::Node>> vsg_nodes;
            std::vector<vsg::ref_ptr<vsg::Node>> vsg_scenes;
            std::vector<bool> vsg_joints;
            vsg::Animations vsg_animations;

            struct MeshExtras
            {
                vsg::ref_ptr<gltf::Attributes> instancedAttributes;
                vsg::ref_ptr<vsg::JointSampler> jointSampler;
            };

            vsg::ref_ptr<vsg::DescriptorConfigurator> default_material;

            // map used to map gltf attribute names to ShaderSet vertex attribute names
            std::map<std::string, std::string> attributeLookup;

            void assign_extras(ExtensionsExtras& src, vsg::Object& dest);
            void assign_name_extras(NameExtensionsExtras& src, vsg::Object& dest);

            bool decodePrimitiveIfRequired(vsg::ref_ptr<gltf::Primitive> gltf_primitive);

            void flattenTransforms(gltf::Node& node, const vsg::dmat4& transform);

            bool getTransform(gltf::Node& node, vsg::dmat4& transform);

            vsg::ref_ptr<vsg::Data> createBuffer(vsg::ref_ptr<gltf::Buffer> gltf_buffer);
            vsg::ref_ptr<vsg::Data> createBufferView(vsg::ref_ptr<gltf::BufferView> gltf_bufferView);
            vsg::ref_ptr<vsg::Data> createArray(const std::string& type, uint32_t componentType, glTFid bufferView, uint32_t offset, uint32_t count);
            vsg::ref_ptr<vsg::Data> createAccessor(vsg::ref_ptr<gltf::Accessor> gltf_accessor);
            vsg::ref_ptr<vsg::Camera> createCamera(vsg::ref_ptr<gltf::Camera> gltf_camera);
            vsg::ref_ptr<vsg::Sampler> createSampler(vsg::ref_ptr<gltf::Sampler> gltf_sampler);
            vsg::ref_ptr<vsg::Data> createImage(vsg::ref_ptr<gltf::Image> gltf_image);
            SamplerImage createTexture(vsg::ref_ptr<gltf::Texture> gltf_texture);
            vsg::ref_ptr<vsg::DescriptorConfigurator> createPbrMaterial(vsg::ref_ptr<gltf::Material> gltf_material);
            vsg::ref_ptr<vsg::DescriptorConfigurator> createUnlitMaterial(vsg::ref_ptr<gltf::Material> gltf_material);
            vsg::ref_ptr<vsg::DescriptorConfigurator> createMaterial(vsg::ref_ptr<gltf::Material> gltf_material);
            vsg::ref_ptr<vsg::Node> createMesh(vsg::ref_ptr<gltf::Mesh> gltf_mesh, const MeshExtras& extras = {});
            vsg::ref_ptr<vsg::Light> createLight(vsg::ref_ptr<gltf::Light> gltf_light);
            vsg::ref_ptr<vsg::Node> createNode(vsg::ref_ptr<gltf::Node> gltf_node, bool jointNode);
            vsg::ref_ptr<vsg::Animation> createAnimation(vsg::ref_ptr<gltf::Animation> gltf_animation);
            vsg::ref_ptr<vsg::Node> createScene(vsg::ref_ptr<gltf::Scene> gltf_scene, bool requiresRootTransformNode, const vsg::dmat4& matrix);

            vsg::ref_ptr<vsg::ShaderSet> getOrCreatePbrShaderSet();
            vsg::ref_ptr<vsg::ShaderSet> getOrCreateFlatShaderSet();

            vsg::ref_ptr<vsg::Object> createSceneGraph(vsg::ref_ptr<gltf::glTF> in_model, vsg::ref_ptr<const vsg::Options> in_options);
        };

        static vsg::Path decodeURI(const std::string_view& uri);

        /// function for extracting components of a uri
        static bool dataURI(const std::string_view& uri, std::string_view& mimeType, std::string_view& encoding, std::string_view& value);

        /// function for mapping a mimeType to .extension that can be used with vsgXchange's plugins.
        static vsg::Path mimeTypeToExtension(const std::string_view& mimeType);

        virtual void assignExtensions(vsg::JSONParser& parser) const;
    };

    /// output stream support for glTFid
    inline std::ostream& operator<<(std::ostream& output, const gltf::glTFid& id)
    {
        if (id)
            output << "glTFid(" << id.value << ")";
        else
            output << "glTFid(null)";
        return output;
    }

    /// input stream support for glTFid
    inline std::istream& operator>>(std::istream& input, gltf::glTFid& id)
    {
        input >> id.value;
        return input;
    }

} // namespace vsgXchange

EVSG_type_name(vsgXchange::gltf)
