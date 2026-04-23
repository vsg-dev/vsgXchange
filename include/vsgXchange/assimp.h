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
#include <vsg/state/Sampler.h>
#include <vsg/animation/Animation.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>

#include <vsgXchange/Version.h>

#include <memory>

// forward declare
class aiScene;
class aiNode;
class aiBone;
class aiMesh;
class aiAnimation;
class aiMaterial;

namespace vsgXchange
{
    enum class TextureFormat
    {
        native,
        vsgt,
        vsgb
    };

    // this needs to be defined before 'vsg/commandline.h' has been included
    inline std::istream& operator>>(std::istream& is, TextureFormat& textureFormat)
    {
        std::string value;
        is >> value;

        if (value == "native")
            textureFormat = TextureFormat::native;
        else if (value == "vsgb")
            textureFormat = TextureFormat::vsgb;
        else if ((value == "vsgt") || (value == "vsga"))
            textureFormat = TextureFormat::vsgt;
        else
            textureFormat = TextureFormat::native;

        return is;
    }

    /// optional assimp ReaderWriter
    class VSGXCHANGE_DECLSPEC assimp : public vsg::Inherit<vsg::ReaderWriter, assimp>
    {
    public:
        assimp();
        vsg::ref_ptr<vsg::Object> read(const vsg::Path&, vsg::ref_ptr<const vsg::Options>) const override;
        vsg::ref_ptr<vsg::Object> read(std::istream&, vsg::ref_ptr<const vsg::Options>) const override;
        vsg::ref_ptr<vsg::Object> read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options = {}) const override;

        bool getFeatures(Features& features) const override;

        // vsg::Options::setValue(str, value) supported options:
        static constexpr const char* generate_smooth_normals = "generate_smooth_normals";
        static constexpr const char* generate_sharp_normals = "generate_sharp_normals";
        static constexpr const char* crease_angle = "crease_angle";                       /// float
        static constexpr const char* two_sided = "two_sided";                             ///  bool
        static constexpr const char* discard_empty_nodes = "discard_empty_nodes";         /// bool
        static constexpr const char* print_assimp = "print_assimp";                       /// int
        static constexpr const char* external_textures = "external_textures";             /// bool
        static constexpr const char* external_texture_format = "external_texture_format"; /// TextureFormat enum
        static constexpr const char* culling = "culling";                                 /// bool, insert cull nodes, defaults to true
        static constexpr const char* vertex_color_space = "vertex_color_space";           /// CoordinateSpace {sRGB or LINEAR} to assume when reading vertex colors
        static constexpr const char* material_color_space = "material_color_space";       /// CoordinateSpace {sRGB or LINEAR} to assume when reading materials colors

        bool readOptions(vsg::Options& options, vsg::CommandLine& arguments) const override;


    public:

        struct SamplerData
        {
            vsg::ref_ptr<vsg::Sampler> sampler;
            vsg::ref_ptr<vsg::Data> data;
        };

        struct SubgraphStats
        {
            unsigned int depth = 0;
            unsigned int numMesh = 0;
            unsigned int numNodes = 0;
            unsigned int numBones = 0;
            vsg::ref_ptr<vsg::Object> vsg_object;

            SubgraphStats& operator+=(const SubgraphStats& rhs)
            {
                numMesh += rhs.numMesh;
                numNodes += rhs.numNodes;
                numBones += rhs.numBones;
                return *this;
            }
        };

        struct BoneStats
        {
            unsigned int index = 0;
            std::string name;
            const aiNode* node;
        };

        /// class that converts the Assimp graph into a scene graph.
        class VSGXCHANGE_DECLSPEC Builder : public vsg::Inherit<vsg::Object, Builder>
        {
        public:
            Builder();

            using CameraMap = std::map<std::string, vsg::ref_ptr<vsg::Camera>>;
            using LightMap = std::map<std::string, vsg::ref_ptr<vsg::Light>>;
            using SubgraphStatsMap = std::map<const aiNode*, SubgraphStats>;
            using BoneStatsMap = std::map<const aiBone*, BoneStats>;
            using BoneTransformMap = std::map<const aiNode*, unsigned int>;

            vsg::Path filename;

            vsg::ref_ptr<const vsg::Options> options;
            const aiScene* scene = nullptr;
            vsg::Animations animations;
            CameraMap cameraMap;
            LightMap lightMap;
            SubgraphStatsMap subgraphStats;
            BoneStatsMap bones;
            BoneTransformMap boneTransforms;

