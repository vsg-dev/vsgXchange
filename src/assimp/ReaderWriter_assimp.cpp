#include "ReaderWriter_assimp.h"
#include "../stbi/ReaderWriter_stbi.h"

#include "assimp_vertex.h"
#include "assimp_phong.h"
#include "assimp_pbr.h"

#include <stack>
#include <strstream>
#include <iostream>
#include <cmath>

#include <vsg/io/FileSystem.h>
#include <vsg/core/Array3D.h>
#include <vsg/commands/Draw.h>
#include <vsg/commands/DrawIndexed.h>
#include <vsg/commands/PushConstants.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/state/StateGroup.h>
#include <vsg/vk/State.h>
#include <vsg/commands/Commands.h>
#include <vsg/commands/BindIndexBuffer.h>
#include <vsg/commands/BindVertexBuffers.h>
#include <vsg/state/DescriptorSet.h>
#include <vsg/state/DescriptorBuffer.h>
#include <vsg/state/DescriptorImage.h>
#include <vsg/io/read.h>
#include <vsg/maths/transform.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/pbrmaterial.h>


namespace {

const std::string kDiffuseMapKey("VSG_DIFFUSE_MAP");
const std::string kSpecularMapKey("VSG_SPECULAR_MAP");
const std::string kAmbientMapKey("VSG_AMBIENT_MAP");
const std::string kEmissiveMapKey("VSG_EMISSIVE_MAP");
const std::string kHeightMapKey("VSG_HEIGHT_MAP");
const std::string kNormalMapKey("VSG_NORMAL_MAP");
const std::string kShininessMapKey("VSG_SHININESS_MAP");
const std::string kOpacityMapKey("VSG_OPACITY_MAP");
const std::string kDisplacementMapKey("VSG_DISPLACEMENT_MAP");
const std::string kLightmapMapKey("VSG_LIGHTMAP_MAP");
const std::string kReflectionMapKey("VSG_REFLECTION_MAP");
const std::string kMetallRoughnessMapKey("VSG_METALLROUGHNESS_MAP");

struct Material
{
    aiColor4D ambient{0.0f, 0.0f, 0.0f, 1.0f};
    aiColor4D diffuse{0.0f, 0.0f, 0.0f, 1.0f};
    aiColor4D specular{0.0f, 0.0f, 0.0f, 1.0f};
    aiColor4D emissive{1.0f, 1.0f, 1.0f, 1.0f};
    aiColor4D shininessTwoSided{0.0f, 0.0f, 0.0f, 0.0f};
    float alphaMask{1.0};
    float alphaMaskCutoff{0.5};

    vsg::ref_ptr<vsg::Data> toData()
    {
        auto buffer = vsg::ubyteArray::create(sizeof(Material));
        std::memcpy(buffer->data(), &ambient.r, sizeof(Material));
        return buffer;
    }
};

struct PbrMaterial
{
    aiColor4D baseColorFactor{0.0, 0.0, 0.0, 1.0};
    aiColor4D emissiveFactor{0.0, 0.0, 0.0, 1.0};
    aiColor4D diffuseFactor{0.0, 0.0, 0.0, 1.0};
    aiColor4D specularFactor{0.0, 0.0, 0.0, 1.0};
    float metallicFactor{1.0f};
    float roughnessFactor{1.0f};
    float alphaMask{1.0f};
    float alphaMaskCutoff{0.5f};

