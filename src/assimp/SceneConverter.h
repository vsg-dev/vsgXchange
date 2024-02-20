/* <editor-fold desc="MIT License">

Copyright(c) 2021 André Normann & Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */


#include <cmath>
#include <sstream>
#include <stack>

#include <vsg/all.h>
#include <vsgXchange/models.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#if (ASSIMP_VERSION_MAJOR == 5 && ASSIMP_VERSION_MINOR == 0)
#    include <assimp/pbrmaterial.h>
#    define AI_MATKEY_BASE_COLOR AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR
#    define AI_MATKEY_GLOSSINESS_FACTOR AI_MATKEY_GLTF_PBRSPECULARGLOSSINESS_GLOSSINESS_FACTOR
#    define AI_MATKEY_METALLIC_FACTOR AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR
#    define AI_MATKEY_ROUGHNESS_FACTOR AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR
#else
#    include <assimp/material.h>

#    if (ASSIMP_VERSION_MAJOR == 5 && ASSIMP_VERSION_MINOR == 1 && ASSIMP_VERSION_PATCH == 0)
#        define AI_MATKEY_GLTF_ALPHACUTOFF "$mat.gltf.alphaCutoff", 0, 0
#        define AI_MATKEY_GLTF_ALPHAMODE "$mat.gltf.alphaMode", 0, 0
#    else
#        include <assimp/GltfMaterial.h>
#    endif
#endif

#include <iostream>

namespace vsgXchange
{
    enum class TextureFormat
    {
        native,
        vsgt,
        vsgb
    };

    // this needs to be defined before 'vsg/commandline.h' has been included
    inline std::istream& operator>> (std::istream& is, TextureFormat& textureFormat)
    {
        std::string value;
        is >> value;

        if (value == "native")
            textureFormat = TextureFormat::native;
        else if (value == "vsgb")
            textureFormat = TextureFormat::vsgb;
        else if ((value == "vsgt")||(value == "vsga"))
            textureFormat = TextureFormat::vsgt;
        else
            textureFormat = TextureFormat::native;

        return is;
    }

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

        SubgraphStats& operator += (const SubgraphStats& rhs)
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

    inline std::ostream& operator<<(std::ostream& output, const SubgraphStats& stats)
    {
        return output<<"SubgraphStats{ numMesh = "<<stats.numMesh<<", numNodes = "<< stats.numNodes<<", numBones = "<<stats.numBones<<" }";
    }

    inline std::ostream& operator<<(std::ostream& output, const aiMatrix4x4& m)
    {
        if (m.IsIdentity()) return output<<"aiMatrix4x4{ Identity }";
        else return output<<"aiMatrix4x4{ {"<<m.a1<<", "<<m.a2<<", "<<m.a3<<", "<<m.a4<< "} {"<<m.b1<<", "<<m.b2<<", "<<m.b3<<", "<<m.b4<< "} {"<<m.c1<<", "<<m.c2<<", "<<m.c3<<", "<<m.c4<< "} {"<<m.d1<<", "<<m.d2<<", "<<m.d3<<", "<<m.d4<< "} }";
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // assimp ReaderWriter SceneConverter
    //
    struct SceneConverter
    {
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
        bool sRGBTextures = false;
        bool culling = true;

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

        static vsg::vec3 convert(const aiVector3D& v) { return vsg::vec3(v[0], v[1], v[2]); }
        static vsg::dvec3 dconvert(const aiVector3D& v) { return vsg::dvec3(v[0], v[1], v[2]); }
        static vsg::vec3 convert(const aiColor3D& v) { return vsg::vec3(v[0], v[1], v[2]); }
        static vsg::vec4 convert(const aiColor4D& v) { return vsg::vec4(v[0], v[1], v[2], v[3]); }

        static bool getColor(const aiMaterial* material, const char *pKey, unsigned int type, unsigned int idx, vsg::vec3& value)
        {
            aiColor3D color;
            if (material->Get(pKey, type, idx, color) == AI_SUCCESS)
            {
                value = convert(color);
                return true;
            }
            return false;
        }
        static bool getColor(const aiMaterial* material, const char *pKey, unsigned int type, unsigned int idx, vsg::vec4& value)
        {
            aiColor4D color;
            if (material->Get(pKey, type, idx, color) == AI_SUCCESS)
            {
                value = convert(color);
                return true;
            }
            return false;
        }

        void processAnimations();
        void processCameras();
        void processLights();

        vsg::ref_ptr<vsg::MatrixTransform> processCoordinateFrame(const vsg::Path& ext);

        VkSamplerAddressMode getWrapMode(aiTextureMapMode mode) const
        {
            switch (mode)
            {
            case aiTextureMapMode_Wrap: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case aiTextureMapMode_Clamp: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case aiTextureMapMode_Decal: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            case aiTextureMapMode_Mirror: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            default: break;
            }
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }

        bool hasAlphaBlend(const aiMaterial* material) const
        {
            aiString alphaMode;
            float opacity = 1.0;
            if ((material->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS && alphaMode == aiString("BLEND")) || (material->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS && opacity < 1.0))
                return true;
            return false;
        }

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

        SamplerData convertTexture(const aiMaterial& material, aiTextureType type) const;

        void convert(const aiMaterial* material, vsg::DescriptorConfigurator& convertedMaterial);

        vsg::ref_ptr<vsg::Data> createIndices(const aiMesh* mesh, unsigned int numIndicesPerFace, uint32_t numIndices);
        void convert(const aiMesh* mesh, vsg::ref_ptr<vsg::Node>& node);

        vsg::ref_ptr<vsg::Node> visit(const aiScene* in_scene, vsg::ref_ptr<const vsg::Options> in_options, const vsg::Path& ext);
        vsg::ref_ptr<vsg::Node> visit(const aiNode* node, int depth);
    };
}
