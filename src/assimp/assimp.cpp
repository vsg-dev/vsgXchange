/* <editor-fold desc="MIT License">

Copyright(c) 2021 André Normann & Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgXchange/models.h>

#include <cmath>
#include <sstream>
#include <stack>

#include <vsg/all.h>

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

namespace
{
    struct SamplerData
    {
        vsg::ref_ptr<vsg::Sampler> sampler;
        vsg::ref_ptr<vsg::Data> data;
    };

} // namespace

using namespace vsgXchange;

class assimp::Implementation
{
public:
    Implementation();

    vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const;
    vsg::ref_ptr<vsg::Object> read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options = {}) const;
    vsg::ref_ptr<vsg::Object> read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options = {}) const;

    const uint32_t _importFlags;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// assimp ReaderWriter fascade
//
assimp::assimp() :
    _implementation(new assimp::Implementation())
{
    vsg::debug("ASSIMP_VERSION_MAJOR ", ASSIMP_VERSION_MAJOR);
    vsg::debug("ASSIMP_VERSION_MINOR ", ASSIMP_VERSION_MINOR);
    vsg::debug("ASSIMP_VERSION_PATCH ", ASSIMP_VERSION_PATCH);
}
assimp::~assimp()
{
    delete _implementation;
}
vsg::ref_ptr<vsg::Object> assimp::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    return _implementation->read(filename, options);
}

vsg::ref_ptr<vsg::Object> assimp::read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const
{
    return _implementation->read(fin, options);
}

vsg::ref_ptr<vsg::Object> assimp::read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options) const
{
    return _implementation->read(ptr, size, options);
}

bool assimp::getFeatures(Features& features) const
{
    std::string suported_extensions;
    Assimp::Importer importer;
    importer.GetExtensionList(suported_extensions);

    vsg::ReaderWriter::FeatureMask supported_features = static_cast<vsg::ReaderWriter::FeatureMask>(vsg::ReaderWriter::READ_FILENAME | vsg::ReaderWriter::READ_ISTREAM | vsg::ReaderWriter::READ_MEMORY);

    std::string::size_type start = 1; // skip *
    std::string::size_type semicolon = suported_extensions.find(';', start);
    while (semicolon != std::string::npos)
    {
        features.extensionFeatureMap[suported_extensions.substr(start, semicolon - start)] = supported_features;
        start = semicolon + 2;
        semicolon = suported_extensions.find(';', start);
    }
    features.extensionFeatureMap[suported_extensions.substr(start, std::string::npos)] = supported_features;

    // enumerate the supported vsg::Options::setValue(str, value) options
    features.optionNameTypeMap[assimp::generate_smooth_normals] = vsg::type_name<bool>();
    features.optionNameTypeMap[assimp::generate_sharp_normals] = vsg::type_name<bool>();
    features.optionNameTypeMap[assimp::crease_angle] = vsg::type_name<float>();
    features.optionNameTypeMap[assimp::two_sided] = vsg::type_name<bool>();
    features.optionNameTypeMap[assimp::discard_empty_nodes] = vsg::type_name<bool>();

    return true;
}

bool assimp::readOptions(vsg::Options& options, vsg::CommandLine& arguments) const
{
    bool result = arguments.readAndAssign<bool>(assimp::generate_smooth_normals, &options);
    result = arguments.readAndAssign<bool>(assimp::generate_sharp_normals, &options) || result;
    result = arguments.readAndAssign<float>(assimp::crease_angle, &options) || result;
    result = arguments.readAndAssign<bool>(assimp::two_sided, &options) || result;
    result = arguments.readAndAssign<bool>(assimp::discard_empty_nodes, &options) || result;
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// assimp ReaderWriter SceneConverter
//
struct SceneConverter
{
    using CameraMap = std::map<std::string, vsg::ref_ptr<vsg::Camera>>;
    using LightMap = std::map<std::string, vsg::ref_ptr<vsg::Light>>;

    vsg::Path filename;

    vsg::ref_ptr<const vsg::Options> options;
    const aiScene* scene = nullptr;
    CameraMap cameraMap;
    LightMap lightMap;

    bool useViewDependentState = true;
    bool discardEmptyNodes = true;

    // TODO flatShadedShaderSet?
    vsg::ref_ptr<vsg::ShaderSet> pbrShaderSet;
    vsg::ref_ptr<vsg::ShaderSet> phongShaderSet;
    vsg::ref_ptr<vsg::SharedObjects> sharedObjects;

    std::vector<vsg::ref_ptr<vsg::DescriptorConfigurator>> convertedMaterials;
    std::vector<vsg::ref_ptr<vsg::Node>> convertedMeshes;

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

SamplerData SceneConverter::convertTexture(const aiMaterial& material, aiTextureType type) const
{
    aiString texPath;
    aiTextureMapMode wrapMode[]{aiTextureMapMode_Wrap, aiTextureMapMode_Wrap, aiTextureMapMode_Wrap};

    if (material.GetTexture(type, 0, &texPath, nullptr, nullptr, nullptr, nullptr, wrapMode) == AI_SUCCESS)
    {
        SamplerData samplerImage;

        if (auto texture = scene->GetEmbeddedTexture(texPath.C_Str()))
        {
            // embedded texture has no width so must be invalid
            if (texture->mWidth == 0) return {};

            if (texture->mHeight == 0)
            {
                vsg::debug("filename = ", filename, " : Embedded compressed format texture->achFormatHint = ", texture->achFormatHint);

                // texture is a compressed format, defer to the VSG's vsg::read() to convert the block of data to vsg::Data image.
                auto imageOptions = vsg::Options::create(*options);
                imageOptions->extensionHint = vsg::Path(".") + texture->achFormatHint;
                samplerImage.data = vsg::read_cast<vsg::Data>(reinterpret_cast<const uint8_t*>(texture->pcData), texture->mWidth, imageOptions);

                // if no data assigned return null
                if (!samplerImage.data) return {};
            }
            else
            {
                vsg::debug("filename = ", filename, " : Embedded raw format texture->achFormatHint = ", texture->achFormatHint);

                // Vulkan doesn't support this format, we have to reorder it to RGBA
                auto image = vsg::ubvec4Array2D::create(texture->mWidth, texture->mHeight, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM});
                auto src = texture->pcData;
                for(auto& dest_c : *image)
                {
                    auto& src_c = *(src++);
                    dest_c.r = src_c.r;
                    dest_c.g = src_c.g;
                    dest_c.b = src_c.b;
                    dest_c.a = src_c.a;
                }
                samplerImage.data = image;
            }
        }
        else
        {
            auto textureFilename = vsg::findFile(texPath.C_Str(), options);
            if (samplerImage.data = vsg::read_cast<vsg::Data>(textureFilename, options); !samplerImage.data.valid())
            {
                vsg::warn("Failed to load texture: ", textureFilename, " texPath = ", texPath.C_Str());
                return {};
            }
        }

        samplerImage.sampler = vsg::Sampler::create();
        samplerImage.sampler->addressModeU = getWrapMode(wrapMode[0]);
        samplerImage.sampler->addressModeV = getWrapMode(wrapMode[1]);
        samplerImage.sampler->addressModeW = getWrapMode(wrapMode[2]);
        samplerImage.sampler->anisotropyEnable = VK_TRUE;
        samplerImage.sampler->maxAnisotropy = 16.0f;
        samplerImage.sampler->maxLod = samplerImage.data->properties.maxNumMipmaps;

        if (samplerImage.sampler->maxLod <= 1.0)
        {
            // Calculate maximum lod level
            auto maxDim = std::max(samplerImage.data->width(), samplerImage.data->height());
            samplerImage.sampler->maxLod = std::floor(std::log2f(static_cast<float>(maxDim)));
        }

        if (sharedObjects)
        {
            sharedObjects->share(samplerImage.data);
            sharedObjects->share(samplerImage.sampler);
        }

        return samplerImage;
    }
    else
    {
        return {};
    }
}

void SceneConverter::convert(const aiMaterial* material, vsg::DescriptorConfigurator& convertedMaterial)
{
    auto& defines = convertedMaterial.defines;

    vsg::PbrMaterial pbr;
    bool hasPbrSpecularGlossiness = getColor(material, AI_MATKEY_COLOR_SPECULAR, pbr.specularFactor);

    convertedMaterial.blending = hasAlphaBlend(material);

    if ((options->getValue(assimp::two_sided, convertedMaterial.two_sided) || (material->Get(AI_MATKEY_TWOSIDED, convertedMaterial.two_sided) == AI_SUCCESS)) && convertedMaterial.two_sided)
    {
        defines.insert("VSG_TWO_SIDED_LIGHTING");
    }

    if (getColor(material, AI_MATKEY_BASE_COLOR, pbr.baseColorFactor) || hasPbrSpecularGlossiness)
    {
        // PBR path
        convertedMaterial.shaderSet = getOrCreatePbrShaderSet();

        if (convertedMaterial.blending)
            pbr.alphaMask = 0.0f;

        if (hasPbrSpecularGlossiness)
        {
            defines.insert("VSG_WORKFLOW_SPECGLOSS");
            getColor(material, AI_MATKEY_COLOR_DIFFUSE, pbr.diffuseFactor);

            if (material->Get(AI_MATKEY_GLOSSINESS_FACTOR, pbr.specularFactor.a) != AI_SUCCESS)
            {
                if (float shininess; material->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS)
                    pbr.specularFactor.a = shininess / 1000;
            }
        }
        else
        {
            material->Get(AI_MATKEY_METALLIC_FACTOR, pbr.metallicFactor);
            material->Get(AI_MATKEY_ROUGHNESS_FACTOR, pbr.roughnessFactor);
        }

        getColor(material, AI_MATKEY_COLOR_EMISSIVE, pbr.emissiveFactor);
        material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, pbr.alphaMaskCutoff);

        SamplerData samplerImage;
        if (samplerImage = convertTexture(*material, aiTextureType_DIFFUSE); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("diffuseMap", samplerImage.data, samplerImage.sampler);
        }
        if (samplerImage = convertTexture(*material, aiTextureType_EMISSIVE); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("emissiveMap", samplerImage.data, samplerImage.sampler);
        }

        if (samplerImage = convertTexture(*material, aiTextureType_LIGHTMAP); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("aoMap", samplerImage.data, samplerImage.sampler);
        }

        if (samplerImage = convertTexture(*material, aiTextureType_NORMALS); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("normalMap", samplerImage.data, samplerImage.sampler);
        }

        if (samplerImage = convertTexture(*material, aiTextureType_UNKNOWN); samplerImage.data.valid())
        {
            // maps to metal roughness.
            convertedMaterial.assignTexture("mrMap", samplerImage.data, samplerImage.sampler);
        }

        if (samplerImage = convertTexture(*material, aiTextureType_SPECULAR); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("specularMap", samplerImage.data, samplerImage.sampler);
        }

        convertedMaterial.assignUniform("material", vsg::PbrMaterialValue::create(pbr));
    }
    else
    {
        // Phong shading
        convertedMaterial.shaderSet = getOrCreatePhongShaderSet();

        vsg::PhongMaterial mat;

        if (convertedMaterial.blending)
            mat.alphaMask = 0.0f;

        material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, mat.alphaMaskCutoff);
        getColor(material, AI_MATKEY_COLOR_AMBIENT, mat.ambient);
        const auto diffuseResult = getColor(material, AI_MATKEY_COLOR_DIFFUSE, mat.diffuse);
        const auto emissiveResult = getColor(material, AI_MATKEY_COLOR_EMISSIVE, mat.emissive);
        const auto specularResult = getColor(material, AI_MATKEY_COLOR_SPECULAR, mat.specular);

        aiShadingMode shadingModel = aiShadingMode_Phong;
        material->Get(AI_MATKEY_SHADING_MODEL, shadingModel);

        unsigned int maxValue = 1;
        float strength = 1.0f;
        if (aiGetMaterialFloatArray(material, AI_MATKEY_SHININESS, &mat.shininess, &maxValue) == AI_SUCCESS)
        {
            maxValue = 1;
            if (aiGetMaterialFloatArray(material, AI_MATKEY_SHININESS_STRENGTH, &strength, &maxValue) == AI_SUCCESS)
                mat.shininess *= strength;
        }
        else
        {
            mat.shininess = 0.0f;
            mat.specular.set(0.0f, 0.0f, 0.0f, 0.0f);
        }

        if (mat.shininess < 0.01f)
        {
            mat.shininess = 0.0f;
            mat.specular.set(0.0f, 0.0f, 0.0f, 0.0f);
        }

        SamplerData samplerImage;
        if (samplerImage = convertTexture(*material, aiTextureType_DIFFUSE); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("diffuseMap", samplerImage.data, samplerImage.sampler);

            if (diffuseResult != AI_SUCCESS)
                mat.diffuse.set(1.0f, 1.0f, 1.0f, 1.0f);
        }

        if (samplerImage = convertTexture(*material, aiTextureType_EMISSIVE); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("emissiveMap", samplerImage.data, samplerImage.sampler);

            if (emissiveResult != AI_SUCCESS)
                mat.emissive.set(1.0f, 1.0f, 1.0f, 1.0f);
        }

        if (samplerImage = convertTexture(*material, aiTextureType_LIGHTMAP); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("aoMap", samplerImage.data, samplerImage.sampler);
        }
        else if (samplerImage = convertTexture(*material, aiTextureType_AMBIENT); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("aoMap", samplerImage.data, samplerImage.sampler);
        }

        if (samplerImage = convertTexture(*material, aiTextureType_NORMALS); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("normalMap", samplerImage.data, samplerImage.sampler);
        }

        if (samplerImage = convertTexture(*material, aiTextureType_SPECULAR); samplerImage.data.valid())
        {
            // TODO phong shader doesn't have a specular texture map at present
            convertedMaterial.assignTexture("specularMap", samplerImage.data, samplerImage.sampler);

            if (specularResult != AI_SUCCESS)
                mat.specular.set(1.0f, 1.0f, 1.0f, 1.0f);
        }

        convertedMaterial.assignUniform("material", vsg::PhongMaterialValue::create(mat));
    }

    if (sharedObjects)
    {
        for(auto& ds : convertedMaterial.descriptorSets)
        {
            if (ds)
            {
                sharedObjects->share(ds->descriptors);
                sharedObjects->share(ds);
            }
        }
    }
}

vsg::ref_ptr<vsg::Data> SceneConverter::createIndices(const aiMesh* mesh, unsigned int numIndicesPerFace, uint32_t numIndices)
{
    if (mesh->mNumVertices > 16384)
    {
        auto indices = vsg::uintArray::create(numIndices);
        auto itr = indices->begin();
        for (unsigned int j = 0; j < mesh->mNumFaces; ++j)
        {
            const auto& face = mesh->mFaces[j];
            if (face.mNumIndices == numIndicesPerFace)
            {
                for (unsigned int i = 0; i < numIndicesPerFace; ++i) (*itr++) = static_cast<uint32_t>(face.mIndices[i]);
            }
        }
        return indices;
    }
    else
    {
        auto indices = vsg::ushortArray::create(numIndices);
        auto itr = indices->begin();
        for (unsigned int j = 0; j < mesh->mNumFaces; ++j)
        {
            const auto& face = mesh->mFaces[j];
            if (face.mNumIndices == numIndicesPerFace)
            {
                for (unsigned int i = 0; i < numIndicesPerFace; ++i) (*itr++) = static_cast<uint16_t>(face.mIndices[i]);
            }
        }
        return indices;
    }
}

void SceneConverter::convert(const aiMesh* mesh, vsg::ref_ptr<vsg::Node>& node)
{
    if (convertedMaterials.size() <= mesh->mMaterialIndex)
    {
        vsg::warn("Warning:  mesh (", mesh, ") mesh->mMaterialIndex = ", mesh->mMaterialIndex, " exceeds available materials.size()= ", convertedMaterials.size());
        return;
    }

    if (mesh->mNumVertices == 0 || mesh->mVertices == nullptr)
    {
        vsg::warn("Warning:  mesh (", mesh, ") no vertices data, mesh->mNumVertices = ", mesh->mNumVertices, " mesh->mVertices = ", mesh->mVertices);
        return;
    }

    if (mesh->mNumFaces == 0 || mesh->mFaces == nullptr)
    {
        vsg::warn("Warning:  mesh (", mesh, ") no mesh data, mesh->mNumFaces = ", mesh->mNumFaces);
        return;
    }

    std::string name = mesh->mName.C_Str();
    auto material = convertedMaterials[mesh->mMaterialIndex];

    // count the number of indices of each type
    uint32_t numTriangleIndices = 0;
    uint32_t numLineIndices = 0;
    uint32_t numPointIndices = 0;
    for (unsigned int j = 0; j < mesh->mNumFaces; ++j)
    {
        const auto& face = mesh->mFaces[j];
        if (face.mNumIndices == 3)
            numTriangleIndices += 3;
        else if (face.mNumIndices == 2)
            numLineIndices += 2;
        else if (face.mNumIndices == 1)
            numPointIndices += 1;
        else
        {
            vsg::warn("Warning: unsupported number of indices on face ", face.mNumIndices);
        }
    }

    int numPrimitiveTypes = 0;
    if (numTriangleIndices > 0) ++numPrimitiveTypes;
    if (numLineIndices > 0) ++numPrimitiveTypes;
    if (numPointIndices > 0) ++numPrimitiveTypes;

    if (numPrimitiveTypes > 1)
    {
        vsg::warn("Warning: more than one primitive type required, numTriangleIndices = ", numTriangleIndices, ", numLineIndices = ", numLineIndices, ", numPointIndices = ", numPointIndices);
    }

    unsigned int numIndicesPerFace = 0;
    uint32_t numIndices = 0;
    VkPrimitiveTopology topology{};
    if (numTriangleIndices > 0)
    {
        topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        numIndicesPerFace = 3;
        numIndices = numTriangleIndices;
    }
    else if (numLineIndices > 0)
    {
        topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        numIndicesPerFace = 2;
        numIndices = numLineIndices;
    }
    else if (numPointIndices > 0)
    {
        topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        numIndicesPerFace = 1;
        numIndices = numPointIndices;
    }
    else
    {
        vsg::warn("Warning: no primitive indices ");
        return;
    }

    auto config = vsg::GraphicsPipelineConfigurator::create(material->shaderSet);
    config->descriptorConfigurator = material;

    auto indices = createIndices(mesh, numIndicesPerFace, numIndices);

    vsg::DataList vertexArrays;
    auto vertices = vsg::vec3Array::create(mesh->mNumVertices);
    std::memcpy(vertices->dataPointer(), mesh->mVertices, mesh->mNumVertices * 12);
    config->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vertices);

    if (mesh->mNormals)
    {
        auto normals = vsg::vec3Array::create(mesh->mNumVertices);
        std::memcpy(normals->dataPointer(), mesh->mNormals, mesh->mNumVertices * 12);
        config->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, normals);
    }
    else
    {
        auto normal = vsg::vec3Value::create(vsg::vec3(0.0f, 0.0f, 1.0f));
        config->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_INSTANCE, normal);
    }

    if (mesh->mTextureCoords[0])
    {
        auto dest_texcoords = vsg::vec2Array::create(mesh->mNumVertices);
        auto src_texcoords = mesh->mTextureCoords[0];
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
        {
            auto& tc = src_texcoords[i];
            dest_texcoords->at(i).set(tc[0], tc[1]);
        }
        config->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, dest_texcoords);
    }
    else
    {
        auto texcoord = vsg::vec2Value::create(vsg::vec2(0.0f, 0.0f));
        config->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_INSTANCE, texcoord);
    }

    if (mesh->mColors[0])
    {
        auto colors = vsg::vec4Array::create(mesh->mNumVertices);
        std::memcpy(colors->dataPointer(), mesh->mColors[0], mesh->mNumVertices * 16);
        config->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_VERTEX, colors);
    }
    else
    {
        auto colors = vsg::vec4Value::create(vsg::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        config->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, colors);
    }

    auto vid = vsg::VertexIndexDraw::create();
    vid->assignArrays(vertexArrays);
    vid->assignIndices(indices);
    vid->indexCount = static_cast<uint32_t>(indices->valueCount());
    vid->instanceCount = 1;
    if (!name.empty()) vid->setValue("name", name);

    // set the GraphicsPipelineStates to the required values.
    struct SetPipelineStates : public vsg::Visitor
    {
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        bool blending = false;
        bool two_sided = false;

        SetPipelineStates(VkPrimitiveTopology in_topology, bool in_blending, bool in_two_sided) : topology(in_topology), blending(in_blending), two_sided(in_two_sided) {}

        void apply(vsg::Object& object) { object.traverse(*this); }
        void apply(vsg::RasterizationState& rs) { if (two_sided) rs.cullMode = VK_CULL_MODE_NONE; }
        void apply(vsg::InputAssemblyState& ias) { ias.topology = topology; }
        void apply(vsg::ColorBlendState& cbs) { cbs.configureAttachments(blending); }
    } sps(topology, material->blending, material->two_sided);
    config->accept(sps);

    if (sharedObjects)
        sharedObjects->share(config, [](auto gpc) { gpc->init(); });
    else
        config->init();

    // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
    auto stateGroup = vsg::StateGroup::create();

    config->copyTo(stateGroup, sharedObjects);

    stateGroup->addChild(vid);

    if (material->blending)
    {
        vsg::ComputeBounds computeBounds;
        vid->accept(computeBounds);
        vsg::dvec3 center = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.5;

        auto depthSorted = vsg::DepthSorted::create();
        depthSorted->binNumber = 10;
        depthSorted->bound.set(center[0], center[1], center[2], radius);
        depthSorted->child = stateGroup;

        node = depthSorted;
    }
    else
    {
        node = stateGroup;
    }
}

vsg::ref_ptr<vsg::Node> SceneConverter::visit(const aiScene* in_scene, vsg::ref_ptr<const vsg::Options> in_options, const vsg::Path& ext)
{
    scene = in_scene;
    options = in_options;
    discardEmptyNodes = vsg::value<bool>(true, assimp::discard_empty_nodes, options);

    std::string name = scene->mName.C_Str();

    if (options) sharedObjects = options->sharedObjects;
    if (!sharedObjects) sharedObjects = vsg::SharedObjects::create();

    processCameras();
    processLights();

    // convert the materials
    convertedMaterials.resize(scene->mNumMaterials);
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
    {
        convertedMaterials[i] = vsg::DescriptorConfigurator::create();
        convert(scene->mMaterials[i], *convertedMaterials[i]);
    }

    // convert the meshes
    convertedMeshes.resize(scene->mNumMeshes);
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
    {
        convert(scene->mMeshes[i], convertedMeshes[i]);
    }

    auto vsg_scene = visit(scene->mRootNode, 0);
    if (!vsg_scene)
    {
        if (scene->mNumMeshes == 1)
        {
            vsg_scene = convertedMeshes[0];
        }
        else if (scene->mNumMeshes > 1)
        {
            auto group = vsg::Group::create();
            for (auto& node : convertedMeshes)
            {
                if (node) group->addChild(node);
            }
        }

        if (!vsg_scene) return {};
    }


    if (auto transform = processCoordinateFrame(ext))
    {
        transform->addChild(vsg_scene);
        if (!name.empty()) transform->setValue("name", name);

        // TODO check if subgraph requires culling
        // transform->subgraphRequiresLocalFrustum = false;

        return transform;
    }
    else
    {
        if (!name.empty()) vsg_scene->setValue("name", name);
        return vsg_scene;
    }
}

vsg::ref_ptr<vsg::Node> SceneConverter::visit(const aiNode* node, int depth)
{
    vsg::Group::Children children;

    std::string name = node->mName.C_Str();

    // assign any cameras
    if (auto camera_itr = cameraMap.find(name); camera_itr != cameraMap.end())
    {
        children.push_back(camera_itr->second);
    }

    // assign any lights
    if (auto light_itr = lightMap.find(name); light_itr != lightMap.end())
    {
        children.push_back(light_itr->second);
    }

    // assign the meshes
    for (unsigned int i = 0; i < node->mNumMeshes; ++i)
    {
        auto mesh_index = node->mMeshes[i];
        if (auto child = convertedMeshes[mesh_index])
        {
            children.push_back(child);
        }
    }

    // visit the children
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        if (auto child = visit(node->mChildren[i], depth + 1))
        {
            children.push_back(child);
        }
    }

    if (children.empty() && discardEmptyNodes) return {};

    if (discardEmptyNodes && node->mTransformation.IsIdentity())
    {
        if (children.size() == 1 && name.empty()) return children[0];

        auto group = vsg::Group::create();
        group->children = children;
        if (!name.empty()) group->setValue("name", name);

        return group;
    }
    else
    {
        aiMatrix4x4 m = node->mTransformation;
        m.Transpose();

        auto transform = vsg::MatrixTransform::create(vsg::dmat4(vsg::mat4((float*)&m)));
        transform->children = children;
        if (!name.empty()) transform->setValue("name", name);

        // TODO check if subgraph requires culling
        //transform->subgraphRequiresLocalFrustum = false;

        return transform;
    }
}
void SceneConverter::processCameras()
{
    if (scene->mNumCameras > 0)
    {
        for (unsigned int li = 0; li < scene->mNumCameras; ++li)
        {
            auto* camera = scene->mCameras[li];
            auto vsg_camera = vsg::Camera::create();
            vsg_camera->name = camera->mName.C_Str();

            vsg_camera->viewMatrix = vsg::LookAt::create(
                vsg::dvec3(camera->mPosition[0], camera->mPosition[1], camera->mPosition[2]), // eye
                vsg::dvec3(camera->mLookAt[0], camera->mLookAt[1], camera->mLookAt[2]),       // center
                vsg::dvec3(camera->mUp[0], camera->mUp[1], camera->mUp[2])                    // up
            );

            double verticalFOV = vsg::degrees(atan(tan(static_cast<double>(camera->mHorizontalFOV) * 0.5) / camera->mAspect) * 2.0);
            vsg_camera->projectionMatrix = vsg::Perspective::create(verticalFOV, camera->mAspect, camera->mClipPlaneNear, camera->mClipPlaneFar);

            // the aiNodes in the scene with the same name as the camera will provide a place to add the camera, this is added in the node handling in the for loop below.
            cameraMap[vsg_camera->name] = vsg_camera;
        }
    }
}

void SceneConverter::processLights()
{
    if (scene->mNumLights > 0)
    {
        auto setColorAndIntensity = [](const aiLight& light, vsg::Light& vsg_light) -> void
        {
            vsg_light.color = convert(light.mColorDiffuse);
            float maxValue = std::max(std::max(vsg_light.color.r, vsg_light.color.g),  vsg_light.color.b);
            if (maxValue > 0.0)
            {
                vsg_light.color /= maxValue;
                vsg_light.intensity = maxValue;
            }
            else
            {
                vsg_light.intensity = 0.0;
            }
        };

        for (unsigned int li = 0; li < scene->mNumLights; ++li)
        {
            auto* light = scene->mLights[li];
            switch (light->mType)
            {
            case (aiLightSource_UNDEFINED): {
                auto vsg_light = vsg::Light::create();
                setColorAndIntensity(*light, *vsg_light);
                vsg_light->name = light->mName.C_Str();
                vsg_light->setValue("light_type", "UNDEFINED");
                lightMap[vsg_light->name] = vsg_light;
                break;
            }
            case (aiLightSource_DIRECTIONAL): {
                auto vsg_light = vsg::DirectionalLight::create();
                setColorAndIntensity(*light, *vsg_light);
                vsg_light->name = light->mName.C_Str();
                vsg_light->direction = dconvert(light->mDirection);
                lightMap[vsg_light->name] = vsg_light;
                break;
            }
            case (aiLightSource_POINT): {
                auto vsg_light = vsg::PointLight::create();
                setColorAndIntensity(*light, *vsg_light);
                vsg_light->name = light->mName.C_Str();
                vsg_light->position = dconvert(light->mDirection);
                lightMap[vsg_light->name] = vsg_light;
                break;
            }
            case (aiLightSource_SPOT): {
                auto vsg_light = vsg::SpotLight::create();
                setColorAndIntensity(*light, *vsg_light);
                vsg_light->name = light->mName.C_Str();
                vsg_light->position = dconvert(light->mDirection);
                vsg_light->direction = dconvert(light->mDirection);
                vsg_light->innerAngle = light->mAngleInnerCone;
                vsg_light->outerAngle = light->mAngleOuterCone;
                lightMap[vsg_light->name] = vsg_light;
                break;
            }
            case (aiLightSource_AMBIENT): {
                auto vsg_light = vsg::AmbientLight::create();
                setColorAndIntensity(*light, *vsg_light);
                vsg_light->name = light->mName.C_Str();
                lightMap[vsg_light->name] = vsg_light;
                break;
            }
            case (aiLightSource_AREA): {
                auto vsg_light = vsg::Light::create();
                setColorAndIntensity(*light, *vsg_light);
                vsg_light->name = light->mName.C_Str();
                vsg_light->setValue("light_type", "AREA");
                lightMap[vsg_light->name] = vsg_light;
                break;
            }
            default:
                break;
            }
        }
    }
}

vsg::ref_ptr<vsg::MatrixTransform> SceneConverter::processCoordinateFrame(const vsg::Path& ext)
{
    vsg::CoordinateConvention source_coordinateConvention = vsg::CoordinateConvention::Y_UP;

    if (auto itr = options->formatCoordinateConventions.find(ext); itr != options->formatCoordinateConventions.end())
    {
        source_coordinateConvention = itr->second;
    }

    if (scene->mMetaData)
    {
        int upAxis = 1;
        if (scene->mMetaData->Get("UpAxis", upAxis) == AI_SUCCESS)
        {
            if (upAxis == 0)
                source_coordinateConvention = vsg::CoordinateConvention::X_UP;
            else if (upAxis == 1)
                source_coordinateConvention = vsg::CoordinateConvention::Y_UP;
            else
                source_coordinateConvention = vsg::CoordinateConvention::Z_UP;

            // unclear on how to interpret the UpAxisSign so will leave it unused for now.
            // int upAxisSign = 1;
            // scene->mMetaData->Get("UpAxisSign", upAxisSign);
        }
    }

    vsg::dmat4 matrix;
    if (vsg::transform(source_coordinateConvention, options->sceneCoordinateConvention, matrix))
    {
        return vsg::MatrixTransform::create(matrix);
    }
    else
    {
        return {};
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// assimp ReaderWriter implementation
//
assimp::Implementation::Implementation() :
    _importFlags{aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_OptimizeMeshes | aiProcess_SortByPType | aiProcess_ImproveCacheLocality | aiProcess_GenUVCoords}
{
}

vsg::ref_ptr<vsg::Object> assimp::Implementation::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    Assimp::Importer importer;
    vsg::Path ext = (options && options->extensionHint) ? options->extensionHint : vsg::lowerCaseFileExtension(filename);

    if (importer.IsExtensionSupported(ext.string()))
    {
        vsg::Path filenameToUse = vsg::findFile(filename, options);
        if (!filenameToUse) return {};

        uint32_t flags = _importFlags;
        if (vsg::value<bool>(false, assimp::generate_smooth_normals, options))
        {
            importer.SetPropertyFloat(AI_CONFIG_PP_CT_MAX_SMOOTHING_ANGLE, vsg::value<float>(80.0f, assimp::crease_angle, options));
            flags |= aiProcess_GenSmoothNormals;
        }
        else if (vsg::value<bool>(false, assimp::generate_sharp_normals, options))
        {
            flags |= aiProcess_GenNormals;
        }

        if (auto scene = importer.ReadFile(filenameToUse.string(), flags); scene)
        {
            auto opt = vsg::Options::create(*options);
            opt->paths.insert(opt->paths.begin(), vsg::filePath(filenameToUse));

            SceneConverter converter;
            converter.filename = filename;
            return converter.visit(scene, opt, ext);
        }
        else
        {
            vsg::warn("Failed to load file: ", filename, '\n', importer.GetErrorString());
        }
    }

#if 0
    // Testing the stream support
    std::ifstream file(filename, std::ios::binary);
    auto opt = vsg::Options::create(*options);
    opt->paths.push_back(vsg::filePath(filename));
    opt->extensionHint = vsg::lowerCaseFileExtension(filename);

    return read(file, opt);
#endif

    return {};
}

vsg::ref_ptr<vsg::Object> assimp::Implementation::read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!options || !options->extensionHint) return {};

    Assimp::Importer importer;
    if (importer.IsExtensionSupported(options->extensionHint.string()))
    {
        std::string buffer(1 << 16, 0); // 64kB
        std::string input;

        while (!fin.eof())
        {
            fin.read(&buffer[0], buffer.size());
            const auto bytes_readed = fin.gcount();
            input.append(&buffer[0], bytes_readed);
        }

        if (auto scene = importer.ReadFileFromMemory(input.data(), input.size(), _importFlags); scene)
        {
            SceneConverter converter;
            return converter.visit(scene, options, options->extensionHint);
        }
        else
        {
            vsg::warn("Failed to load file from stream: ", importer.GetErrorString());
        }
    }

    return {};
}

vsg::ref_ptr<vsg::Object> assimp::Implementation::read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!options || !options->extensionHint) return {};

    Assimp::Importer importer;
    if (importer.IsExtensionSupported(options->extensionHint.string()))
    {
        if (auto scene = importer.ReadFileFromMemory(ptr, size, _importFlags); scene)
        {
            SceneConverter converter;
            return converter.visit(scene, options, options->extensionHint);
        }
        else
        {
            vsg::warn("Failed to load file from memory: ", importer.GetErrorString());
        }
    }
    return {};
}