    vsg::ref_ptr<vsg::Data> toData()
    {
        auto buffer = vsg::ubyteArray::create(sizeof(PbrMaterial));
        std::memcpy(buffer->data(), &baseColorFactor.r, sizeof(PbrMaterial));
        return buffer;
    }
};

static vsg::vec4 kBlackColor{0.0, 0.0, 0.0, 0.0};
static vsg::vec4 kWhiteColor{1.0, 1.0, 1.0, 1.0};
static vsg::vec4 kNormalColor{127.0f / 255.0f, 127.0f / 255.0f, 1.0f, 1.0f};

vsg::ref_ptr<vsg::Data> createTexture(const vsg::vec4 &color)
{
    auto vsg_data = vsg::vec4Array2D::create(1, 1, vsg::Data::Layout{VK_FORMAT_R32G32B32A32_SFLOAT});
    std::fill(vsg_data->begin(), vsg_data->end(), color);
    return vsg_data;
}

static auto kWhiteData = createTexture(kWhiteColor);
static auto kBlackData = createTexture(kBlackColor);
static auto kNormalData = createTexture(kNormalColor);

static std::string processGLSLShaderSource(const std::string &source, const std::vector<std::string> &defines)
{
    // trim leading spaces/tabs
    auto trimLeading = [](std::string& str) {
        size_t startpos = str.find_first_not_of(" \t");
        if (std::string::npos != startpos)
        {
            str = str.substr(startpos);
        }
    };

    // trim trailing spaces/tabs/newlines
    auto trimTrailing = [](std::string& str) {
        size_t endpos = str.find_last_not_of(" \t\n");
        if (endpos != std::string::npos)
        {
            str = str.substr(0, endpos + 1);
        }
    };

    // sanitise line by triming leading and trailing characters
    auto sanitise = [&trimLeading, &trimTrailing](std::string& str) {
        trimLeading(str);
        trimTrailing(str);
    };

    // return true if str starts with match string
    auto startsWith = [](const std::string& str, const std::string& match) {
        return str.compare(0, match.length(), match) == 0;
    };

    // returns the string between the start and end character
    auto stringBetween = [](const std::string& str, const char& startChar, const char& endChar) {
        auto start = str.find_first_of(startChar);
        if (start == std::string::npos) return std::string();

        auto end = str.find_first_of(endChar, start);
        if (end == std::string::npos) return std::string();

        if ((end - start) - 1 == 0) return std::string();

        return str.substr(start + 1, (end - start) - 1);
    };

    auto split = [](const std::string& str, const char& seperator) {
        std::vector<std::string> elements;

        std::string::size_type prev_pos = 0, pos = 0;

        while ((pos = str.find(seperator, pos)) != std::string::npos)
        {
            auto substring = str.substr(prev_pos, pos - prev_pos);
            elements.push_back(substring);
            prev_pos = ++pos;
        }

        elements.push_back(str.substr(prev_pos, pos - prev_pos));

        return elements;
    };

    auto addLine = [](std::ostringstream& ss, const std::string& line) {
        ss << line << "\n";
    };

    std::istringstream iss(source);
    std::ostringstream headerstream;
    std::ostringstream sourcestream;

    const std::string versionmatch = "#version";
    const std::string importdefinesmatch = "#pragma import_defines";

    std::vector<std::string> finaldefines;

    for (std::string line; std::getline(iss, line);)
    {
        std::string sanitisedline = line;
        sanitise(sanitisedline);

        // is it the version
        if (startsWith(sanitisedline, versionmatch))
        {
            addLine(headerstream, line);
        }
        // is it the defines import
        else if (startsWith(sanitisedline, importdefinesmatch))
        {
            // get the import defines between ()
            auto csv = stringBetween(sanitisedline, '(', ')');
            auto importedDefines = split(csv, ',');

            addLine(headerstream, line);

            // loop the imported defines and see if it's also requested in defines, if so insert a define line
            for (auto importedDef : importedDefines)
            {
                auto sanitiesedImportDef = importedDef;
                sanitise(sanitiesedImportDef);

                auto finditr = std::find(defines.begin(), defines.end(), sanitiesedImportDef);
                if (finditr != defines.end())
                {
                    addLine(headerstream, "#define " + sanitiesedImportDef);
                }
            }
        }
        else
        {
            // standard source line
            addLine(sourcestream, line);
        }
    }

    return headerstream.str() + sourcestream.str();
}

}

using namespace vsgXchange;

ReaderWriter_assimp::ReaderWriter_assimp()
    : _options{vsg::Options::create()}
    , _importFlags{aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_OptimizeMeshes | aiProcess_SortByPType | aiProcess_ImproveCacheLocality | aiProcess_GenUVCoords}
{
    auto readerWriter = vsg::CompositeReaderWriter::create();
    readerWriter->add(ReaderWriter_stbi::create());
    _options->readerWriter = readerWriter;

    auto readSpvFile = [](const uint32_t *content, size_t size, VkShaderStageFlagBits stage) -> vsg::ref_ptr<vsg::ShaderStage> {
        const auto count = size / sizeof(vsg::ShaderModule::SPIRV::value_type);
        vsg::ShaderModule::SPIRV spirv(content, content+count);

        if (auto shader = vsg::ShaderStage::create(stage, "main", spirv); shader.valid())
            return shader;

        return {};
    };

    createDefaultPipelineAndState();
}