            bool useViewDependentState = true;
            bool discardEmptyNodes = false;
            int printAssimp = 0;
            bool externalTextures = false;
            TextureFormat externalTextureFormat = TextureFormat::native;
            bool culling = true;

            // set for the file format being read.
            vsg::CoordinateSpace sourceVertexColorSpace = vsg::CoordinateSpace::LINEAR;
            vsg::CoordinateSpace sourceMaterialColorSpace = vsg::CoordinateSpace::LINEAR;

            // set for the target ShaderSet's
            vsg::CoordinateSpace targetVertexColorSpace = vsg::CoordinateSpace::LINEAR;
            vsg::CoordinateSpace targetMaterialCoordinateSpace = vsg::CoordinateSpace::LINEAR;

            // TODO flatShadedShaderSet?
            vsg::ref_ptr<vsg::ShaderSet> pbrShaderSet;
            vsg::ref_ptr<vsg::ShaderSet> phongShaderSet;
            vsg::ref_ptr<vsg::SharedObjects> sharedObjects;
            vsg::ref_ptr<vsg::External> externalObjects;

            std::vector<vsg::ref_ptr<vsg::DescriptorConfigurator>> convertedMaterials;
            std::vector<vsg::ref_ptr<vsg::Node>> convertedMeshes;
            std::set<std::string> animationTransforms;
            vsg::ref_ptr<vsg::JointSampler> jointSampler;
            vsg::ref_ptr<vsg::Node> topEmptyTransform;

            SubgraphStats collectSubgraphStats(const aiNode* node, unsigned int depth);
            SubgraphStats collectSubgraphStats(const aiScene* in_scene);

            SubgraphStats print(std::ostream& out, const aiAnimation* in_anim, vsg::indentation indent);
            SubgraphStats print(std::ostream& out, const aiMaterial* in_material, vsg::indentation indent);
            SubgraphStats print(std::ostream& out, const aiMesh* in_mesh, vsg::indentation indent);
            SubgraphStats print(std::ostream& out, const aiNode* in_node, vsg::indentation indent);
            SubgraphStats print(std::ostream& out, const aiScene* in_scene, vsg::indentation indent);

            bool getColor(const aiMaterial* material, const char* pKey, unsigned int type, unsigned int idx, vsg::vec3& value);
            bool getColor(const aiMaterial* material, const char* pKey, unsigned int type, unsigned int idx, vsg::vec4& value);

            SamplerData convertTexture(const aiMaterial& material, int type) const;

            void processAnimations();
            void processCameras();
            void processLights();

            vsg::ref_ptr<vsg::MatrixTransform> processCoordinateFrame(const vsg::Path& ext);


            vsg::ref_ptr<vsg::ShaderSet> getOrCreatePbrShaderSet()
            {
                if (!pbrShaderSet)
                {
                    pbrShaderSet = vsg::createPhysicsBasedRenderingShaderSet(options);
                    if (sharedObjects) sharedObjects->share(pbrShaderSet);
                }
                return pbrShaderSet;
            }

            vsg::ref_ptr<vsg::ShaderSet> getOrCreatePhongShaderSet()
            {
                if (!phongShaderSet)
                {
                    phongShaderSet = vsg::createPhongShaderSet(options);
                    if (sharedObjects) sharedObjects->share(phongShaderSet);
                }
                return phongShaderSet;
            }


            void convert(const aiMaterial* material, vsg::DescriptorConfigurator& convertedMaterial);

            vsg::ref_ptr<vsg::Data> createIndices(const aiMesh* mesh, unsigned int numIndicesPerFace, uint32_t numIndices);
            void convert(const aiMesh* mesh, vsg::ref_ptr<vsg::Node>& node);

            vsg::ref_ptr<vsg::Node> visit(const aiNode* node, int depth);

            virtual vsg::ref_ptr<vsg::Node> visit(const aiScene* in_scene, vsg::ref_ptr<const vsg::Options> in_options, const vsg::Path& ext);
        };


    protected:
        ~assimp();

        class Implementation;
        Implementation* _implementation;
    };

} // namespace vsgXchange

EVSG_type_name(vsgXchange::TextureFormat)
EVSG_type_name(vsgXchange::assimp);
EVSG_type_name(vsgXchange::assimp::Builder);
