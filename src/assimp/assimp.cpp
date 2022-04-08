/* <editor-fold desc="MIT License">

Copyright(c) 2021 André Normann & Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgXchange/models.h>

#include "shaders/assimp_vert.cpp"
#include "shaders/assimp_pbr_frag.cpp"
#include "shaders/assimp_phong_frag.cpp"

#include <cmath>
#include <sstream>
#include <stack>

#include <vsg/all.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#if (ASSIMP_VERSION_MAJOR==5 && ASSIMP_VERSION_MINOR==0)
    #include <assimp/pbrmaterial.h>
    #define AI_MATKEY_BASE_COLOR AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR
    #define AI_MATKEY_GLOSSINESS_FACTOR AI_MATKEY_GLTF_PBRSPECULARGLOSSINESS_GLOSSINESS_FACTOR
    #define AI_MATKEY_METALLIC_FACTOR AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR
    #define AI_MATKEY_ROUGHNESS_FACTOR AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR
#else
    #include <assimp/material.h>

    #if (ASSIMP_VERSION_MAJOR==5 && ASSIMP_VERSION_MINOR==1 && ASSIMP_VERSION_PATCH==0)
        #define AI_MATKEY_GLTF_ALPHACUTOFF "$mat.gltf.alphaCutoff", 0, 0
        #define AI_MATKEY_GLTF_ALPHAMODE "$mat.gltf.alphaMode", 0, 0
    #else
        #include <assimp/GltfMaterial.h>
    #endif
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

    static vsg::vec3 convert(const aiVector3D& v) { return vsg::vec3(v[0], v[1], v[2]); }
    static vsg::dvec3 dconvert(const aiVector3D& v) { return vsg::dvec3(v[0], v[1], v[2]); }
    static vsg::vec3 convert(const aiColor3D& v) { return vsg::vec3(v[0], v[1], v[2]); }

    const std::string kDiffuseMapKey{"VSG_DIFFUSE_MAP"};
    const std::string kSpecularMapKey{"VSG_SPECULAR_MAP"};
    const std::string kAmbientMapKey{"VSG_AMBIENT_MAP"};
    const std::string kEmissiveMapKey{"VSG_EMISSIVE_MAP"};
    const std::string kHeightMapKey{"VSG_HEIGHT_MAP"};
    const std::string kNormalMapKey{"VSG_NORMAL_MAP"};
    const std::string kShininessMapKey{"VSG_SHININESS_MAP"};
    const std::string kOpacityMapKey{"VSG_OPACITY_MAP"};
    const std::string kDisplacementMapKey{"VSG_DISPLACEMENT_MAP"};
    const std::string kLightmapMapKey{"VSG_LIGHTMAP_MAP"};
    const std::string kReflectionMapKey{"VSG_REFLECTION_MAP"};
    const std::string kMetallRoughnessMapKey{"VSG_METALLROUGHNESS_MAP"};

    vsg::ref_ptr<vsg::Data> createTexture(const vsg::vec4& color)
    {
        auto vsg_data = vsg::vec4Array2D::create(1, 1, color, vsg::Data::Layout{VK_FORMAT_R32G32B32A32_SFLOAT});
        return vsg_data;
    }
    const vsg::vec4 kBlackColor{0.0, 0.0, 0.0, 0.0};
    const vsg::vec4 kWhiteColor{1.0, 1.0, 1.0, 1.0};
    const vsg::vec4 kNormalColor{127.0f / 255.0f, 127.0f / 255.0f, 1.0f, 1.0f};

    vsg::ref_ptr<vsg::Data> kWhiteData = createTexture(kWhiteColor);
    vsg::ref_ptr<vsg::Data> kBlackData = createTexture(kBlackColor);
    vsg::ref_ptr<vsg::Data> kNormalData = createTexture(kNormalColor);

    using CameraMap = std::map<std::string, vsg::ref_ptr<vsg::Camera>>;
    static CameraMap processCameras(const aiScene* scene);

    using LightMap = std::map<std::string, vsg::ref_ptr<vsg::Light>>;
    static LightMap processLights(const aiScene* scene);

    static vsg::ref_ptr<vsg::MatrixTransform> processCoordinateFrame(const aiScene* scene, vsg::ref_ptr<const vsg::Options> options, const vsg::Path& ext);

    class SceneConverter;

private:
    using StateCommandPtr = vsg::ref_ptr<vsg::StateCommand>;
    using State = std::pair<StateCommandPtr, StateCommandPtr>;
    using BindState = std::vector<State>;

    vsg::ref_ptr<vsg::GraphicsPipeline> createPipeline(vsg::ref_ptr<vsg::ShaderStage> vs, vsg::ref_ptr<vsg::ShaderStage> fs, vsg::ref_ptr<vsg::DescriptorSetLayout> descriptorSetLayout, bool doubleSided = false, bool enableBlend = false) const;
    void createDefaultPipelineAndState();

    vsg::ref_ptr<vsg::Object> processScene(const aiScene* scene, vsg::ref_ptr<const vsg::Options> options, const vsg::Path& ext) const;

    BindState processMaterials(const aiScene* scene, vsg::ref_ptr<const vsg::Options> options) const;

    static VkSamplerAddressMode getWrapMode(aiTextureMapMode mode)
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

    SamplerData getTexture(const aiScene* scene, vsg::ref_ptr<const vsg::Options> options, aiMaterial& material, aiTextureType type, std::vector<std::string>& defines) const
    {
        aiString texPath;
        std::array<aiTextureMapMode, 3> wrapMode{{aiTextureMapMode_Wrap, aiTextureMapMode_Wrap, aiTextureMapMode_Wrap}};

        if (material.GetTexture(type, 0, &texPath, nullptr, nullptr, nullptr, nullptr, wrapMode.data()) == AI_SUCCESS)
        {
            SamplerData samplerImage;

            if (texPath.data[0] == '*')
            {
                const auto texIndex = std::atoi(texPath.C_Str() + 1);
                const auto texture = scene->mTextures[texIndex];

                //qCDebug(lc) << "Handle embedded texture" << texPath.C_Str() << texIndex << texture->achFormatHint << texture->mWidth << texture->mHeight;

                if (texture->mWidth > 0 && texture->mHeight == 0)
                {
                    auto imageOptions = vsg::Options::create(*options);
                    imageOptions->extensionHint = texture->achFormatHint;
                    if (samplerImage.data = vsg::read_cast<vsg::Data>(reinterpret_cast<const uint8_t*>(texture->pcData), texture->mWidth, imageOptions); !samplerImage.data.valid())
                        return {};
                }
            }
            else
            {
                const std::string filename = vsg::findFile(texPath.C_Str(), options);

                if (samplerImage.data = vsg::read_cast<vsg::Data>(filename, options); !samplerImage.data.valid())
                {
                    std::cerr << "Failed to load texture: " << filename << " texPath = " << texPath.C_Str() << std::endl;
                    return {};
                }
            }

            switch (type)
            {
            case aiTextureType_DIFFUSE: defines.push_back(kDiffuseMapKey); break;
            case aiTextureType_SPECULAR: defines.push_back(kSpecularMapKey); break;
            case aiTextureType_EMISSIVE: defines.push_back(kEmissiveMapKey); break;
            case aiTextureType_HEIGHT: defines.push_back(kHeightMapKey); break;
            case aiTextureType_NORMALS: defines.push_back(kNormalMapKey); break;
            case aiTextureType_SHININESS: defines.push_back(kShininessMapKey); break;
            case aiTextureType_OPACITY: defines.push_back(kOpacityMapKey); break;
            case aiTextureType_DISPLACEMENT: defines.push_back(kDisplacementMapKey); break;
            case aiTextureType_AMBIENT:
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
    }

    static bool hasAlphaBlend(const aiMaterial* material)
    {
        aiString alphaMode;
        float opacity = 1.0;
        if ((material->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS && alphaMode == aiString("BLEND"))
             || (material->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS && opacity < 1.0))
            return true;
        return false;
    }

    struct BlendDetector : vsg::Inherit<vsg::Visitor, BlendDetector>
    {
        void apply(vsg::Object& object) override
        {
            object.traverse(*this);
        }

        void apply(vsg::BindGraphicsPipeline& bgp) override
        {
            for (auto gps : bgp.pipeline->pipelineStates)
            {
                gps->accept(*this);
            }
            bgp.traverse(*this);
        }

        void apply(vsg::ColorBlendState& cbs) override
        {
          for (auto attachment : cbs.attachments)
            if (attachment.blendEnable)
                result = true;
        }

        bool result{false};
    };

    vsg::ref_ptr<vsg::GraphicsPipeline> _defaultPipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> _defaultState;
    const uint32_t _importFlags;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// assimp ReaderWriter fascade
//
assimp::assimp() :
    _implementation(new assimp::Implementation())
{
    // std::cout<<"ASSIMP_VERSION_MAJOR "<<ASSIMP_VERSION_MAJOR<<std::endl;
    // std::cout<<"ASSIMP_VERSION_MINOR "<<ASSIMP_VERSION_MINOR<<std::endl;
    // std::cout<<"ASSIMP_VERSION_PATCH "<<ASSIMP_VERSION_PATCH<<std::endl;
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
    features.optionNameTypeMap[assimp::original_converter] = vsg::type_name<bool>();

    return true;
}

bool assimp::readOptions(vsg::Options& options, vsg::CommandLine& arguments) const
{
    bool result = arguments.readAndAssign<void>(assimp::generate_smooth_normals, &options);
    result = arguments.readAndAssign<void>(assimp::generate_sharp_normals, &options) || result;
    result = arguments.readAndAssign<float>(assimp::crease_angle, &options) || result;
    result = arguments.readAndAssign<void>(assimp::two_sided, &options) || result;
    result = arguments.readAndAssign<void>(assimp::original_converter, &options) || result;
    return result;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// assimp ReaderWriter implementation
//
assimp::Implementation::Implementation() :
    _importFlags{aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_OptimizeMeshes | aiProcess_SortByPType | aiProcess_ImproveCacheLocality | aiProcess_GenUVCoords}
{
    createDefaultPipelineAndState();
}

vsg::ref_ptr<vsg::GraphicsPipeline> assimp::Implementation::createPipeline(vsg::ref_ptr<vsg::ShaderStage> vs, vsg::ref_ptr<vsg::ShaderStage> fs, vsg::ref_ptr<vsg::DescriptorSetLayout> descriptorSetLayout, bool doubleSided, bool enableBlend) const
{
    vsg::PushConstantRanges pushConstantRanges{
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection view, and model matrices, actual push constant calls autoaatically provided by the VSG's DispatchTraversal
    };

    vsg::VertexInputState::Bindings vertexBindingsDescriptions{
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // normal data
        VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX},  // texcoord data
        VkVertexInputBindingDescription{3, sizeof(vsg::vec4), VK_VERTEX_INPUT_RATE_INSTANCE}  // color data
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions{
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // normal data
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},     // texcoord data
        VkVertexInputAttributeDescription{3, 3, VK_FORMAT_R32G32B32A32_SFLOAT, 0}     // texcoord data
    };

    auto rasterState = vsg::RasterizationState::create();
    rasterState->cullMode = doubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

    auto colorBlendState = vsg::ColorBlendState::create();
    colorBlendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{
        {enableBlend, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT}};

    vsg::GraphicsPipelineStates pipelineStates{
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        vsg::InputAssemblyState::create(),
        rasterState,
        vsg::MultisampleState::create(),
        colorBlendState,
        vsg::DepthStencilState::create()};

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
    return vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vs, fs}, pipelineStates);
}

void assimp::Implementation::createDefaultPipelineAndState()
{
    auto vertexShader = assimp_vert();
    auto fragmentShader = assimp_phong_frag();

    vsg::DescriptorSetLayoutBindings descriptorBindings{
        {10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};

    _defaultPipeline = createPipeline(vertexShader, fragmentShader, vsg::DescriptorSetLayout::create(descriptorBindings));

    // create texture image and associated DescriptorSets and binding
    auto mat = vsg::PhongMaterialValue::create();
    auto material = vsg::DescriptorBuffer::create(mat, 10);

    auto descriptorSet = vsg::DescriptorSet::create(_defaultPipeline->layout->setLayouts.front(), vsg::Descriptors{material});
    _defaultState = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, _defaultPipeline->layout, 0, descriptorSet);
}

assimp::Implementation::CameraMap assimp::Implementation::processCameras(const aiScene* scene)
{
    CameraMap cameraMap;
    if (scene->mNumCameras > 0)
    {
        for(unsigned int li = 0; li<scene->mNumCameras; ++li)
        {
            auto* camera = scene->mCameras[li];
            auto vsg_camera = vsg::Camera::create();
            vsg_camera->name = camera->mName.C_Str();

            vsg_camera->viewMatrix = vsg::LookAt::create(
                vsg::dvec3(camera->mPosition[0], camera->mPosition[1], camera->mPosition[2]), // eye
                vsg::dvec3(camera->mLookAt[0], camera->mLookAt[1], camera->mLookAt[2]), // center
                vsg::dvec3(camera->mUp[0], camera->mUp[1], camera->mUp[2]) // up
            );

            double verticalFOV = vsg::degrees( atan(tan(static_cast<double>(camera->mHorizontalFOV) * 0.5) / camera->mAspect) * 2.0);
            vsg_camera->projectionMatrix = vsg::Perspective::create(verticalFOV, camera->mAspect, camera->mClipPlaneNear, camera->mClipPlaneFar);

            // the aiNodes in the scene with the same name as the camera will provide a place to add the camera, this is added in the node handling in the for loop below.
            cameraMap[vsg_camera->name] = vsg_camera;
        }
    }
    return cameraMap;
}

assimp::Implementation::LightMap assimp::Implementation::processLights(const aiScene* scene)
{
    LightMap lightMap;

    if (scene->mNumLights > 0)
    {
        std::cout<<"scene->mNumLights = "<<scene->mNumLights<<std::endl;
        for(unsigned int li = 0; li < scene->mNumLights; ++li)
        {
            auto* light = scene->mLights[li];

            std::cout<<"light "<<light->mName.C_Str()<<std::endl;
            switch(light->mType)
            {
                case(aiLightSource_UNDEFINED):
                {
                    std::cout<<"    light->mType = aiLightSource_UNDEFINED"<<std::endl;
                    auto vsg_light = vsg::Light::create();
                    vsg_light->name = light->mName.C_Str();
                    vsg_light->color = convert(light->mColorDiffuse);
                    vsg_light->setValue("light_type", "UNDEFINED");
                    lightMap[vsg_light->name] = vsg_light;
                    break;
                }
                case(aiLightSource_DIRECTIONAL):
                {
                    std::cout<<"    light->mType = aiLightSource_DIRECTIONAL"<<std::endl;
                    auto vsg_light = vsg::DirectionalLight::create();
                    vsg_light->name = light->mName.C_Str();
                    vsg_light->color = convert(light->mColorDiffuse);
                    vsg_light->direction = dconvert(light->mDirection);
                    lightMap[vsg_light->name] = vsg_light;
                    break;
                }
                case(aiLightSource_POINT):
                {
                    std::cout<<"    light->mType = aiLightSource_POINT"<<std::endl;
                    auto vsg_light = vsg::PointLight::create();
                    vsg_light->name = light->mName.C_Str();
                    vsg_light->color = convert(light->mColorDiffuse);
                    vsg_light->position = dconvert(light->mDirection);
                    lightMap[vsg_light->name] = vsg_light;
                    break;
                }
                case(aiLightSource_SPOT):
                {
                    std::cout<<"    light->mType = aiLightSource_SPOT"<<std::endl;
                    auto vsg_light = vsg::SpotLight::create();
                    vsg_light->name = light->mName.C_Str();
                    vsg_light->color = convert(light->mColorDiffuse);
                    vsg_light->position = dconvert(light->mDirection);
                    vsg_light->direction = dconvert(light->mDirection);
                    vsg_light->innerAngle = light->mAngleInnerCone;
                    vsg_light->outerAngle = light->mAngleOuterCone;
                    lightMap[vsg_light->name] = vsg_light;
                    break;
                }
                case(aiLightSource_AMBIENT):
                {
                    std::cout<<"    light->mType = aiLightSource_AMBIENT"<<std::endl;
                    auto vsg_light = vsg::AmbientLight::create();
                    vsg_light->name = light->mName.C_Str();
                    vsg_light->color = convert(light->mColorDiffuse);
                    lightMap[vsg_light->name] = vsg_light;
                    break;
                }
                case(aiLightSource_AREA):
                {
                    std::cout<<"    light->mType = aiLightSource_AREA"<<std::endl;
                    auto vsg_light = vsg::Light::create();
                    vsg_light->name = light->mName.C_Str();
                    vsg_light->color = convert(light->mColorDiffuse);
                    vsg_light->setValue("light_type", "AREA");
                    lightMap[vsg_light->name] = vsg_light;
                    break;
                }
                default:
                    std::cout<<"    light->mType = "<<light->mType<<std::endl;
                    break;
            }
        }
    }
    return lightMap;
}

vsg::ref_ptr<vsg::MatrixTransform> assimp::Implementation::processCoordinateFrame(const aiScene* scene, vsg::ref_ptr<const vsg::Options> options, const vsg::Path& ext)
{
    vsg::CoordinateConvention source_coordianteConvention = vsg::CoordinateConvention::Y_UP;
    if (auto itr = options->formatCoordinateConventions.find(ext); itr != options->formatCoordinateConventions.end()) source_coordianteConvention = itr->second;

    if (scene->mMetaData)
    {
        int upAxis = 1;
        if (scene->mMetaData->Get("UpAxis", upAxis))
        {
            if (upAxis==1) source_coordianteConvention = vsg::CoordinateConvention::X_UP;
            else if (upAxis==2) source_coordianteConvention = vsg::CoordinateConvention::Y_UP;
            else source_coordianteConvention = vsg::CoordinateConvention::Z_UP;

            // unclear on how to intepret the UpAxisSign so will leave it unused for now.
            // int upAxisSign = 1;
            // scene->mMetaData->Get("UpAxisSign", upAxisSign);
        }
    }

    vsg::dmat4 matrix;
    if (vsg::transform(source_coordianteConvention, options->sceneCoordinateConvention, matrix))
    {
        return vsg::MatrixTransform::create(matrix);
    }
    else
    {
        return {};
    }
}


struct assimp::Implementation::SceneConverter
{
    vsg::ref_ptr<const vsg::Options> options;
    const aiScene* scene = nullptr;
    assimp::Implementation::CameraMap cameraMap;
    assimp::Implementation::LightMap lightMap;

    // TODO flatShadedShaderSet?
    vsg::ref_ptr<vsg::ShaderSet> pbrShaderSet;
    vsg::ref_ptr<vsg::ShaderSet> phongShaderSet;
    vsg::ref_ptr<vsg::SharedObjects> sharedObjects;

    std::vector<vsg::ref_ptr<vsg::DescriptorConfig>> convertedMaterials;
    std::vector<vsg::ref_ptr<vsg::Node>> convertedMeshes;

    vsg::ref_ptr<vsg::ShaderSet> getOrCreatePhrShaderSet()
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

    std::ostream& indent(std::ostream& out, int depth)
    {
        for(int i=0; i<depth; ++i) out.put(' ');
        return out;
    }

    SamplerData convertTexture(const aiMaterial& material, aiTextureType type) const
    {
        aiString texPath;
        aiTextureMapMode wrapMode[]{aiTextureMapMode_Wrap, aiTextureMapMode_Wrap, aiTextureMapMode_Wrap};

        if (material.GetTexture(type, 0, &texPath, nullptr, nullptr, nullptr, nullptr, wrapMode) == AI_SUCCESS)
        {
            SamplerData samplerImage;

            if (texPath.data[0] == '*')
            {
                const auto texIndex = std::atoi(texPath.C_Str() + 1);
                const auto texture = scene->mTextures[texIndex];

                //qCDebug(lc) << "Handle embedded texture" << texPath.C_Str() << texIndex << texture->achFormatHint << texture->mWidth << texture->mHeight;

                if (texture->mWidth > 0 && texture->mHeight == 0)
                {
                    auto imageOptions = vsg::Options::create(*options);
                    imageOptions->extensionHint = texture->achFormatHint;
                    if (samplerImage.data = vsg::read_cast<vsg::Data>(reinterpret_cast<const uint8_t*>(texture->pcData), texture->mWidth, imageOptions); !samplerImage.data.valid())
                        return {};
                }
            }
            else
            {
                const std::string filename = vsg::findFile(texPath.C_Str(), options);

                if (samplerImage.data = vsg::read_cast<vsg::Data>(filename, options); !samplerImage.data.valid())
                {
                    std::cerr << "Failed to load texture: " << filename << " texPath = " << texPath.C_Str() << std::endl;
                    return {};
                }
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

    void convert(const aiMaterial* material, vsg::DescriptorConfig& convertedMaterial)
    {
        std::cout<<"process(material = "<<material<<")"<<std::endl;
        auto& defines = convertedMaterial.defines;

        vsg::PbrMaterial pbr;
        bool hasPbrSpecularGlossiness = material->Get(AI_MATKEY_COLOR_SPECULAR, pbr.specularFactor);

        convertedMaterial.blending = hasAlphaBlend(material);

        if (material->Get(AI_MATKEY_BASE_COLOR, pbr.baseColorFactor) == AI_SUCCESS || hasPbrSpecularGlossiness)
        {
            // PBR path
            convertedMaterial.shaderSet = getOrCreatePhrShaderSet();

            if (convertedMaterial.blending)
                pbr.alphaMask = 0.0f;

            if (hasPbrSpecularGlossiness)
            {
                defines.push_back("VSG_WORKFLOW_SPECGLOSS");
                material->Get(AI_MATKEY_COLOR_DIFFUSE, pbr.diffuseFactor);

                if (material->Get(AI_MATKEY_GLOSSINESS_FACTOR, pbr.specularFactor.a) != AI_SUCCESS)
                {
                    if (float shininess; material->Get(AI_MATKEY_SHININESS, shininess))
                        pbr.specularFactor.a = shininess / 1000;
                }
            }
            else
            {
                material->Get(AI_MATKEY_METALLIC_FACTOR, pbr.metallicFactor);
                material->Get(AI_MATKEY_ROUGHNESS_FACTOR, pbr.roughnessFactor);
            }

            material->Get(AI_MATKEY_COLOR_EMISSIVE, pbr.emissiveFactor);
            material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, pbr.alphaMaskCutoff);

            bool isTwoSided = vsg::value<bool>(false, assimp::two_sided, options) || (material->Get(AI_MATKEY_TWOSIDED, isTwoSided) == AI_SUCCESS);
            if (isTwoSided) defines.push_back("VSG_TWOSIDED");

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
            material->Get(AI_MATKEY_COLOR_AMBIENT, mat.ambient);
            const auto diffuseResult = material->Get(AI_MATKEY_COLOR_DIFFUSE, mat.diffuse);
            const auto emissiveResult = material->Get(AI_MATKEY_COLOR_EMISSIVE, mat.emissive);
            const auto specularResult = material->Get(AI_MATKEY_COLOR_SPECULAR, mat.specular);

            aiShadingMode shadingModel = aiShadingMode_Phong;
            material->Get(AI_MATKEY_SHADING_MODEL, shadingModel);

            bool isTwoSided{false};
            bool optionFlag{false};
            if(options->getValue(assimp::two_sided, optionFlag) && optionFlag)
            {
                isTwoSided = true;
                defines.push_back("VSG_TWOSIDED");
            }
            else if (material->Get(AI_MATKEY_TWOSIDED, isTwoSided) == AI_SUCCESS && isTwoSided)
                defines.push_back("VSG_TWOSIDED");

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
                // TODO phong shader doesn't present have a specular texture maps
                convertedMaterial.assignTexture("specularMap", samplerImage.data, samplerImage.sampler);

                if (specularResult != AI_SUCCESS)
                    mat.specular.set(1.0f, 1.0f, 1.0f, 1.0f);
            }

            convertedMaterial.assignUniform("material", vsg::PhongMaterialValue::create(mat));
        }

        if (sharedObjects)
        {
            sharedObjects->share(convertedMaterial.descriptors);
        }

        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(convertedMaterial.descriptorBindings);
        convertedMaterial.descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, convertedMaterial.descriptors);
        if (sharedObjects)
        {
            sharedObjects->share(convertedMaterial.descriptorSet);
        }

        std::cout<<"   descriptors.size() = "<<convertedMaterial.descriptors.size()<<std::endl;
        for(auto& descriptor : convertedMaterial.descriptors) std::cout<<"     "<<descriptor<<std::endl;

        std::cout<<"   defines.size() = "<<defines.size()<<std::endl;
        for(auto& define : defines) std::cout<<"     "<<define<<std::endl;
    }

    vsg::ref_ptr<vsg::Data> createIndices(const aiMesh* mesh, unsigned int numIndicesPerFace, uint32_t numIndidices)
    {
        if (mesh->mNumVertices > 16384)
        {
            auto indices = vsg::uintArray::create(numIndidices);
            auto itr = indices->begin();
            for (unsigned int j = 0; j < mesh->mNumFaces; ++j)
            {
                const auto& face = mesh->mFaces[j];
                if (face.mNumIndices == numIndicesPerFace)
                {
                    for(unsigned int i=0; i<numIndicesPerFace; ++i) (*itr++) = static_cast<uint32_t>(face.mIndices[i]);
                }
            }
            return indices;
        }
        else
        {
            auto indices = vsg::ushortArray::create(numIndidices);
            auto itr = indices->begin();
            for (unsigned int j = 0; j < mesh->mNumFaces; ++j)
            {
                const auto& face = mesh->mFaces[j];
                if (face.mNumIndices == numIndicesPerFace)
                {
                    for(unsigned int i=0; i<numIndicesPerFace; ++i) (*itr++) = static_cast<uint16_t>(face.mIndices[i]);
                }
            }
            return indices;
        }
    }

    void convert(const aiMesh* mesh, vsg::ref_ptr<vsg::Node>& node)
    {
        std::cout<<"process(mesh = "<<mesh<<") mesh->mMaterialIndex = "<<mesh->mMaterialIndex<<std::endl;
        if (convertedMaterials.size() <= mesh->mMaterialIndex)
        {
            std::cout<<"Warning:  mesh"<<mesh<<") mesh->mMaterialIndex = "<<mesh->mMaterialIndex<<" exceedes available meterails.size()= "<<convertedMaterials.size()<<std::endl;
            return;
        }

        if (mesh->mNumVertices == 0 || mesh->mVertices == nullptr)
        {
            std::cout<<"Warning:  mesh"<<mesh<<") no verticex data, mesh->mNumVertices = "<<mesh->mNumVertices<<" mesh->mVertices = "<<mesh->mVertices<<std::endl;
            return;
        }

        if (mesh->mNumFaces == 0 || mesh->mFaces == nullptr)
        {
            std::cout<<"Warning:  mesh"<<mesh<<") no mesh data, mesh->mNumFaces = "<<mesh->mNumFaces<<std::endl;
            return;
        }

        auto& material = *convertedMaterials[mesh->mMaterialIndex];
        auto config = vsg::GraphicsPipelineConfig::create(material.shaderSet);
        /*auto& defines =*/ config->shaderHints->defines = material.defines;

        std::cout<<"    numVertices = "<<mesh->mNumVertices<<std::endl;
        std::cout<<"    mesh->mNormals = "<<mesh->mNormals<<std::endl;
        std::cout<<"    mesh->mColors[0] = " <<mesh->mColors[0]<<std::endl;
        std::cout<<"    mesh->mTextureCoords[0] = "<<mesh->mTextureCoords[0]<<std::endl;

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
            for(unsigned int i=0; i<mesh->mNumVertices; ++i)
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


        // count the number of indices of each type
        uint32_t numTriangleIndices = 0;
        uint32_t numLineIndices = 0;
        uint32_t numPointIndices = 0;
        for (unsigned int j = 0; j < mesh->mNumFaces; ++j)
        {
            const auto& face = mesh->mFaces[j];
            if (face.mNumIndices == 3) numTriangleIndices += 3;
            else if (face.mNumIndices == 2) numLineIndices += 2;
            else if (face.mNumIndices == 1) numPointIndices += 1;
            else
            {
                std::cout<<"Warning: unsupported number of indices on face "<<face.mNumIndices<<std::endl;
            }
        }

        if (numTriangleIndices > 0)
        {
            //config->inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            //config->inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            config->inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            auto indices = createIndices(mesh, 3, numTriangleIndices);

            auto vid = vsg::VertexIndexDraw::create();
            vid->assignArrays(vertexArrays);
            vid->assignIndices(indices);
            vid->indexCount = indices->valueCount();
            vid->instanceCount = 1;

            if (material.blending)
            {
                config->colorBlendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{
                    {true, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_SUBTRACT, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT}};

                if (sharedObjects) sharedObjects->share(config->colorBlendState);
            }

            // pass DescriptorSetLaout to config
            if (material.descriptorSet)
            {
                config->descriptorSetLayout = material.descriptorSet->setLayout;
                config->descriptorBindings = material.descriptorBindings;
            }

            if (sharedObjects) sharedObjects->share(config, [](auto gpc) { gpc->init(); });
            else config->init();

            if (sharedObjects) sharedObjects->share(config->bindGraphicsPipeline);


            // create StateGroup as the root of the scene/command graph to hold the GraphicsProgram, and binding of Descriptors to decorate the whole graph
            auto stateGroup = vsg::StateGroup::create();
            stateGroup->add(config->bindGraphicsPipeline);

            if (material.descriptorSet)
            {
                auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, config->layout, 0, material.descriptorSet);
                if (sharedObjects) sharedObjects->share(bindDescriptorSet);

                stateGroup->add(bindDescriptorSet);
            }

            // auto bindViewDescriptorSets = BindViewDescriptorSets::create(VK_PIPELINE_BIND_POINT_GRAPHICS, cofig.layout, 1);
            // stateGroup->add(bindViewDescriptorSets);

            stateGroup->addChild(vid);

            if (material.blending)
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


    }

    vsg::ref_ptr<vsg::Node> visit(const aiScene* in_scene, vsg::ref_ptr<const vsg::Options> in_options, const vsg::Path& ext)
    {
        scene = in_scene;
        options = in_options;

        if (options) sharedObjects = options->sharedObjects;
        if (!sharedObjects) sharedObjects = vsg::SharedObjects::create();

        cameraMap = processCameras(scene);
        lightMap = processLights(scene);

        // convet the materials
        convertedMaterials.resize(scene->mNumMaterials);
        for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
        {
            convertedMaterials[i] = vsg::DescriptorConfig::create();
            convert(scene->mMaterials[i], *convertedMaterials[i]);
        }

        // convet the meshses
        convertedMeshes.resize(scene->mNumMeshes);
        for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
        {
            convert(scene->mMeshes[i], convertedMeshes[i]);
        }


        auto vsg_scene = visit(scene->mRootNode, 0);
        if (!vsg_scene) return {};

        if (sharedObjects) sharedObjects->report(std::cout);

        if (auto transform = processCoordinateFrame(scene, options, ext))
        {
            transform->addChild(vsg_scene);

            // TODO check if subgraph requires culling
            // transform->subgraphRequiresLocalFrustum = false;

            return transform;
        }
        else
        {
            return vsg_scene;
        }
    }

    vsg::ref_ptr<vsg::Node> visit(const aiNode* node, int depth)
    {
        indent(std::cout, depth)<<"visit(node = "<<node<<") node->mNumMeshes = "<<node->mNumMeshes<<std::endl;

        vsg::Group::Children children;

        std::string name = node->mName.C_Str();

        // assign any lights
        if (auto camera_itr = cameraMap.find(name); camera_itr != cameraMap.end())
        {
            children.push_back(camera_itr->second);
        }

        // assign any lights
        if (auto light_itr = lightMap.find(name); light_itr != lightMap.end())
        {
            children.push_back(light_itr->second);
        }

        // visit the meshes
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
            if (auto child = visit(node->mChildren[i], depth+1))
            {
                children.push_back(child);
            }
        }

        if (children.empty()) return {};

        if (node->mTransformation.IsIdentity())
        {
            if (children.size()==1) return children[0];

            auto group = vsg::Group::create();
            group->children = children;

            return group;
        }
        else
        {
            aiMatrix4x4 m = node->mTransformation;
            m.Transpose();

            auto transform = vsg::MatrixTransform::create(vsg::dmat4(vsg::mat4((float*)&m)));
            transform->children = children;

            // TODO check if subgraph requires culling
            //transform->subgraphRequiresLocalFrustum = false;

            return transform;
        }
    }
};