vsg::ref_ptr<vsg::GraphicsPipeline> ReaderWriter_assimp::createPipeline(vsg::ref_ptr<vsg::ShaderStage> vs, vsg::ref_ptr<vsg::ShaderStage> fs, vsg::ref_ptr<vsg::DescriptorSetLayout> descriptorSetLayout, bool doubleSided, bool enableBlend) const
{
    vsg::PushConstantRanges pushConstantRanges{
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls autoaatically provided by the VSG's DispatchTraversal
    };

    vsg::VertexInputState::Bindings vertexBindingsDescriptions{
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // normal data
        VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // texcoord data
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions{
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // normal data
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT,    0}  // texcoord data
    };

    auto rasterState = vsg::RasterizationState::create();
    rasterState->cullMode = doubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

    auto colorBlendState = vsg::ColorBlendState::create();
    colorBlendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{
        {enableBlend, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_SUBTRACT, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT}
    };

    vsg::GraphicsPipelineStates pipelineStates{
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        vsg::InputAssemblyState::create(),
        rasterState,
        vsg::MultisampleState::create(),
        colorBlendState,
        vsg::DepthStencilState::create()
    };

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
    return vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vs, fs}, pipelineStates);
}

void ReaderWriter_assimp::createDefaultPipelineAndState()
{
    auto vertexShader = vsg::ShaderStage::create(VK_SHADER_STAGE_VERTEX_BIT, "main", processGLSLShaderSource(assimp_vertex, {}));
    auto fragmentShader = vsg::ShaderStage::create(VK_SHADER_STAGE_FRAGMENT_BIT, "main", processGLSLShaderSource(assimp_phong, {}));

    vsg::DescriptorSetLayoutBindings descriptorBindings{
        {10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    };

    _defaultPipeline = createPipeline(vertexShader, fragmentShader, vsg::DescriptorSetLayout::create(descriptorBindings));

    // create texture image and associated DescriptorSets and binding
    Material mat;
    auto material = vsg::DescriptorBuffer::create(mat.toData(), 10);

    auto descriptorSet = vsg::DescriptorSet::create(_defaultPipeline->layout->setLayouts.front(), vsg::Descriptors{material});
    _defaultState = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, _defaultPipeline->layout, 0, descriptorSet);
}

vsg::ref_ptr<vsg::Object> ReaderWriter_assimp::processScene(const aiScene *scene, vsg::ref_ptr<const vsg::Options> options) const
{
    int upAxis = 1, upAxisSign = 1;

    if (scene->mMetaData)
    {
        if (!scene->mMetaData->Get("UpAxis", upAxis))
            upAxis = 1;

        if (!scene->mMetaData->Get("UpAxisSign", upAxisSign))
            upAxisSign = 1;
    }

    // Process materials
    //auto pipelineLayout = _defaultPipeline->layout;
    auto stateSets = processMaterials(scene, options);

    auto root = vsg::MatrixTransform::create();

    if (upAxis == 1)
        root->setMatrix(vsg::rotate(vsg::PIf * 0.5f, (float)upAxisSign, 0.0f, 0.0f));

    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(vsg::BindGraphicsPipeline::create(_defaultPipeline));
    scenegraph->add(_defaultState);

    std::stack<std::pair<aiNode*, vsg::ref_ptr<vsg::Group>>> nodes;
    nodes.push({scene->mRootNode, scenegraph});

    while (!nodes.empty())
    {
        auto [node, parent] = nodes.top();

        aiMatrix4x4 m = node->mTransformation;
        m.Transpose();

        auto xform = vsg::MatrixTransform::create();
        xform->setMatrix(vsg::mat4((float*)&m));
        parent->addChild(xform);

        for (unsigned int i=0; i<node->mNumMeshes; ++i)
        {
            auto mesh = scene->mMeshes[node->mMeshes[i]];
            vsg::ref_ptr<vsg::vec3Array> vertices(new vsg::vec3Array(mesh->mNumVertices));
            vsg::ref_ptr<vsg::vec3Array> normals(new vsg::vec3Array(mesh->mNumVertices));
            vsg::ref_ptr<vsg::vec2Array> texcoords(new vsg::vec2Array(mesh->mNumVertices));
            std::vector<unsigned int> indices;

            for (unsigned int j=0; j<mesh->mNumVertices; ++j)
            {
                vertices->at(j) = vsg::vec3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);

                if (mesh->mNormals)
                {
                    normals->at(j) = vsg::vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z);
                }
                else
                {
                    normals->at(j) = vsg::vec3(0,0,0);
                }

                if (mesh->mTextureCoords[0])
                {
                    texcoords->at(j) = vsg::vec2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y);
                }
                else
                {
                    texcoords->at(j) = vsg::vec2(0,0);
                }
            }

            for (unsigned int j=0; j<mesh->mNumFaces; ++j)
            {
                const auto &face = mesh->mFaces[j];

                for (unsigned int k=0; k<face.mNumIndices; ++k)
                    indices.push_back(face.mIndices[k]);
            }

            vsg::ref_ptr<vsg::Data> vsg_indices;

            if (indices.size() < std::numeric_limits<uint16_t>::max())
            {
                auto myindices = vsg::ushortArray::create(static_cast<uint16_t>(indices.size()));
                std::copy(indices.begin(), indices.end(), myindices->data());
                vsg_indices = myindices;
            }
            else
            {
                auto myindices = vsg::uintArray::create(static_cast<uint32_t>(indices.size()));
                std::copy(indices.begin(), indices.end(), myindices->data());
                vsg_indices = myindices;
            }

            auto stategroup = vsg::StateGroup::create();
            xform->addChild(stategroup);

            //qCDebug(lc) << "Using material:" << scene->mMaterials[mesh->mMaterialIndex]->GetName().C_Str();
            if (mesh->mMaterialIndex < stateSets.size())
            {
                auto state = stateSets[mesh->mMaterialIndex];

                stategroup->add(state.first);
                stategroup->add(state.second);
            }

            stategroup->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{vertices, normals, texcoords}));
            stategroup->addChild(vsg::BindIndexBuffer::create(vsg_indices));
            stategroup->addChild(vsg::DrawIndexed::create(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0));
        }

        nodes.pop();
        for (unsigned int i=0; i<node->mNumChildren; ++i)
            nodes.push({node->mChildren[i], xform});
    }

    root->addChild(scenegraph);

    return root;
}