vsg::ref_ptr<vsg::Object> assimp::Implementation::processScene(const aiScene* scene, vsg::ref_ptr<const vsg::Options> options, const vsg::Path& ext) const
{
    if (!vsg::value<bool>(false, assimp::original_converter, options))
    {
        SceneConverter converter;
        return converter.visit(scene, options, ext);
    }

    std::cout<<"assimp::Implementation::processScene_Old(..)"<<std::endl;

    bool useVertexIndexDraw = true;

    // Process materials
    //auto pipelineLayout = _defaultPipeline->layout;
    auto stateSets = processMaterials(scene, options);
    auto cameraMap = processCameras(scene);
    auto lightMap = processLights(scene);

    auto scenegraph = vsg::StateGroup::create();
    scenegraph->add(vsg::BindGraphicsPipeline::create(_defaultPipeline));
    scenegraph->add(_defaultState);

    std::stack<std::pair<aiNode*, vsg::ref_ptr<vsg::Group>>> nodes;
    nodes.push({scene->mRootNode, scenegraph});

    while (!nodes.empty())
    {
        auto [node, parent] = nodes.top();

        if (node)
        {
            aiMatrix4x4 m = node->mTransformation;
            m.Transpose();

            auto xform = vsg::MatrixTransform::create();
            xform->matrix = vsg::mat4((float*)&m);
            parent->addChild(xform);

            std::string name = node->mName.C_Str();
            if (auto camera_itr = cameraMap.find(name); camera_itr != cameraMap.end())
            {
                xform->addChild(camera_itr->second);
            }

            if (auto light_itr = lightMap.find(name); light_itr != lightMap.end())
            {
                xform->addChild(light_itr->second);
            }

            for (unsigned int i = 0; i < node->mNumMeshes; ++i)
            {
                auto mesh = scene->mMeshes[node->mMeshes[i]];
                auto vertices = vsg::vec3Array::create(mesh->mNumVertices);
                auto normals = vsg::vec3Array::create(mesh->mNumVertices);
                auto texcoords = vsg::vec2Array::create(mesh->mNumVertices);
                auto colors = vsg::vec4Array::create(1);
                colors->set(0, vsg::vec4(1.0, 1.0, 1.0, 1.0));

                std::vector<unsigned int> indices;

                for (unsigned int j = 0; j < mesh->mNumVertices; ++j)
                {
                    vertices->at(j) = vsg::vec3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);

                    if (mesh->mNormals)
                    {
                        normals->at(j) = vsg::vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z);
                    }
                    else
                    {
                        normals->at(j) = vsg::vec3(0, 0, 0);
                    }

                    if (mesh->mTextureCoords[0])
                    {
                        texcoords->at(j) = vsg::vec2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y);
                    }
                    else
                    {
                        texcoords->at(j) = vsg::vec2(0, 0);
                    }
                }

                for (unsigned int j = 0; j < mesh->mNumFaces; ++j)
                {
                    const auto& face = mesh->mFaces[j];

                    // A face can contain points, lines and triangles, having 1, 2 & 3 indicies respectively
                    // We need to query the number of indicies and build the appropriate primitives in VSG
                    // TODO: Add point and line primitives. At present we can only deal with triangles, so ignore others.
                    if(face.mNumIndices != 3) continue;

                    for (unsigned int k = 0; k < face.mNumIndices; ++k)
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

                if (useVertexIndexDraw)
                {
                    auto vid = vsg::VertexIndexDraw::create();
                    vid->assignArrays(vsg::DataList{vertices, normals, texcoords, colors});
                    vid->assignIndices(vsg_indices);
                    vid->indexCount = indices.size();
                    vid->instanceCount = 1;
                    stategroup->addChild(vid);
                }
                else
                {
                    stategroup->addChild(vsg::BindVertexBuffers::create(0, vsg::DataList{vertices, normals, texcoords, colors}));
                    stategroup->addChild(vsg::BindIndexBuffer::create(vsg_indices));
                    stategroup->addChild(vsg::DrawIndexed::create(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0));
                }

                //qCDebug(lc) << "Using material:" << scene->mMaterials[mesh->mMaterialIndex]->GetName().C_Str();
                bool isDepthSorted = false;
                if (mesh->mMaterialIndex < stateSets.size())
                {
                    auto state = stateSets[mesh->mMaterialIndex];

                    stategroup->add(state.first);
                    stategroup->add(state.second);

                    BlendDetector blend;
                    state.first->accept(blend);
                    if (blend.result)
                    {
                        vsg::ComputeBounds computeBounds;
                        stategroup->accept(computeBounds);
                        vsg::dvec3 center = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
                        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.5;

                        auto depthSorted = vsg::DepthSorted::create();
                        depthSorted->binNumber = 10;
                        depthSorted->bound.set(center[0], center[1], center[2], radius);
                        depthSorted->child = stategroup;
                        xform->addChild(depthSorted);
                        isDepthSorted = true;
                    }
                }
                if (!isDepthSorted)
                {
                    xform->addChild(stategroup);
                }
            }

            nodes.pop();

            for (unsigned int i = 0; i < node->mNumChildren; ++i)
                nodes.push({node->mChildren[i], xform});
        }
        else
        {
            nodes.pop();
        }
    }

    if (auto transform = processCoordinateFrame(scene, options, ext))
    {
        transform->addChild(scenegraph);
        return transform;
    }
    else
    {
        return scenegraph;
    }
}