ReaderWriter_assimp::BindState ReaderWriter_assimp::processMaterials(const aiScene *scene, vsg::ref_ptr<const vsg::Options> options) const
{
    BindState bindDescriptorSets;
    bindDescriptorSets.reserve(scene->mNumMaterials);

    auto getWrapMode = [](aiTextureMapMode mode) {
        switch (mode)
        {
        case aiTextureMapMode_Wrap: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case aiTextureMapMode_Clamp: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case aiTextureMapMode_Decal: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case aiTextureMapMode_Mirror: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        default: break;
        }
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    };

    auto getTexture = [&](const aiScene &scene, aiMaterial &material, aiTextureType type, std::vector<std::string> &defines) -> vsg::SamplerImage {
        aiString texPath;
        std::array<aiTextureMapMode, 3> wrapMode{aiTextureMapMode_Wrap, aiTextureMapMode_Wrap, aiTextureMapMode_Wrap};

        if (material.GetTexture(type, 0, &texPath, nullptr, nullptr, nullptr, nullptr, wrapMode.data()) == AI_SUCCESS)
        {
            vsg::SamplerImage samplerImage;

            if (texPath.data[0] == '*')
            {
                const auto texIndex = std::atoi(texPath.C_Str()+1);
                const auto texture = scene.mTextures[texIndex];

                //qCDebug(lc) << "Handle embedded texture" << texPath.C_Str() << texIndex << texture->achFormatHint << texture->mWidth << texture->mHeight;

                if (texture->mWidth > 0 && texture->mHeight == 0)
                {
                    std::istrstream stream((const char*)texture->pcData, texture->mWidth);

                    auto options = vsg::Options::create(*_options);
                    options->extensionHint = texture->achFormatHint;
                    samplerImage.data = vsg::read_cast<vsg::Data>(stream, options);
                }
            }
            else
            {
                const std::string filename = vsg::findFile(texPath.C_Str(), options);

                if (samplerImage.data = vsg::read_cast<vsg::Data>(filename, _options); !samplerImage.data.valid())
                {
                    std::cerr << "Failed to load texture: " << filename << std::endl;
                }

            }

            switch (type)
            {
            case aiTextureType_DIFFUSE: defines.push_back(kDiffuseMapKey); break;
            case aiTextureType_SPECULAR: defines.push_back(kSpecularMapKey); break;
            case aiTextureType_AMBIENT: defines.push_back(kAmbientMapKey); break;
            case aiTextureType_EMISSIVE: defines.push_back(kEmissiveMapKey); break;
            case aiTextureType_HEIGHT: defines.push_back(kHeightMapKey); break;
            case aiTextureType_NORMALS: defines.push_back(kNormalMapKey); break;
            case aiTextureType_SHININESS: defines.push_back(kShininessMapKey); break;
            case aiTextureType_OPACITY: defines.push_back(kOpacityMapKey); break;
            case aiTextureType_DISPLACEMENT: defines.push_back(kDisplacementMapKey); break;
            case aiTextureType_LIGHTMAP: defines.push_back(kLightmapMapKey); break;
            case aiTextureType_REFLECTION: defines.push_back(kReflectionMapKey); break;
            case aiTextureType_UNKNOWN: defines.push_back(kMetallRoughnessMapKey); break;
            default: break;
            }

            samplerImage.sampler = vsg::Sampler::create();

            samplerImage.sampler->addressModeU = getWrapMode(wrapMode[0]);
            samplerImage.sampler->addressModeV = getWrapMode(wrapMode[1]);
            samplerImage.sampler->addressModeW = getWrapMode(wrapMode[2]);

            samplerImage.sampler->anisotropyEnable = VK_TRUE;
            samplerImage.sampler->maxAnisotropy = 16.0f;

            samplerImage.sampler->maxLod = samplerImage.data->getLayout().maxNumMipmaps;

            if (samplerImage.sampler->maxLod <= 1.0)
            {
                //                if (texPath.length > 0)
                //                    std::cout << "Auto generating mipmaps for texture: " << scene.GetShortFilename(texPath.C_Str()) << std::endl;;

                // Calculate maximum lod level
                auto maxDim = std::max(samplerImage.data->width(), samplerImage.data->height());
                samplerImage.sampler->maxLod = std::floor(std::log2f(static_cast<float>(maxDim)));
            }

            return samplerImage;
        }

        return {};
    };

    for (unsigned int i=0; i<scene->mNumMaterials; ++i)
    {
        const auto material = scene->mMaterials[i];

        bool hasPbrSpecularGlossiness{false};
        material->Get(AI_MATKEY_GLTF_PBRSPECULARGLOSSINESS, hasPbrSpecularGlossiness);

        if (PbrMaterial pbr; material->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR, pbr.baseColorFactor) == AI_SUCCESS || hasPbrSpecularGlossiness)
        {
            // PBR path
            std::vector<std::string> defines;
            bool isTwoSided{false};

            if (hasPbrSpecularGlossiness)
            {
                defines.push_back("VSG_WORKFLOW_SPECGLOSS");
                material->Get(AI_MATKEY_COLOR_DIFFUSE, pbr.diffuseFactor);
                material->Get(AI_MATKEY_COLOR_SPECULAR, pbr.specularFactor);

                if (material->Get(AI_MATKEY_GLTF_PBRSPECULARGLOSSINESS_GLOSSINESS_FACTOR, pbr.specularFactor.a) != AI_SUCCESS)
                {
                    if (float shininess; material->Get(AI_MATKEY_SHININESS, shininess))
                        pbr.specularFactor.a = shininess / 1000;
                }
            }
            else
            {
                material->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, pbr.metallicFactor);
                material->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, pbr.roughnessFactor);
            }

            material->Get(AI_MATKEY_COLOR_EMISSIVE, pbr.emissiveFactor);
            material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, pbr.alphaMaskCutoff);


            if (material->Get(AI_MATKEY_TWOSIDED, isTwoSided); isTwoSided)
                defines.push_back("VSG_TWOSIDED");


            vsg::DescriptorSetLayoutBindings descriptorBindings{
                {10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
            };
            vsg::Descriptors descList;


            auto buffer = vsg::DescriptorBuffer::create(pbr.toData(), 10);
            descList.push_back(buffer);

            if (auto samplerImage = getTexture(*scene, *material, aiTextureType_DIFFUSE, defines); samplerImage.data.valid())
            {
                auto diffuseTexture = vsg::DescriptorImage::create(samplerImage, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(diffuseTexture);
                descriptorBindings.push_back({0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
            }

            if (auto samplerImage = getTexture(*scene, *material, aiTextureType_EMISSIVE, defines); samplerImage.data.valid())
            {
                auto emissiveTexture = vsg::DescriptorImage::create(samplerImage, 4, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(emissiveTexture);
                descriptorBindings.push_back({4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
            }

            if (auto samplerImage = getTexture(*scene, *material, aiTextureType_LIGHTMAP, defines); samplerImage.data.valid())
            {
                auto aoTexture = vsg::DescriptorImage::create(samplerImage, 3, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(aoTexture);
                descriptorBindings.push_back({3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
            }

            if (auto samplerImage = getTexture(*scene, *material, aiTextureType_NORMALS, defines); samplerImage.data.valid())
            {
                auto normalTexture = vsg::DescriptorImage::create(samplerImage, 2, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(normalTexture);
                descriptorBindings.push_back({2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
            }

            if (auto samplerImage = getTexture(*scene, *material, aiTextureType_UNKNOWN, defines); samplerImage.data.valid())
            {
                auto mrTexture = vsg::DescriptorImage::create(samplerImage, 1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(mrTexture);
                descriptorBindings.push_back({1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
            }

            if (auto samplerImage = getTexture(*scene, *material, aiTextureType_SPECULAR, defines); samplerImage.data.valid())
            {
                auto texture = vsg::DescriptorImage::create(samplerImage, 5, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(texture);
                descriptorBindings.push_back({5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
            }

            auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
            auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, descList);

            auto vertexShader = vsg::ShaderStage::create(VK_SHADER_STAGE_VERTEX_BIT, "main", processGLSLShaderSource(assimp_vertex, defines));
            auto fragmentShader = vsg::ShaderStage::create(VK_SHADER_STAGE_FRAGMENT_BIT, "main", processGLSLShaderSource(assimp_pbr, defines));
            auto pipeline = createPipeline(vertexShader, fragmentShader, descriptorSetLayout, isTwoSided);
            auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, 0, descriptorSet);

            bindDescriptorSets.push_back({vsg::BindGraphicsPipeline::create(pipeline), bindDescriptorSet});
        }
        else
        {
            // Phong shading
            Material mat;
            std::vector<std::string> defines;

            material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, mat.alphaMaskCutoff);
            material->Get(AI_MATKEY_COLOR_DIFFUSE, mat.diffuse);
            material->Get(AI_MATKEY_COLOR_AMBIENT, mat.ambient);
            material->Get(AI_MATKEY_COLOR_EMISSIVE, mat.emissive);
            material->Get(AI_MATKEY_COLOR_SPECULAR, mat.specular);

            aiShadingMode shadingModel = aiShadingMode_Phong;
            material->Get(AI_MATKEY_SHADING_MODEL, shadingModel);

            bool isTwoSided{false};
            if (material->Get(AI_MATKEY_TWOSIDED, isTwoSided) == AI_SUCCESS && isTwoSided)
                defines.push_back("VSG_TWOSIDED");

            unsigned int maxValue = 1;
            float strength = 1.0f;
            if (aiGetMaterialFloatArray(material, AI_MATKEY_SHININESS, &mat.shininessTwoSided.r, &maxValue) == AI_SUCCESS)
            {
                maxValue = 1;
                if (aiGetMaterialFloatArray(material, AI_MATKEY_SHININESS_STRENGTH, &strength, &maxValue) == AI_SUCCESS)
                    mat.shininessTwoSided.r *= strength;
            }
            else
            {
                mat.shininessTwoSided.r = 0.0f;
                mat.specular = aiColor4D(0.0f, 0.0f, 0.0f, 0.0f);
            }

            if (mat.shininessTwoSided.r < 0.01f)
            {
                mat.shininessTwoSided.r = 0.0f;
                mat.specular = aiColor4D(0.0f, 0.0f, 0.0f, 0.0f);
            }

            vsg::DescriptorSetLayoutBindings descriptorBindings{
                {10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
            };
            vsg::Descriptors descList;

            auto buffer = vsg::DescriptorBuffer::create(mat.toData(), 10);
            descList.push_back(buffer);

            if (auto samplerImage = getTexture(*scene, *material, aiTextureType_DIFFUSE, defines); samplerImage.data.valid())
            {
                auto diffuseTexture = vsg::DescriptorImage::create(samplerImage, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(diffuseTexture);
                descriptorBindings.push_back({0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
            }

            if (auto samplerImage = getTexture(*scene, *material, aiTextureType_EMISSIVE, defines); samplerImage.data.valid())
            {
                auto emissiveTexture = vsg::DescriptorImage::create(samplerImage, 4, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(emissiveTexture);
                descriptorBindings.push_back({4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
            }

            if (auto samplerImage = getTexture(*scene, *material, aiTextureType_LIGHTMAP, defines); samplerImage.data.valid())
            {
                auto aoTexture = vsg::DescriptorImage::create(samplerImage, 3, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(aoTexture);
                descriptorBindings.push_back({3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
            }

            if (auto samplerImage = getTexture(*scene, *material, aiTextureType_NORMALS, defines); samplerImage.data.valid())
            {
                auto normalTexture = vsg::DescriptorImage::create(samplerImage, 2, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(normalTexture);
                descriptorBindings.push_back({2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
            }

            auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
            auto vertexShader = vsg::ShaderStage::create(VK_SHADER_STAGE_VERTEX_BIT, "main", processGLSLShaderSource(assimp_vertex, defines));
            auto fragmentShader = vsg::ShaderStage::create(VK_SHADER_STAGE_FRAGMENT_BIT, "main", processGLSLShaderSource(assimp_phong, defines));

            auto pipeline = createPipeline(vertexShader, fragmentShader, descriptorSetLayout, isTwoSided);

            auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, descList);
            auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, 0, descriptorSet);

            bindDescriptorSets.push_back({vsg::BindGraphicsPipeline::create(pipeline), bindDescriptorSet});
        }
    }

    return bindDescriptorSets;
}

vsg::ref_ptr<vsg::Object> ReaderWriter_assimp::read(const vsg::Path &filename, vsg::ref_ptr<const vsg::Options> options) const
{
    Assimp::Importer importer;

    if (const auto ext = vsg::fileExtension(filename); importer.IsExtensionSupported(ext))
    {
        if (auto scene = importer.ReadFile(filename, _importFlags); scene)
        {
            auto opt = vsg::Options::create(*options);
            opt->paths.push_back(vsg::filePath(filename));

            return processScene(scene, opt);
        }
        else
        {
            std::cerr << "Failed to load file: " << filename << std::endl << importer.GetErrorString() << std::endl;
        }
    }

#if 0
    // Testing the stream support
    std::ifstream file(filename, std::ios::binary);
    auto opt = vsg::Options::create(*options);
    opt->paths.push_back(vsg::filePath(filename));
    opt->extensionHint = vsg::fileExtension(filename);

    return read(file, opt);
#endif

    return {};
}

vsg::ref_ptr<vsg::Object> ReaderWriter_assimp::read(std::istream &fin, vsg::ref_ptr<const vsg::Options> options) const
{
    Assimp::Importer importer;

    if (importer.IsExtensionSupported(options->extensionHint))
    {
        std::string buffer(1<<16, 0); // 64kB
        std::string input;

        while (!fin.eof())
        {
            fin.read(&buffer[0], buffer.size());
            const auto bytes_readed = fin.gcount();
            input.append(&buffer[0], bytes_readed);
        }

        if (auto scene = importer.ReadFileFromMemory(input.data(), input.size(), _importFlags); scene)
        {
            return processScene(scene, options);
        }
        else
        {
            std::cerr << "Failed to load file from stream: " << importer.GetErrorString() << std::endl;
        }
    }

    return {};
}