assimp::Implementation::BindState assimp::Implementation::processMaterials(const aiScene* scene, vsg::ref_ptr<const vsg::Options> options) const
{
    BindState bindDescriptorSets;
    bindDescriptorSets.reserve(scene->mNumMaterials);

    for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
    {
        const auto material = scene->mMaterials[i];

        auto shaderHints = vsg::ShaderCompileSettings::create();
        auto& defines = shaderHints->defines;

        vsg::PbrMaterial pbr;
        bool hasPbrSpecularGlossiness = material->Get(AI_MATKEY_COLOR_SPECULAR, pbr.specularFactor);

        bool alphaBlend = hasAlphaBlend(material);
        if (alphaBlend)
            pbr.alphaMask = 0.0f;

        if (material->Get(AI_MATKEY_BASE_COLOR, pbr.baseColorFactor) == AI_SUCCESS || hasPbrSpecularGlossiness)
        {
            // PBR path

            if (hasPbrSpecularGlossiness)
            {
                defines.push_back("VSG_WORKFLOW_SPECGLOSS");
                material->Get(AI_MATKEY_COLOR_DIFFUSE, pbr.diffuseFactor);

                if (material->Get(AI_MATKEY_GLOSSINESS_FACTOR, pbr.specularFactor.a) != AI_SUCCESS)
                {
                    if (float shininess; material->Get(AI_MATKEY_SHININESS, shininess))
                        pbr.specularFactor.a = shininess / 1000;
                }
            }
            else
            {
                material->Get(AI_MATKEY_METALLIC_FACTOR, pbr.metallicFactor);
                material->Get(AI_MATKEY_ROUGHNESS_FACTOR, pbr.roughnessFactor);
            }

            material->Get(AI_MATKEY_COLOR_EMISSIVE, pbr.emissiveFactor);
            material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, pbr.alphaMaskCutoff);

            bool isTwoSided = vsg::value<bool>(false, assimp::two_sided, options) || (material->Get(AI_MATKEY_TWOSIDED, isTwoSided) == AI_SUCCESS);
            if (isTwoSided) defines.push_back("VSG_TWOSIDED");

            vsg::DescriptorSetLayoutBindings descriptorBindings{
                {10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
            vsg::Descriptors descList;

            auto buffer = vsg::DescriptorBuffer::create(vsg::PbrMaterialValue::create(pbr), 10);
            descList.push_back(buffer);

            SamplerData samplerImage;
            if (samplerImage = getTexture(scene, options, *material, aiTextureType_DIFFUSE, defines); samplerImage.data.valid())
            {
                auto diffuseTexture = vsg::DescriptorImage::create(samplerImage.sampler, samplerImage.data, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(diffuseTexture);
                descriptorBindings.push_back({0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
            }

            if (samplerImage = getTexture(scene, options, *material, aiTextureType_EMISSIVE, defines); samplerImage.data.valid())
            {
                auto emissiveTexture = vsg::DescriptorImage::create(samplerImage.sampler, samplerImage.data, 4, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(emissiveTexture);
                descriptorBindings.push_back({4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
            }

            if (samplerImage = getTexture(scene, options, *material, aiTextureType_LIGHTMAP, defines); samplerImage.data.valid())
            {
                auto aoTexture = vsg::DescriptorImage::create(samplerImage.sampler, samplerImage.data, 3, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(aoTexture);
                descriptorBindings.push_back({3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
            }

            if (samplerImage = getTexture(scene, options, *material, aiTextureType_NORMALS, defines); samplerImage.data.valid())
            {
                auto normalTexture = vsg::DescriptorImage::create(samplerImage.sampler, samplerImage.data, 2, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(normalTexture);
                descriptorBindings.push_back({2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
            }

            if (samplerImage = getTexture(scene, options, *material, aiTextureType_UNKNOWN, defines); samplerImage.data.valid())
            {
                auto mrTexture = vsg::DescriptorImage::create(samplerImage.sampler, samplerImage.data, 1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(mrTexture);
                descriptorBindings.push_back({1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
            }

            if (samplerImage = getTexture(scene, options, *material, aiTextureType_SPECULAR, defines); samplerImage.data.valid())
            {
                auto texture = vsg::DescriptorImage::create(samplerImage.sampler, samplerImage.data, 5, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(texture);
                descriptorBindings.push_back({5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
            }

            auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
            auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, descList);

            auto vertexShader = assimp_vert();
            auto fragmentShader = assimp_pbr_frag();
            vertexShader->module->hints = shaderHints;
            fragmentShader->module->hints = shaderHints;

            auto pipeline = createPipeline(vertexShader, fragmentShader, descriptorSetLayout, isTwoSided, alphaBlend);
            auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, 0, descriptorSet);

            bindDescriptorSets.push_back({vsg::BindGraphicsPipeline::create(pipeline), bindDescriptorSet});
        }
        else
        {
            // Phong shading
            vsg::PhongMaterial mat;

            if (alphaBlend)
                mat.alphaMask = 0.0f;
            material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, mat.alphaMaskCutoff);
            material->Get(AI_MATKEY_COLOR_AMBIENT, mat.ambient);
            const auto diffuseResult = material->Get(AI_MATKEY_COLOR_DIFFUSE, mat.diffuse);
            const auto emissiveResult = material->Get(AI_MATKEY_COLOR_EMISSIVE, mat.emissive);
            const auto specularResult = material->Get(AI_MATKEY_COLOR_SPECULAR, mat.specular);

            aiShadingMode shadingModel = aiShadingMode_Phong;
            material->Get(AI_MATKEY_SHADING_MODEL, shadingModel);

            bool isTwoSided{false};
            bool optionFlag{false};
            if(options->getValue(assimp::two_sided, optionFlag) && optionFlag) 
            {
                isTwoSided = true;
                defines.push_back("VSG_TWOSIDED");
            }
            else if (material->Get(AI_MATKEY_TWOSIDED, isTwoSided) == AI_SUCCESS && isTwoSided)
                defines.push_back("VSG_TWOSIDED");

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

            vsg::DescriptorSetLayoutBindings descriptorBindings{
                {10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
            vsg::Descriptors descList;

            SamplerData samplerImage;
            if (samplerImage = getTexture(scene, options, *material, aiTextureType_DIFFUSE, defines); samplerImage.data.valid())
            {
                auto diffuseTexture = vsg::DescriptorImage::create(samplerImage.sampler, samplerImage.data, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(diffuseTexture);
                descriptorBindings.push_back({0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});

                if (diffuseResult != AI_SUCCESS)
                    mat.diffuse.set(1.0f, 1.0f, 1.0f, 1.0f);
            }

            if (samplerImage = getTexture(scene, options, *material, aiTextureType_EMISSIVE, defines); samplerImage.data.valid())
            {
                auto emissiveTexture = vsg::DescriptorImage::create(samplerImage.sampler, samplerImage.data, 4, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(emissiveTexture);
                descriptorBindings.push_back({4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});

                if (emissiveResult != AI_SUCCESS)
                    mat.emissive.set(1.0f, 1.0f, 1.0f, 1.0f);
            }

            if (samplerImage = getTexture(scene, options, *material, aiTextureType_LIGHTMAP, defines); samplerImage.data.valid())
            {
                auto aoTexture = vsg::DescriptorImage::create(samplerImage.sampler, samplerImage.data, 3, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(aoTexture);
                descriptorBindings.push_back({3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
            }
            else if (samplerImage = getTexture(scene, options, *material, aiTextureType_AMBIENT, defines); samplerImage.data.valid())
            {
                auto texture = vsg::DescriptorImage::create(samplerImage.sampler, samplerImage.data, 3, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(texture);
                descriptorBindings.push_back({3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
            }

            if (samplerImage = getTexture(scene, options, *material, aiTextureType_NORMALS, defines); samplerImage.data.valid())
            {
                auto normalTexture = vsg::DescriptorImage::create(samplerImage.sampler, samplerImage.data, 2, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(normalTexture);
                descriptorBindings.push_back({2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
            }

            if (samplerImage = getTexture(scene, options, *material, aiTextureType_SPECULAR, defines); samplerImage.data.valid())
            {
                auto texture = vsg::DescriptorImage::create(samplerImage.sampler, samplerImage.data, 5, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                descList.push_back(texture);
                descriptorBindings.push_back({5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});

                if (specularResult != AI_SUCCESS)
                    mat.specular.set(1.0f, 1.0f, 1.0f, 1.0f);
            }

            auto buffer = vsg::DescriptorBuffer::create(vsg::PhongMaterialValue::create(mat), 10);
            descList.push_back(buffer);

            auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

            auto vertexShader = assimp_vert();
            auto fragmentShader = assimp_pbr_frag();
            vertexShader->module->hints = shaderHints;
            fragmentShader->module->hints = shaderHints;

            auto pipeline = createPipeline(vertexShader, fragmentShader, descriptorSetLayout, isTwoSided, alphaBlend);

            auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, descList);
            auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, 0, descriptorSet);

            bindDescriptorSets.push_back({vsg::BindGraphicsPipeline::create(pipeline), bindDescriptorSet});
        }
    }

    return bindDescriptorSets;
}

vsg::ref_ptr<vsg::Object> assimp::Implementation::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    std::cout<<"\n--------------------------------------------\nassimp::Implementation::read("<<filename<<")"<<std::endl;

    Assimp::Importer importer;

    if (const auto ext = vsg::lowerCaseFileExtension(filename); importer.IsExtensionSupported(ext))
    {
        vsg::Path filenameToUse = vsg::findFile(filename, options);
        if (filenameToUse.empty()) return {};

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

        if (auto scene = importer.ReadFile(filenameToUse, flags); scene)
        {
            auto opt = vsg::Options::create(*options);
            opt->paths.insert(opt->paths.begin(), vsg::filePath(filenameToUse));

            return processScene(scene, opt, ext);
        }
        else
        {
            std::cerr << "Failed to load file: " << filename << std::endl
                      << importer.GetErrorString() << std::endl;
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
    if (!options) return {};

    Assimp::Importer importer;
    if (importer.IsExtensionSupported(options->extensionHint))
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
            return processScene(scene, options, options->extensionHint);
        }
        else
        {
            std::cerr << "Failed to load file from stream: " << importer.GetErrorString() << std::endl;
        }
    }

    return {};
}

vsg::ref_ptr<vsg::Object> assimp::Implementation::read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!options) return {};

    Assimp::Importer importer;
    if (importer.IsExtensionSupported(options->extensionHint))
    {
        if (auto scene = importer.ReadFileFromMemory(ptr, size, _importFlags); scene)
        {
            return processScene(scene, options, options->extensionHint);
        }
        else
        {
            std::cerr << "Failed to load file from memory: " << importer.GetErrorString() << std::endl;
        }
    }
    return {};
}
