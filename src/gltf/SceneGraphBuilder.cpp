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

#include <vsg/animation/AnimationGroup.h>
#include <vsg/animation/Joint.h>
#include <vsg/animation/JointSampler.h>
#include <vsg/animation/TransformSampler.h>
#include <vsg/lighting/DirectionalLight.h>
#include <vsg/lighting/PointLight.h>
#include <vsg/lighting/SpotLight.h>
#include <vsg/maths/transform.h>
#include <vsg/nodes/CullGroup.h>
#include <vsg/nodes/CullNode.h>
#include <vsg/nodes/DepthSorted.h>
#include <vsg/nodes/Group.h>
#include <vsg/nodes/InstanceDraw.h>
#include <vsg/nodes/InstanceDrawIndexed.h>
#include <vsg/nodes/Layer.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/Switch.h>
#include <vsg/nodes/VertexDraw.h>
#include <vsg/nodes/VertexIndexDraw.h>
#include <vsg/state/material.h>
#include <vsg/utils/ComputeBounds.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>

#include <vsg/io/write.h>

#ifdef vsgXchange_draco
#    include "draco/compression/decode.h"
#    include "draco/core/decoder_buffer.h"
#endif

using namespace vsgXchange;

gltf::SceneGraphBuilder::SceneGraphBuilder()
{
    attributeLookup = {
        {"POSITION", "vsg_Vertex"},
        {"NORMAL", "vsg_Normal"},
        {"TEXCOORD_0", "vsg_TexCoord0"},
        {"TEXCOORD_1", "vsg_TexCoord1"},
        {"TEXCOORD_2", "vsg_TexCoord2"},
        {"TEXCOORD_3", "vsg_TexCoord3"},
        {"COLOR", "vsg_Color"},
        {"COLOR_0", "vsg_Color"},
        {"JOINTS_0", "vsg_JointIndices"},
        {"WEIGHTS_0", "vsg_JointWeights"},
        {"TRANSLATION", "vsg_Translation"},
        {"ROTATION", "vsg_Rotation"},
        {"SCALE", "vsg_Scale"}};
}

void gltf::SceneGraphBuilder::assign_extras(ExtensionsExtras& src, vsg::Object& dest)
{
    if (src.extras)
    {
        if (src.extras->object)
        {
            dest.setObject("extras", src.extras->object);
        }
        else if (src.extras->objects)
        {
            dest.setObject("extras", src.extras->objects);
        }
    }
};

void gltf::SceneGraphBuilder::assign_name_extras(NameExtensionsExtras& src, vsg::Object& dest)
{
    if (!src.name.empty())
    {
        if (auto joint = dest.cast<vsg::Joint>())
            joint->name = src.name;
        else if (auto animation = dest.cast<vsg::Animation>())
            animation->name = src.name;
        else if (auto animationSampler = dest.cast<vsg::AnimationSampler>())
            animationSampler->name = src.name;
        else
            dest.setValue("name", src.name);
    }

    if (src.extras)
    {
        if (src.extras->object)
        {
            vsg::info("Assignig extras object ", src.extras->object);
            dest.setObject("extras", src.extras->object);
        }
        else if (src.extras->objects)
        {
            vsg::info("Assignig extras objects ", src.extras->object);
            dest.setObject("extras", src.extras->objects);
        }
    }
};

vsg::ref_ptr<vsg::Data> gltf::SceneGraphBuilder::createBuffer(vsg::ref_ptr<gltf::Buffer> gltf_buffer)
{
    return gltf_buffer->data;
}

vsg::ref_ptr<vsg::Data> gltf::SceneGraphBuilder::createBufferView(vsg::ref_ptr<gltf::BufferView> gltf_bufferView)
{
    if (!gltf_bufferView->buffer)
    {
        vsg::info("Warning: no buffer available to create BufferView.");
        return {};
    }

    if (!vsg_buffers[gltf_bufferView->buffer.value])
    {
        vsg::info("Warning: no vsg::Data available to create BufferView.");
        return {};
    }

    // TODO: decide whether we need to do anything with the BufferView.target
    auto vsg_buffer = vsg::ubyteArray::create(vsg_buffers[gltf_bufferView->buffer.value],
                                              gltf_bufferView->byteOffset,
                                              gltf_bufferView->byteStride,
                                              gltf_bufferView->byteLength / gltf_bufferView->byteStride);
    return vsg_buffer;
}

vsg::ref_ptr<vsg::Data> gltf::SceneGraphBuilder::createArray(const std::string& type, uint32_t componentType, glTFid bufferView, uint32_t offset, uint32_t count)
{
    vsg::ref_ptr<vsg::Data> vsg_data;

    auto vsg_bufferView = vsg_bufferViews[bufferView.value];

    auto stride = [&](uint32_t s) -> uint32_t {
        return std::max(vsg_bufferView->properties.stride, s);
    };

    switch (componentType)
    {
    case (COMPONENT_TYPE_BYTE):
        if (type == "SCALAR")
            vsg_data = vsg::byteArray::create(vsg_bufferView, offset, stride(1), count);
        else if (type == "VEC2")
            vsg_data = vsg::bvec2Array::create(vsg_bufferView, offset, stride(2), count);
        else if (type == "VEC3")
            vsg_data = vsg::bvec3Array::create(vsg_bufferView, offset, stride(3), count);
        else if (type == "VEC4")
            vsg_data = vsg::bvec4Array::create(vsg_bufferView, offset, stride(4), count);
        else
            vsg::warn("Unsupported componentType = ", componentType);
        break;
    case (COMPONENT_TYPE_UNSIGNED_BYTE):
        if (type == "SCALAR")
            vsg_data = vsg::ubyteArray::create(vsg_bufferView, offset, stride(1), count);
        else if (type == "VEC2")
            vsg_data = vsg::ubvec2Array::create(vsg_bufferView, offset, stride(2), count);
        else if (type == "VEC3")
            vsg_data = vsg::ubvec3Array::create(vsg_bufferView, offset, stride(3), count);
        else if (type == "VEC4")
            vsg_data = vsg::ubvec4Array::create(vsg_bufferView, offset, stride(4), count);
        else
            vsg::warn("Unsupported componentType = ", componentType);
        break;
    case (COMPONENT_TYPE_SHORT):
        if (type == "SCALAR")
            vsg_data = vsg::shortArray::create(vsg_bufferView, offset, stride(2), count);
        else if (type == "VEC2")
            vsg_data = vsg::svec2Array::create(vsg_bufferView, offset, stride(4), count);
        else if (type == "VEC3")
            vsg_data = vsg::svec3Array::create(vsg_bufferView, offset, stride(6), count);
        else if (type == "VEC4")
            vsg_data = vsg::svec4Array::create(vsg_bufferView, offset, stride(8), count);
        else
            vsg::warn("Unsupported componentType = ", componentType);
        break;
    case (COMPONENT_TYPE_UNSIGNED_SHORT):
        if (type == "SCALAR")
            vsg_data = vsg::ushortArray::create(vsg_bufferView, offset, stride(2), count);
        else if (type == "VEC2")
            vsg_data = vsg::usvec2Array::create(vsg_bufferView, offset, stride(4), count);
        else if (type == "VEC3")
            vsg_data = vsg::usvec3Array::create(vsg_bufferView, offset, stride(6), count);
        else if (type == "VEC4")
            vsg_data = vsg::usvec4Array::create(vsg_bufferView, offset, stride(8), count);
        else
            vsg::warn("Unsupported componentType = ", componentType);
        break;
    case (COMPONENT_TYPE_INT):
        if (type == "SCALAR")
            vsg_data = vsg::intArray::create(vsg_bufferView, offset, stride(4), count);
        else if (type == "VEC2")
            vsg_data = vsg::ivec2Array::create(vsg_bufferView, offset, stride(8), count);
        else if (type == "VEC3")
            vsg_data = vsg::ivec3Array::create(vsg_bufferView, offset, stride(12), count);
        else if (type == "VEC4")
            vsg_data = vsg::ivec4Array::create(vsg_bufferView, offset, stride(16), count);
        else
            vsg::warn("Unsupported componentType = ", componentType);
        break;
    case (COMPONENT_TYPE_UNSIGNED_INT):
        if (type == "SCALAR")
            vsg_data = vsg::uintArray::create(vsg_bufferView, offset, stride(4), count);
        else if (type == "VEC2")
            vsg_data = vsg::uivec2Array::create(vsg_bufferView, offset, stride(8), count);
        else if (type == "VEC3")
            vsg_data = vsg::uivec3Array::create(vsg_bufferView, offset, stride(12), count);
        else if (type == "VEC4")
            vsg_data = vsg::uivec4Array::create(vsg_bufferView, offset, stride(16), count);
        else
            vsg::warn("Unsupported componentType = ", componentType);
        break;
    case (COMPONENT_TYPE_FLOAT):
        if (type == "SCALAR")
            vsg_data = vsg::floatArray::create(vsg_bufferView, offset, stride(4), count);
        else if (type == "VEC2")
            vsg_data = vsg::vec2Array::create(vsg_bufferView, offset, stride(8), count);
        else if (type == "VEC3")
            vsg_data = vsg::vec3Array::create(vsg_bufferView, offset, stride(12), count);
        else if (type == "VEC4")
            vsg_data = vsg::vec4Array::create(vsg_bufferView, offset, stride(16), count);
        //else if (type=="MAT2")   vsg_data = vsg::mat2Array::create(vsg_bufferView, offset, stride(16), count);
        //else if (type=="MAT3")   vsg_data = vsg::mat3Array::create(vsg_bufferView, offset, stride(36), count);
        else if (type == "MAT4")
            vsg_data = vsg::mat4Array::create(vsg_bufferView, offset, stride(64), count);
        else
            vsg::warn("Unsupported componentType = ", componentType);
        break;
    case (COMPONENT_TYPE_DOUBLE):
        if (type == "SCALAR")
            vsg_data = vsg::doubleArray::create(vsg_bufferView, offset, stride(8), count);
        else if (type == "VEC2")
            vsg_data = vsg::dvec2Array::create(vsg_bufferView, offset, stride(16), count);
        else if (type == "VEC3")
            vsg_data = vsg::dvec3Array::create(vsg_bufferView, offset, stride(24), count);
        else if (type == "VEC4")
            vsg_data = vsg::dvec4Array::create(vsg_bufferView, offset, stride(32), count);
        //else if (type=="MAT2")   vsg_data = vsg::dmat2Array::create(vsg_bufferView, offset, stride(32), count);
        //else if (type=="MAT3")   vsg_data = vsg::dmat3Array::create(vsg_bufferView, offset, stride(72), count);
        else if (type == "MAT4")
            vsg_data = vsg::dmat4Array::create(vsg_bufferView, offset, stride(128), count);
        else
            vsg::warn("Unsupported componentType = ", componentType);
        break;
    }

    if (cloneAccessors)
    {
        vsg::info("clonning vsg_data ", vsg_data);
        vsg_data = vsg::clone(vsg_data);
        vsg::info("clonned vsg_data ", vsg_data);
    }

    return vsg_data;
}

vsg::ref_ptr<vsg::Data> gltf::SceneGraphBuilder::createAccessor(vsg::ref_ptr<gltf::Accessor> gltf_accessor)
{
    if (!gltf_accessor->bufferView)
    {
        vsg::info("Warning: no bufferView available to create Accessor.");
        return {};
    }

    if (!vsg_bufferViews[gltf_accessor->bufferView.value])
    {
        vsg::info("Warning: no vsg::Data available to create BufferView.");
        return {};
    }

    auto vsg_data = createArray(gltf_accessor->type, gltf_accessor->componentType, gltf_accessor->bufferView, gltf_accessor->byteOffset, gltf_accessor->count);

    if (gltf_accessor->sparse)
    {
        auto sparse = gltf_accessor->sparse;
        auto vsg_indices = createArray("SCALAR", sparse->indices->componentType, sparse->indices->bufferView, sparse->indices->byteOffset, sparse->count);
        auto vsg_values = createArray(gltf_accessor->type, gltf_accessor->componentType, sparse->values->bufferView, sparse->values->byteOffset, sparse->count);

        if (auto uint_indices = vsg_indices.cast<vsg::uintArray>())
        {
            for (size_t i = 0; i < uint_indices->size(); ++i)
            {
                std::memcpy(vsg_data->dataPointer(uint_indices->at(i)), vsg_values->dataPointer(i), vsg_data->valueSize());
            }
        }
        else if (auto ushort_indices = vsg_indices.cast<vsg::ushortArray>())
        {
            for (size_t i = 0; i < ushort_indices->size(); ++i)
            {
                std::memcpy(vsg_data->dataPointer(ushort_indices->at(i)), vsg_values->dataPointer(i), vsg_data->valueSize());
            }
        }
        else
        {
            vsg::warn("gltf::SceneGraphBuilder::createAccessor(...) sparse indices type (", sparse->indices->componentType, " not supported. ");
        }
    }

    return vsg_data;
}

vsg::ref_ptr<vsg::Camera> gltf::SceneGraphBuilder::createCamera(vsg::ref_ptr<gltf::Camera> gltf_camera)
{
    auto vsg_camera = vsg::Camera::create();

    if (gltf_camera->perspective)
    {
        auto perspective = gltf_camera->perspective;
        // note, vsg::Perspective implements GLU Perspective style settings so uses degress for fov, while glTF uses radians so need to convert to degrees
        vsg_camera->projectionMatrix = vsg::Perspective::create(vsg::degrees(perspective->yfov), perspective->aspectRatio, perspective->znear, perspective->zfar);
    }

    if (gltf_camera->orthographic)
    {
        auto orthographic = gltf_camera->orthographic;
        double halfWidth = orthographic->xmag;  // TODO: figure how to map to GLU/VSG style orthographic
        double halfHeight = orthographic->ymag; // TODO: figure how to mapto GLU/VSG style orthographic
        vsg_camera->projectionMatrix = vsg::Orthographic::create(-halfWidth, halfWidth, -halfHeight, halfHeight, orthographic->znear, orthographic->zfar);
    }

    vsg_camera->name = gltf_camera->name;
    assign_extras(*gltf_camera, *vsg_camera);

    return vsg_camera;
}

vsg::ref_ptr<vsg::Data> gltf::SceneGraphBuilder::createImage(vsg::ref_ptr<gltf::Image> gltf_image)
{
    if (gltf_image->data)
    {
        // vsg::info("createImage(", gltf_image, ") gltf_image->data = ", gltf_image->data);
        return gltf_image->data;
    }
    else if (gltf_image->bufferView)
    {
        auto data = vsg_bufferViews[gltf_image->bufferView.value];
        // vsg::info("createImage(", gltf_image, ") bufferView = ", gltf_image->bufferView, ", vsg_bufferView = ", data);
        return data;
    }
    else
    {
        vsg::info("createImage(", gltf_image, ") uri = ", gltf_image->uri, ", nothing to create vsg::Data image from.");
        return {};
    }
}

vsg::ref_ptr<vsg::Sampler> gltf::SceneGraphBuilder::createSampler(vsg::ref_ptr<gltf::Sampler> gltf_sampler)
{
    auto vsg_sampler = vsg::Sampler::create();

    vsg_sampler->maxAnisotropy = maxAnisotropy;
    vsg_sampler->anisotropyEnable = (maxAnisotropy > 0.0f) ? VK_TRUE : VK_FALSE;

    // assume mipmapping.
    vsg_sampler->minLod = 0.0f;
    vsg_sampler->maxLod = 16.0f;

    // See https://docs.vulkan.org/spec/latest/chapters/samplers.html for suggestions on mapping from OpenGL style to Vulkan
    switch (gltf_sampler->minFilter)
    {
    case (9728): // NEAREST
        vsg_sampler->minFilter = VK_FILTER_NEAREST;
        vsg_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        vsg_sampler->minLod = 0.0f;
        vsg_sampler->maxLod = 0.25f;
        break;
    case (9729): // LINEAR
        vsg_sampler->minFilter = VK_FILTER_LINEAR;
        vsg_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        vsg_sampler->minLod = 0.0f;
        vsg_sampler->maxLod = 0.25f;
        break;
    case (9984): // NEAREST_MIPMAP_NEAREST
        vsg_sampler->minFilter = VK_FILTER_NEAREST;
        vsg_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        break;
    case (9985): // LINEAR_MIPMAP_NEAREST
        vsg_sampler->minFilter = VK_FILTER_LINEAR;
        vsg_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        break;
    case (9986): // NEAREST_MIPMAP_LINEAR
        vsg_sampler->minFilter = VK_FILTER_NEAREST;
        vsg_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        break;
    case (9987): // LINEAR_MIPMAP_LINEAR
        vsg_sampler->minFilter = VK_FILTER_LINEAR;
        vsg_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        break;
    default:
        vsg::debug("gltf_sampler->minFilter value of ", gltf_sampler->minFilter, " not set, using linear mipmap linear.");
        vsg_sampler->minFilter = VK_FILTER_LINEAR;
        vsg_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        break;
    }

    switch (gltf_sampler->magFilter)
    {
    case (9728):
        vsg_sampler->magFilter = VK_FILTER_NEAREST;
        break;
    case (9729):
        vsg_sampler->magFilter = VK_FILTER_LINEAR;
        break;
    default:
        vsg::debug("gltf_sampler->magFilter value of ", gltf_sampler->magFilter, " not set, using default of linear.");
        vsg_sampler->magFilter = VK_FILTER_LINEAR;
        break;
    }

    auto addressMode = [](uint32_t wrap) -> VkSamplerAddressMode {
        switch (wrap)
        {
        case (33071): // CLAMP_TO_EDGE
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case (33648): // MIRRORED_REPEAT
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case (10497): // REPEAT
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        default:
            vsg::warn("gltf_sampler->wrap* value of ", wrap, " not supported.");
            break;
        }
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    };

    vsg_sampler->addressModeU = addressMode(gltf_sampler->wrapS);
    vsg_sampler->addressModeV = addressMode(gltf_sampler->wrapT);

    if (sharedObjects)
    {
        sharedObjects->share(vsg_sampler);
    }

    // vsg::info("created sampler { ", vsg_sampler->minFilter, ", ", vsg_sampler->magFilter, ", ", vsg_sampler->mipmapMode, ", ",  gltf_sampler->wrapS, ", ", gltf_sampler->wrapT, "}");

    return vsg_sampler;
}

gltf::SceneGraphBuilder::SamplerImage gltf::SceneGraphBuilder::createTexture(vsg::ref_ptr<gltf::Texture> gltf_texture)
{
    SamplerImage samplerImage;

    if (gltf_texture->sampler)
    {
        samplerImage.sampler = vsg_samplers[gltf_texture->sampler.value];
    }

    if (gltf_texture->source)
    {
        samplerImage.image = vsg_images[gltf_texture->source.value];
    }

    return samplerImage;
}

vsg::ref_ptr<vsg::DescriptorConfigurator> gltf::SceneGraphBuilder::createPbrMaterial(vsg::ref_ptr<gltf::Material> gltf_material)
{
    auto vsg_material = vsg::DescriptorConfigurator::create();

    vsg_material->shaderSet = getOrCreatePbrShaderSet();

    vsg_material->two_sided = gltf_material->doubleSided;
    if (vsg_material->two_sided) vsg_material->defines.insert("VSG_TWO_SIDED_LIGHTING");

    auto pbrMaterialValue = vsg::PbrMaterialValue::create();
    auto& pbrMaterial = pbrMaterialValue->value();

    auto texCoordIndicesValue = vsg::TexCoordIndicesValue::create();
    auto& texCoordIndices = texCoordIndicesValue->value();

    if (gltf_material->pbrMetallicRoughness.baseColorFactor.values.size() == 4)
    {
        auto& baseColorFactor = gltf_material->pbrMetallicRoughness.baseColorFactor.values;
        pbrMaterial.baseColorFactor.set(baseColorFactor[0], baseColorFactor[1], baseColorFactor[2], baseColorFactor[3]);
        // vsg::info("Assigned baseColorFacator ", pbrMaterial.baseColorFactor);
    }

    if (gltf_material->pbrMetallicRoughness.baseColorTexture.index)
    {
        auto& textureInfo = gltf_material->pbrMetallicRoughness.baseColorTexture;
        auto& texture = vsg_textures[textureInfo.index.value];
        if (texture.image)
        {
            // vsg::info("Assigned diffuseMap ", texture.image, ", ", texture.sampler);
            vsg_material->assignTexture("diffuseMap", texture.image, texture.sampler);
            texCoordIndices.diffuseMap = textureInfo.texCoord;

            if (auto texture_transform = textureInfo.extension<KHR_texture_transform>("KHR_texture_transform"))
            {
                vsg_material->setObject("KHR_texture_transform", texture_transform);
            }
        }
        else
        {
            vsg::warn("Could not assign diffuseMap ", textureInfo.index);
        }
    }

    pbrMaterial.metallicFactor = gltf_material->pbrMetallicRoughness.metallicFactor;
    pbrMaterial.roughnessFactor = gltf_material->pbrMetallicRoughness.roughnessFactor;

    if (gltf_material->pbrMetallicRoughness.metallicRoughnessTexture.index)
    {
        auto& textureInfo = gltf_material->pbrMetallicRoughness.metallicRoughnessTexture;
        auto& texture = vsg_textures[textureInfo.index.value];
        if (texture.image)
        {
            // vsg::info("Assigned metallicRoughnessTexture ", texture.image, ", ", texture.sampler);
            vsg_material->assignTexture("mrMap", texture.image, texture.sampler);
            texCoordIndices.mrMap = textureInfo.texCoord;
        }
        else
        {
            vsg::warn("Could not assign metallicRoughnessTexture ", textureInfo.index);
        }
    }

    // TODO : pbrMaterial.diffuseFactor? No glTF mapping?

    if (gltf_material->normalTexture.index)
    {
        // TODO: gltf_material->normalTexture.scale

        auto& textureInfo = gltf_material->normalTexture;
        auto& texture = vsg_textures[textureInfo.index.value];
        if (texture.image)
        {
            // vsg::info("Assigned normalTexture ", texture.image, ", ", texture.sampler, ", scale = ", gltf_material->normalTexture.scale);
            vsg_material->assignTexture("normalMap", texture.image, texture.sampler);
            texCoordIndices.normalMap = textureInfo.texCoord;
        }
        else
        {
            vsg::warn("Could not assign normalTexture ", textureInfo.index);
        }
    }

    if (gltf_material->occlusionTexture.index)
    {
        // TODO: gltf_material->occlusionTexture.strength

        auto& textureInfo = gltf_material->occlusionTexture;
        auto& texture = vsg_textures[textureInfo.index.value];
        if (texture.image)
        {
            // vsg::info("Assigned occlusionTexture ", texture.image, ", ", texture.sampler, ", strength = ", textureInfo.strength);
            vsg_material->assignTexture("aoMap", texture.image, texture.sampler);
            texCoordIndices.aoMap = textureInfo.texCoord;
        }
        else
        {
            vsg::warn("Could not assign occlusionTexture ", textureInfo.index);
        }
    }

    if (gltf_material->emissiveTexture.index)
    {
        auto& textureInfo = gltf_material->emissiveTexture;
        auto& texture = vsg_textures[textureInfo.index.value];
        if (texture.image)
        {
            // vsg::info("Assigned emissiveTexture ", texture.image, ", ", texture.sampler);
            vsg_material->assignTexture("emissiveMap", texture.image, texture.sampler);
            texCoordIndices.emissiveMap = textureInfo.texCoord;
        }
        else
        {
            vsg::warn("Could not assign emissiveTexture ", textureInfo.index);
        }
    }

    if (gltf_material->emissiveFactor.values.size() >= 3)
    {
        pbrMaterial.emissiveFactor.set(gltf_material->emissiveFactor.values[0], gltf_material->emissiveFactor.values[1], gltf_material->emissiveFactor.values[2], 1.0);
        // vsg::info("Set pbrMaterial.emissiveFactor = ", pbrMaterial.emissiveFactor);
    }

    if (gltf_material->alphaMode == "BLEND")
    {
        vsg_material->blending = true;
    }
    else if (gltf_material->alphaMode == "MASK")
    {
        // vsg_material->blending = true; // TODO, do we need to enable blending?
        vsg_material->defines.insert("VSG_ALPHA_TEST");
        pbrMaterial.alphaMaskCutoff = gltf_material->alphaCutoff;
    }

    if (auto materials_specular = gltf_material->extension<KHR_materials_specular>("KHR_materials_specular"))
    {
        float sf = materials_specular->specularFactor;
        pbrMaterial.specularFactor.set(sf, sf, sf, 1.0);

        if (materials_specular->specularTexture.index)
        {
            auto& texture = vsg_textures[materials_specular->specularTexture.index.value];
            if (texture.image)
            {
                // vsg::info("Assigned specularTexture ", texture.image, ", ", texture.sampler);
                vsg_material->assignTexture("specularMap", texture.image, texture.sampler);
            }
            else
            {
                vsg::warn("Could not assign specularTexture ", materials_specular->specularTexture.index);
            }
        }

        if (materials_specular->specularColorFactor.values.size() >= 3)
        {
            auto& specularColorFactor = materials_specular->specularColorFactor.values;
            pbrMaterial.specularFactor.set(specularColorFactor[0], specularColorFactor[1], specularColorFactor[2], 1.0); // TODO, alpha value? Shoult it be specularFactor?
            // vsg::info("Assigned specularColorFactor pbrMaterial.specularFactor ", pbrMaterial.specularFactor);
        }

        if (materials_specular->specularColorTexture.index)
        {
            vsg::info("Not assigned yet: specularColorTexture = ", materials_specular->specularColorTexture.index, ", ", materials_specular->specularColorTexture.texCoord);
        }
    }

    if (auto materials_pbrSpecularGlossiness = gltf_material->extension<KHR_materials_pbrSpecularGlossiness>("KHR_materials_pbrSpecularGlossiness"))
    {

        if (materials_pbrSpecularGlossiness->diffuseFactor.values.size() >= 3)
        {
            auto& diffuseFactor = materials_pbrSpecularGlossiness->diffuseFactor.values;
            pbrMaterial.diffuseFactor.set(diffuseFactor[0], diffuseFactor[1], diffuseFactor[2], 1.0);
        }

        if (materials_pbrSpecularGlossiness->specularFactor.values.size() >= 3)
        {
            auto& specularFactor = materials_pbrSpecularGlossiness->specularFactor.values;
            pbrMaterial.specularFactor.set(specularFactor[0], specularFactor[1], specularFactor[2], 1.0);
        }

        if (materials_pbrSpecularGlossiness->diffuseTexture.index)
        {
            auto& texture = vsg_textures[materials_pbrSpecularGlossiness->diffuseTexture.index.value];
            if (texture.image)
            {
                vsg_material->assignTexture("diffuseMap", texture.image, texture.sampler);
            }
            else
            {
                vsg::warn("Could not assign diffuseTexture ", materials_pbrSpecularGlossiness->diffuseTexture.index);
            }
        }

        if (materials_pbrSpecularGlossiness->specularGlossinessTexture.index)
        {
            auto& texture = vsg_textures[materials_pbrSpecularGlossiness->specularGlossinessTexture.index.value];
            if (texture.image)
            {
                vsg_material->assignTexture("specularMap", texture.image, texture.sampler);
            }
            else
            {
                vsg::warn("Could not assign specularTexture ", materials_pbrSpecularGlossiness->specularGlossinessTexture.index);
            }
        }

        pbrMaterial.specularFactor.a = materials_pbrSpecularGlossiness->glossinessFactor;

        vsg_material->defines.insert("VSG_WORKFLOW_SPECGLOSS");
    }

    if (auto materials_emissive_strength = gltf_material->extension<KHR_materials_emissive_strength>("KHR_materials_emissive_strength"))
    {
        pbrMaterial.emissiveFactor.a = materials_emissive_strength->emissiveStrength;
    }

#if 0
    if (auto materials_ior = gltf_material->extension<KHR_materials_ior>("KHR_materials_ior"))
    {
        vsg::info("Have Index Of Refraction: ", materials_ior);
    }

    if (gltf_material->extensions)
    {
        for(auto& [name, schema] : gltf_material->extensions->values)
        {
            vsg::info("extensions ", name, ", ", schema);
        }
    }
#endif

    vsg_material->assignDescriptor("material", pbrMaterialValue);
    vsg_material->assignDescriptor("texCoordIndices", texCoordIndicesValue);

    // TODO: vsg_material->defines.insert("VSG_WORKFLOW_SPECGLOSS");
    // TODO: VSG -> detailMap

    return vsg_material;
}

vsg::ref_ptr<vsg::DescriptorConfigurator> gltf::SceneGraphBuilder::createUnlitMaterial(vsg::ref_ptr<gltf::Material> gltf_material)
{
    auto vsg_material = vsg::DescriptorConfigurator::create();

    vsg_material->shaderSet = getOrCreateFlatShaderSet();

    auto phongMaterialValue = vsg::PhongMaterialValue::create();
    auto& phongMaterial = phongMaterialValue->value();

    auto texCoordIndicesValue = vsg::TexCoordIndicesValue::create();
    auto& texCoordIndices = texCoordIndicesValue->value();

    if (gltf_material->pbrMetallicRoughness.baseColorFactor.values.size() == 4)
    {
        auto& baseColorFactor = gltf_material->pbrMetallicRoughness.baseColorFactor.values;
        phongMaterial.diffuse.set(baseColorFactor[0], baseColorFactor[1], baseColorFactor[2], baseColorFactor[3]);
        // vsg::info("Assigned phongMaterial.diffuse ", pbrMaterial.baseColorFactor);
    }

    if (gltf_material->pbrMetallicRoughness.baseColorTexture.index)
    {
        auto& textureInfo = gltf_material->pbrMetallicRoughness.baseColorTexture;
        auto& texture = vsg_textures[textureInfo.index.value];
        if (texture.image)
        {
            // vsg::info("Assigned diffuseMap ", texture.image, ", ", texture.sampler);
            vsg_material->assignTexture("diffuseMap", texture.image, texture.sampler);
            texCoordIndices.diffuseMap = textureInfo.texCoord;

            if (auto texture_transform = textureInfo.extension<KHR_texture_transform>("KHR_texture_transform"))
            {
                vsg_material->setObject("KHR_texture_transform", texture_transform);
            }
        }
        else
        {
            vsg::warn("Could not assign diffuseMap ", gltf_material->pbrMetallicRoughness.baseColorTexture.index);
        }
    }

    if (gltf_material->alphaMode == "BLEND")
    {
        vsg_material->blending = true;
    }
    else if (gltf_material->alphaMode == "MASK")
    {
        // vsg_material->blending = true; // TODO, do we need to enable blending?
        vsg_material->defines.insert("VSG_ALPHA_TEST");
        phongMaterial.alphaMaskCutoff = gltf_material->alphaCutoff;
    }

    vsg_material->assignDescriptor("material", phongMaterialValue);

    return vsg_material;
}

vsg::ref_ptr<vsg::DescriptorConfigurator> gltf::SceneGraphBuilder::createMaterial(vsg::ref_ptr<gltf::Material> gltf_material)
{
    auto vsg_material = vsg::DescriptorConfigurator::create();
    if (auto materials_unlit = gltf_material->extension<KHR_materials_unlit>("KHR_materials_unlit"))
        return createUnlitMaterial(gltf_material);
    else
        return createPbrMaterial(gltf_material);
}

vsg::ref_ptr<vsg::Node> gltf::SceneGraphBuilder::createMesh(vsg::ref_ptr<gltf::Mesh> gltf_mesh, const MeshExtras& meshExtras)
{
    /*
    struct Attributes : public vsg::Inherit<vsg::JSONParser::Schema, Attributes>
    {
        std::map<std::string, glTFid> values;
    };

    struct Primitive : public vsg::Inherit<ExtensionsExtras, Primitive>
    {
        Attributes attributes;
        glTFid indices;
        glTFid material;
        uint32_t mode = 0;
        vsg::ObjectsSchema<Attributes> targets;
    };

    struct Mesh : public vsg::Inherit<NameExtensionsExtras, Mesh>
    {
        vsg::ObjectsSchema<Primitive> primitives;
        vsg::ValuesSchema<double> weights;
    };
*/

    const VkPrimitiveTopology topologyLookup[] = {
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST,     // 0, POINTS
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST,      // 1, LINES
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST,      // 2, LINE_LOOP, need special handling
        VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,     // 3, LINE_STRIP
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,  // 4, TRIANGLES
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // 5, TRIANGLE_STRIP
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN    // 6, TRIANGLE_FAN
    };
#if 0
    vsg::info("mesh = {");
    vsg::info("    primitives = ", gltf_mesh->primitives.values.size());
    vsg::info("    weight = ", gltf_mesh->weights.values.size());
#endif

    std::vector<vsg::ref_ptr<vsg::Node>> nodes;

    for (auto& primitive : gltf_mesh->primitives.values)
    {
        vsg::ref_ptr<vsg::DescriptorConfigurator> vsg_material;
        if (primitive->material)
        {
            vsg_material = vsg_materials[primitive->material.value];
        }
        else
        {
            vsg::debug("Material for primitive not assigned, primitive = ", primitive, ", primitive->material = ", primitive->material);
            vsg_material = default_material;
        }

        auto config = vsg::GraphicsPipelineConfigurator::create(vsg_material->shaderSet);
        config->descriptorConfigurator = vsg_material;
        if (options) config->assignInheritedState(options->inheritedState);

        if (meshExtras.jointSampler)
        {
            vsg_material->assignDescriptor("jointMatrices", meshExtras.jointSampler->jointMatrices);
        }

#if 0
        vsg::info("    primitive = {");
        vsg::info("        attributes = {");
        for(auto& [semantic, id] : primitive->attributes.values)
        {
             vsg::info("            ", semantic, ", ", id);
        }
        vsg::info("        }");
        vsg::info("        indices = ", primitive->indices);
        vsg::info("        material = ", primitive->material);
        vsg::info("        mode = ", primitive->mode);
        vsg::info("        targets = ", primitive->targets.values.size()) ;

        vsg::info("        * topology = ", topologyLookup[primitive->mode]) ;
        if (primitive->mode==2) vsg::info("        * LINE_LOOP needs special handling.");

        vsg::info("    }");
#endif

        vsg::DataList vertexArrays;

        auto assignArray = [&](Attributes& attrib, VkVertexInputRate vertexInputRate, const std::string& attribute_name) -> bool {
            auto array_itr = attrib.values.find(attribute_name);
            if (array_itr == attrib.values.end()) return false;

            auto name_itr = attributeLookup.find(attribute_name);
            if (name_itr == attributeLookup.end()) return false;

            if (array_itr->second.value >= vsg_accessors.size())
            {
                vsg::warn("gltf::SceneGraphBuilder::createMesh() error in assignArray( attrib, vertexIndexRate", attribute_name, "), array index out of range.");
                return false;
            }

            vsg::ref_ptr<vsg::Data> array = vsg_accessors[array_itr->second.value];
            if (!array)
            {
                vsg::warn("gltf::SceneGraphBuilder::createMesh() error in assignArray( attrib, vertexIndexRate", attribute_name, "), required array null.");
                return false;
            }

            if (attribute_name == "ROTATION")
            {
                if (auto vec4Rotations = array.cast<vsg::vec4Array>())
                {
                    auto quatArray = vsg::quatArray::create(array, 0, 12, vec4Rotations->size());
                    array = quatArray;
                }
            }
            else if (attribute_name == "TEXCOORD_0" || attribute_name == "TEXCOORD_1" || attribute_name == "TEXCOORD_2" || attribute_name == "TEXCOORD_3")
            {
                if (auto texture_transform = vsg_material->getObject<KHR_texture_transform>("KHR_texture_transform"))
                {
                    vsg::vec2 offset(0.0f, 0.0f);
                    vsg::vec2 scale(1.0f, 1.0f);
                    float rotation = texture_transform->rotation;
                    if (texture_transform->offset.values.size() >= 2) offset.set(texture_transform->offset.values[0], texture_transform->offset.values[1]);
                    if (texture_transform->scale.values.size() >= 2) scale.set(texture_transform->scale.values[0], texture_transform->scale.values[1]);

                    if (auto texCoords = array.cast<vsg::vec2Array>())
                    {
                        float sin_rotation = std::sin(rotation);
                        float cos_rotation = std::cos(rotation);
                        auto transformedTexCoords = vsg::vec2Array::create(texCoords->size());
                        auto dest_itr = transformedTexCoords->begin();
                        for (auto& tc : *texCoords)
                        {
                            auto& dest_tc = *(dest_itr++);
                            dest_tc.x = offset.x + (tc.x * scale.x) * cos_rotation + (tc.y * scale.y) * sin_rotation;
                            dest_tc.y = offset.y + (tc.y * scale.y) * cos_rotation - (tc.x * scale.x) * sin_rotation;
                        }
                        array = transformedTexCoords;
                    }
                }
            }
            else if (attribute_name == "JOINTS_0")
            {
                if (auto ushortCoords = array.cast<vsg::usvec4Array>())
                {
                    auto intCoords = vsg::ivec4Array::create(ushortCoords->size());
                    auto dest_itr = intCoords->begin();
                    for (auto& usc : *ushortCoords)
                    {
                        *(dest_itr++) = vsg::ivec4(usc[0], usc[1], usc[2], usc[3]);
                    }

                    array = intCoords;
                }
                else if (auto ubyteCoords = array.cast<vsg::ubvec4Array>())
                {
                    auto intCoords = vsg::ivec4Array::create(ubyteCoords->size());
                    auto dest_itr = intCoords->begin();
                    for (auto& ubc : *ubyteCoords)
                    {
                        *(dest_itr++) = vsg::ivec4(ubc[0], ubc[1], ubc[2], ubc[3]);
                    }

                    array = intCoords;
                }
            }

            config->assignArray(vertexArrays, name_itr->second, vertexInputRate, array);
            return true;
        };

        if (!assignArray(primitive->attributes, VK_VERTEX_INPUT_RATE_VERTEX, "POSITION"))
        {
            vsg::warn("gltf::SceneGraphBuilder::createMesh() error no vertex array assigned.");
            return {};
        }

        if (!assignArray(primitive->attributes, VK_VERTEX_INPUT_RATE_VERTEX, "NORMAL"))
        {
            auto normal = vsg::vec3Value::create(vsg::vec3(0.0f, 0.0f, 1.0f));
            config->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_INSTANCE, normal);
        }

        if (!assignArray(primitive->attributes, VK_VERTEX_INPUT_RATE_VERTEX, "TEXCOORD_0"))
        {
            auto texcoord = vsg::vec2Value::create(vsg::vec2(0.0f, 0.0f));
            config->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_INSTANCE, texcoord);
        }

        assignArray(primitive->attributes, VK_VERTEX_INPUT_RATE_VERTEX, "TEXCOORD_1");
        assignArray(primitive->attributes, VK_VERTEX_INPUT_RATE_VERTEX, "TEXCOORD_2");
        assignArray(primitive->attributes, VK_VERTEX_INPUT_RATE_VERTEX, "TEXCOORD_3");

        uint32_t vertexCount = vertexArrays.front()->valueCount();
        uint32_t instanceCount = 1;
        if (meshExtras.instancedAttributes)
        {
            for (auto& [name, id] : meshExtras.instancedAttributes->values)
            {
                instanceCount = vsg_accessors[id.value]->valueCount();
            }
        }

        if (!assignArray(primitive->attributes, VK_VERTEX_INPUT_RATE_VERTEX, "COLOR_0"))
        {
            if (instanceNodeHint == vsg::Options::INSTANCE_NONE)
            {
                auto defaultColor = vsg::vec4Array::create(instanceCount, vsg::vec4(1.0f, 1.0f, 1.0f, 1.0f));
                config->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, defaultColor);
            }
            else if ((instanceNodeHint & vsg::Options::INSTANCE_COLORS) == 0)
            {
                auto defaultColor = vsg::vec4Array::create(vertexCount, vsg::vec4(1.0f, 1.0f, 1.0f, 1.0f));
                config->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_VERTEX, defaultColor);
            }
        }

        if (meshExtras.jointSampler)
        {
            assignArray(primitive->attributes, VK_VERTEX_INPUT_RATE_VERTEX, "JOINTS_0");
            assignArray(primitive->attributes, VK_VERTEX_INPUT_RATE_VERTEX, "WEIGHTS_0");
        }

        if (meshExtras.instancedAttributes)
        {
            assignArray(*meshExtras.instancedAttributes, VK_VERTEX_INPUT_RATE_INSTANCE, "TRANSLATION");
            assignArray(*meshExtras.instancedAttributes, VK_VERTEX_INPUT_RATE_INSTANCE, "ROTATION");
            assignArray(*meshExtras.instancedAttributes, VK_VERTEX_INPUT_RATE_INSTANCE, "SCALE");
        }

        vsg::ref_ptr<vsg::Node> draw;

        if (!meshExtras.instancedAttributes && instanceNodeHint != vsg::Options::INSTANCE_NONE)
        {
            if ((instanceNodeHint & vsg::Options::INSTANCE_COLORS) != 0) config->enableArray("vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, 16, VK_FORMAT_R32G32B32A32_SFLOAT);
            if ((instanceNodeHint & vsg::Options::INSTANCE_TRANSLATIONS) != 0) config->enableArray("vsg_Translation", VK_VERTEX_INPUT_RATE_INSTANCE, 12, VK_FORMAT_R32G32B32_SFLOAT);
            if ((instanceNodeHint & vsg::Options::INSTANCE_ROTATIONS) != 0) config->enableArray("vsg_Rotation", VK_VERTEX_INPUT_RATE_INSTANCE, 16, VK_FORMAT_R32G32B32A32_SFLOAT);
            if ((instanceNodeHint & vsg::Options::INSTANCE_SCALES) != 0) config->enableArray("vsg_Scale", VK_VERTEX_INPUT_RATE_INSTANCE, 12, VK_FORMAT_R32G32B32_SFLOAT);

            if (primitive->indices)
            {
                auto instanceDrawIndexed = vsg::InstanceDrawIndexed::create();
                assign_extras(*primitive, *instanceDrawIndexed);
                instanceDrawIndexed->assignArrays(vertexArrays);

                auto indices = vsg_accessors[primitive->indices.value];
                if (!indices)
                {
                    vsg::warn("gltf::SceneGraphBuilder::createMesh() error required indices array null.");
                    return {};
                }

                if (auto ubyte_indices = indices.cast<vsg::ubyteArray>())
                {
                    // need to promote ubyte indices to ushort as Vulkan requires an extension to be enabled for ubyte indices.
                    auto ushort_indices = vsg::ushortArray::create(ubyte_indices->size());
                    auto itr = ushort_indices->begin();
                    for (auto value : *ubyte_indices)
                    {
                        *(itr++) = static_cast<uint16_t>(value);
                    }

                    instanceDrawIndexed->assignIndices(ushort_indices);
                    instanceDrawIndexed->indexCount = static_cast<uint32_t>(ushort_indices->valueCount());
                }
                else
                {
                    instanceDrawIndexed->assignIndices(indices);
                    instanceDrawIndexed->indexCount = static_cast<uint32_t>(indices->valueCount());
                }
                draw = instanceDrawIndexed;
            }
            else
            {
                auto instanceDraw = vsg::InstanceDraw::create();
                assign_extras(*primitive, *instanceDraw);
                instanceDraw->assignArrays(vertexArrays);

                assign_extras(*primitive, *instanceDraw);
                instanceDraw->vertexCount = vertexCount;
                draw = instanceDraw;
            }
        }
        else if (primitive->indices)
        {
            auto vid = vsg::VertexIndexDraw::create();
            assign_extras(*primitive, *vid);
            vid->assignArrays(vertexArrays);
            vid->instanceCount = instanceCount;

            auto indices = vsg_accessors[primitive->indices.value];
            if (!indices)
            {
                vsg::warn("gltf::SceneGraphBuilder::createMesh() error required indices array null.");
                return {};
            }

            if (auto ubyte_indices = indices.cast<vsg::ubyteArray>())
            {
                // need to promote ubyte indices to ushort as Vulkan requires an extension to be enabled for ubyte indices.
                auto ushort_indices = vsg::ushortArray::create(ubyte_indices->size());
                auto itr = ushort_indices->begin();
                for (auto value : *ubyte_indices)
                {
                    *(itr++) = static_cast<uint16_t>(value);
                }

                vid->assignIndices(ushort_indices);
                vid->indexCount = static_cast<uint32_t>(ushort_indices->valueCount());
            }
            else
            {
                vid->assignIndices(indices);
                vid->indexCount = static_cast<uint32_t>(indices->valueCount());
            }

            draw = vid;
        }
        else
        {
            auto vd = vsg::VertexDraw::create();
            assign_extras(*primitive, *vd);
            vd->assignArrays(vertexArrays);
            vd->instanceCount = instanceCount;
            vd->vertexCount = vertexCount;
            draw = vd;
        }

        // set the GraphicsPipelineStates to the required values.
        struct SetPipelineStates : public vsg::Visitor
        {
            VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            bool blending = false;
            bool two_sided = false;

            SetPipelineStates(VkPrimitiveTopology in_topology, bool in_blending, bool in_two_sided) :
                topology(in_topology), blending(in_blending), two_sided(in_two_sided) {}

            void apply(vsg::Object& object) { object.traverse(*this); }
            void apply(vsg::RasterizationState& rs)
            {
                if (two_sided) rs.cullMode = VK_CULL_MODE_NONE;
            }
            void apply(vsg::InputAssemblyState& ias) { ias.topology = topology; }
            void apply(vsg::ColorBlendState& cbs) { cbs.configureAttachments(blending); }

        } sps(topologyLookup[primitive->mode], vsg_material->blending, vsg_material->two_sided);

        config->accept(sps);

        if (sharedObjects)
            sharedObjects->share(config, [](auto gpc) { gpc->init(); });
        else
            config->init();

        // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
        auto stateGroup = vsg::StateGroup::create();

        config->copyTo(stateGroup, sharedObjects);

        stateGroup->addChild(draw);

        if (vsg_material->blending)
        {
            if (meshExtras.instancedAttributes || instanceNodeHint != vsg::Options::INSTANCE_NONE)
            {
#if 0
                auto layer = vsg::Layer::create();
                layer->binNumber = 10;
                layer->child = stateGroup;

                nodes.push_back(layer);
#else
                nodes.push_back(stateGroup);
#endif
            }
            else
            {
                vsg::ComputeBounds computeBounds;
                draw->accept(computeBounds);
                vsg::dvec3 center = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
                double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.5;

                auto depthSorted = vsg::DepthSorted::create();
                depthSorted->binNumber = 10;
                depthSorted->bound.set(center[0], center[1], center[2], radius);
                depthSorted->child = stateGroup;

                nodes.push_back(depthSorted);
            }
        }
        else
        {
            nodes.push_back(stateGroup);
        }
    }

    if (nodes.empty())
    {
        vsg::warn("Empty mesh");
        return {};
    }

    vsg::ref_ptr<vsg::Node> vsg_mesh;
    if (nodes.size() == 1)
    {
        // vsg::info("Mesh with single primtiive");
        vsg_mesh = nodes.front();
    }
    else
    {
        // vsg::info("Mesh with multiple primtiives - could possible use vsg::Geomterty.");
        auto group = vsg::Group::create();
        for (auto node : nodes)
        {
            group->addChild(node);
        }

        vsg_mesh = group;
    }

    assign_name_extras(*gltf_mesh, *vsg_mesh);

    return vsg_mesh;
}

bool gltf::SceneGraphBuilder::getTransform(gltf::Node& node, vsg::dmat4& matrix)
{
    if (node.matrix.values.size() == 16)
    {
        auto& m = node.matrix.values;
        matrix.set(m[0], m[1], m[2], m[3],
                   m[4], m[5], m[6], m[7],
                   m[8], m[9], m[10], m[11],
                   m[12], m[13], m[14], m[15]);
        return true;
    }
    else if (!(node.translation.values.empty()) ||
             !(node.rotation.values.empty()) ||
             !(node.scale.values.empty()))
    {
        auto& t = node.translation.values;
        auto& r = node.rotation.values;
        auto& s = node.scale.values;

        vsg::dvec3 vsg_t(0.0, 0.0, 0.0);
        vsg::dquat vsg_r;
        vsg::dvec3 vsg_s(1.0, 1.0, 1.0);

        if (t.size() >= 3) vsg_t.set(t[0], t[1], t[2]);
        if (r.size() >= 4) vsg_r.set(r[0], r[1], r[2], r[3]);
        if (s.size() >= 3) vsg_s.set(s[0], s[1], s[2]);

        matrix = vsg::translate(vsg_t) * vsg::rotate(vsg_r) * vsg::scale(vsg_s);
        return true;
    }
    else
    {
        return false;
    }
}

vsg::ref_ptr<vsg::Light> gltf::SceneGraphBuilder::createLight(vsg::ref_ptr<gltf::Light> gltf_light)
{
    bool range_set = gltf_light->range != std::numeric_limits<float>::max();

    vsg::ref_ptr<vsg::Light> vsg_light;
    if (gltf_light->type == "directional")
    {
        auto directionalLight = vsg::DirectionalLight::create();
        vsg_light = directionalLight;
    }
    else if (gltf_light->type == "point")
    {
        auto pointLight = vsg::PointLight::create();
        if (range_set) pointLight->radius = gltf_light->range;
        vsg_light = pointLight;
    }
    else if (gltf_light->type == "spot")
    {
        auto spotLight = vsg::SpotLight::create();
        if (range_set) spotLight->radius = gltf_light->range;
        vsg_light = spotLight;
        if (gltf_light->spot)
        {
            spotLight->innerAngle = gltf_light->spot->innerConeAngle;
            spotLight->outerAngle = gltf_light->spot->outerConeAngle;
        }
    }
    else
    {
        return {};
    }

    vsg_light->name = gltf_light->name;
    vsg_light->intensity = gltf_light->intensity;
    if (gltf_light->color.values.size() >= 3) vsg_light->color.set(gltf_light->color.values[0], gltf_light->color.values[1], gltf_light->color.values[2]);

    return vsg_light;
}

vsg::ref_ptr<vsg::Animation> gltf::SceneGraphBuilder::createAnimation(vsg::ref_ptr<gltf::Animation> gltf_animation)
{
    vsg::LogOutput log;

    auto vsg_animation = vsg::Animation::create();
    vsg_animation->name = gltf_animation->name;

    // gltf_animation->report(log);

    struct NodeChannels
    {
        vsg::ref_ptr<AnimationChannel> translation;
        vsg::ref_ptr<AnimationChannel> rotation;
        vsg::ref_ptr<AnimationChannel> scale;
        vsg::ref_ptr<AnimationChannel> weights;
    };

    std::map<uint32_t, NodeChannels> nodeChannels;

    for (auto& channel : gltf_animation->channels.values)
    {
        auto node_id = channel->target.node.value;

        if (channel->target.path == "translation")
            nodeChannels[node_id].translation = channel;
        else if (channel->target.path == "rotation")
            nodeChannels[node_id].rotation = channel;
        else if (channel->target.path == "scale")
            nodeChannels[node_id].scale = channel;
        else if (channel->target.path == "weights")
            nodeChannels[node_id].weights = channel;
        else
            vsg::warn("gltf::SceneGraphBuilder::createSceneGraph() unsupported AnimationChannel.target.path of ", channel->target.path);
    }

    for (auto& [node_id, channels] : nodeChannels)
    {
        if (channels.translation || channels.rotation || channels.scale)
        {
            auto keyframes = vsg::TransformKeyframes::create();

            if (channels.translation)
            {
                auto samplerID = channels.translation->sampler.value;
                auto sampler = gltf_animation->samplers.values[samplerID];
                auto vsg_input = vsg_accessors[sampler->input.value];
                auto vsg_output = vsg_accessors[sampler->output.value];

                auto timeValues = vsg_input.cast<vsg::floatArray>();
                auto translationValues = vsg_output.cast<vsg::vec3Array>();

                if (timeValues && translationValues)
                {
                    size_t count = std::min(vsg_input->valueCount(), vsg_output->valueCount());

                    auto& translations = keyframes->positions;
                    translations.resize(count);

                    for (size_t i = 0; i < count; ++i)
                    {
                        const auto& t = translationValues->at(i);
                        translations[i].time = timeValues->at(i);
                        translations[i].value.set(t.x, t.y, t.z);
                    }
                }
                else
                {
                    vsg::warn("gltf::SceneGraphBuilder::createAnimation(..) unsupported translation types. vsg_input = ", vsg_input, ", vsg_output = ", vsg_output);
                }
            }

            if (channels.rotation)
            {
                auto samplerID = channels.rotation->sampler.value;
                auto sampler = gltf_animation->samplers.values[samplerID];
                auto vsg_input = vsg_accessors[sampler->input.value];
                auto vsg_output = vsg_accessors[sampler->output.value];

                auto timeValues = vsg_input.cast<vsg::floatArray>();
                auto rotationValues = vsg_output.cast<vsg::vec4Array>();

                if (timeValues && rotationValues)
                {
                    size_t count = std::min(vsg_input->valueCount(), vsg_output->valueCount());

                    auto& rotations = keyframes->rotations;
                    rotations.resize(count);

                    for (size_t i = 0; i < count; ++i)
                    {
                        const auto& q = rotationValues->at(i);
                        rotations[i].time = timeValues->at(i);
                        rotations[i].value.set(q.x, q.y, q.z, q.w);
                    }
                }
                else
                {
                    vsg::warn("gltf::SceneGraphBuilder::createAnimation(..) unsupported rotation types. vsg_input = ", vsg_input, ", vsg_output = ", vsg_output);
                }
            }

            if (channels.scale)
            {
                auto samplerID = channels.scale->sampler.value;
                auto sampler = gltf_animation->samplers.values[samplerID];
                auto vsg_input = vsg_accessors[sampler->input.value];
                auto vsg_output = vsg_accessors[sampler->output.value];

                auto timeValues = vsg_input.cast<vsg::floatArray>();
                auto scaleValues = vsg_output.cast<vsg::vec3Array>();

                if (timeValues && scaleValues)
                {
                    size_t count = std::min(vsg_input->valueCount(), vsg_output->valueCount());

                    auto& scales = keyframes->scales;
                    scales.resize(count);

                    for (size_t i = 0; i < count; ++i)
                    {
                        const auto& s = scaleValues->at(i);
                        scales[i].time = timeValues->at(i);
                        scales[i].value.set(s.x, s.y, s.z);
                    }
                }
                else
                {
                    vsg::warn("gltf::SceneGraphBuilder::createAnimation(..) unsupported scale types. vsg_input = ", vsg_input, ", vsg_output = ", vsg_output);
                }
            }

            auto transformSampler = vsg::TransformSampler::create();

            auto node = vsg_nodes[node_id];
            if (auto mt = node.cast<vsg::MatrixTransform>())
            {
                vsg::decompose(mt->matrix, transformSampler->position, transformSampler->rotation, transformSampler->scale);
            }
            else if (auto joint = node.cast<vsg::Joint>())
            {
                vsg::decompose(joint->matrix, transformSampler->position, transformSampler->rotation, transformSampler->scale);
            }

            transformSampler->keyframes = keyframes;
            transformSampler->object = vsg_nodes[node_id];

            vsg_animation->samplers.push_back(transformSampler);
        }
    }

    return vsg_animation;
}

vsg::ref_ptr<vsg::Node> gltf::SceneGraphBuilder::createNode(vsg::ref_ptr<gltf::Node> gltf_node, bool jointNode)
{
    vsg::ref_ptr<vsg::Node> vsg_node;

    vsg::ref_ptr<vsg::Light> vsg_light;
    if (auto khr_lights = gltf_node->extension<KHR_lights_punctual>("KHR_lights_punctual"))
    {
        auto id = khr_lights->light;
        if (id && id.value < vsg_lights.size())
        {
            vsg_light = vsg_lights[id.value];
        }
    }

    MeshExtras meshExtras;

    if (gltf_node->skin)
    {
        meshExtras.jointSampler = vsg_skins[gltf_node->skin.value];
    }

    vsg::ref_ptr<vsg::Node> vsg_mesh;
    if (gltf_node->mesh)
    {
        vsg_mesh = vsg_meshes[gltf_node->mesh.value];
        auto gltf_mesh = model->meshes.values[gltf_node->mesh.value];

        if (auto mesh_gpu_instancing = gltf_node->extension<EXT_mesh_gpu_instancing>("EXT_mesh_gpu_instancing"))
        {
            meshExtras.instancedAttributes = mesh_gpu_instancing->attributes;
        }

        if (!vsg_meshes[gltf_node->mesh.value])
        {
            vsg_meshes[gltf_node->mesh.value] = createMesh(gltf_mesh, meshExtras);
        }

        vsg_mesh = vsg_meshes[gltf_node->mesh.value];
    }

    bool isTransform = !(gltf_node->matrix.values.empty()) ||
                       !(gltf_node->rotation.values.empty()) ||
                       !(gltf_node->scale.values.empty()) ||
                       !(gltf_node->translation.values.empty());

    size_t numChildren = gltf_node->children.values.size();
    if (gltf_node->camera) ++numChildren;
    if (vsg_mesh) ++numChildren;
    if (vsg_light) ++numChildren;

    if (jointNode)
    {
        auto joint = vsg::Joint::create();
        if (gltf_node->camera) joint->addChild(vsg_cameras[gltf_node->camera.value]);
        if (vsg_light) joint->addChild(vsg_light);
        if (vsg_mesh) joint->addChild(vsg_mesh);

        if (gltf_node->matrix.values.size() == 16)
        {
            auto& m = gltf_node->matrix.values;
            joint->matrix.set(m[0], m[1], m[2], m[3],
                              m[4], m[5], m[6], m[7],
                              m[8], m[9], m[10], m[11],
                              m[12], m[13], m[14], m[15]);
        }
        else
        {
            auto& t = gltf_node->translation.values;
            auto& r = gltf_node->rotation.values;
            auto& s = gltf_node->scale.values;

            vsg::dvec3 vsg_t(0.0, 0.0, 0.0);
            vsg::dquat vsg_r;
            vsg::dvec3 vsg_s(1.0, 1.0, 1.0);

            if (t.size() >= 3) vsg_t.set(t[0], t[1], t[2]);
            if (r.size() >= 4) vsg_r.set(r[0], r[1], r[2], r[3]);
            if (s.size() >= 3) vsg_s.set(s[0], s[1], s[2]);

            joint->matrix = vsg::translate(vsg_t) * vsg::rotate(vsg_r) * vsg::scale(vsg_s);
        }

        vsg_node = joint;
    }
    else if (isTransform)
    {
        auto transform = vsg::MatrixTransform::create();
        if (gltf_node->camera) transform->addChild(vsg_cameras[gltf_node->camera.value]);
        if (vsg_light) transform->addChild(vsg_light);
        if (vsg_mesh) transform->addChild(vsg_mesh);

        if (gltf_node->matrix.values.size() == 16)
        {
            auto& m = gltf_node->matrix.values;
            transform->matrix.set(m[0], m[1], m[2], m[3],
                                  m[4], m[5], m[6], m[7],
                                  m[8], m[9], m[10], m[11],
                                  m[12], m[13], m[14], m[15]);
        }
        else
        {
            auto& t = gltf_node->translation.values;
            auto& r = gltf_node->rotation.values;
            auto& s = gltf_node->scale.values;

            vsg::dvec3 vsg_t(0.0, 0.0, 0.0);
            vsg::dquat vsg_r;
            vsg::dvec3 vsg_s(1.0, 1.0, 1.0);

            if (t.size() >= 3) vsg_t.set(t[0], t[1], t[2]);
            if (r.size() >= 4) vsg_r.set(r[0], r[1], r[2], r[3]);
            if (s.size() >= 3) vsg_s.set(s[0], s[1], s[2]);

            transform->matrix = vsg::translate(vsg_t) * vsg::rotate(vsg_r) * vsg::scale(vsg_s);
        }

        vsg_node = transform;
    }
    else if (numChildren > 1 || gltf_node->requireMetaData())
    {
        auto group = vsg::Group::create();

        if (gltf_node->camera) group->addChild(vsg_cameras[gltf_node->camera.value]);
        if (vsg_light) group->addChild(vsg_light);
        if (vsg_mesh) group->addChild(vsg_mesh);

        vsg_node = group;
    }
    else
    {
        if (gltf_node->camera)
            vsg_node = vsg_cameras[gltf_node->camera.value];
        else if (vsg_mesh)
            vsg_node = vsg_mesh;
        else if (vsg_light)
            vsg_node = vsg_light;
        else
            vsg_node = vsg::Group::create(); // TODO: single child so should this just point to the child?
    }

    assign_name_extras(*gltf_node, *vsg_node);

    return vsg_node;
}

void gltf::SceneGraphBuilder::flattenTransforms(gltf::Node& node, const vsg::dmat4& inheritedTransform)
{
    vsg::dmat4 accumulatedTransform = inheritedTransform;

    vsg::dmat4 localMatrix;
    if (getTransform(node, localMatrix))
    {
        accumulatedTransform = accumulatedTransform * localMatrix;

        // clear node transform values so that they aren't reapplied.
        node.matrix.values.clear();
        node.rotation.values.clear();
        node.scale.values.clear();
        node.translation.values.clear();
    }

    if (node.camera)
    {
        vsg::info("TODO: need to flatten camera ", node.camera);
    }

    if (node.skin)
    {
        vsg::info("TODO: need to flatten skin ", node.skin);
    }

    auto inverse_accumulatedTransform = vsg::inverse(accumulatedTransform);

    if (node.mesh)
    {
        auto mesh = model->meshes.values[node.mesh.value];
        for (auto primitive : mesh->primitives.values)
        {
            if (auto position_itr = primitive->attributes.values.find("POSITION"); position_itr != primitive->attributes.values.end())
            {
                auto data = vsg_accessors[position_itr->second.value];
                auto vertices = data.cast<vsg::vec3Array>();

                for (auto& v : *vertices)
                {
                    v = accumulatedTransform * vsg::dvec3(v);
                }
            }
            if (auto normal_itr = primitive->attributes.values.find("NORMAL"); normal_itr != primitive->attributes.values.end())
            {
                auto data = vsg_accessors[normal_itr->second.value];
                auto normals = data.cast<vsg::vec3Array>();

                for (auto& n : *normals)
                {
                    n = vsg::dvec3(n) * inverse_accumulatedTransform;
                }
            }
        }
    }

    for (auto& id : node.children.values)
    {
        auto child = model->nodes.values[id.value];
        flattenTransforms(*child, accumulatedTransform);
    }
}

vsg::ref_ptr<vsg::Node> gltf::SceneGraphBuilder::createScene(vsg::ref_ptr<gltf::Scene> gltf_scene, bool requiresRootTransformNode, const vsg::dmat4& rootTransform)
{
    if (gltf_scene->nodes.values.empty())
    {
        vsg::warn("Cannot create scene graph from empty gltf::Scene.");
        return {};
    }

    vsg::Group::Children children;
    for (auto& id : gltf_scene->nodes.values)
    {
        if (vsg_nodes[id.value]) children.push_back(vsg_nodes[id.value]);
    }

    // add transform node if required
    if (requiresRootTransformNode)
    {
        auto transform = vsg::MatrixTransform::create(rootTransform);
        transform->children.swap(children);

        children.clear();
        children.push_back(transform);
    }

    // add animation group if required.
    if (!vsg_animations.empty())
    {
        auto animationGroup = vsg::AnimationGroup::create();
        animationGroup->animations = vsg_animations;

        animationGroup->children.swap(children);

        children.clear();
        children.push_back(animationGroup);
    }

    // All culling node if required.
    bool culling = vsg::value<bool>(true, gltf::culling, options) && (instanceNodeHint == vsg::Options::INSTANCE_NONE);
    if (culling)
    {
        if (auto bounds = vsg::visit<vsg::ComputeBounds>(children).bounds)
        {
            vsg::dsphere bs((bounds.max + bounds.min) * 0.5, vsg::length(bounds.max - bounds.min) * 0.5);
            if (children.size() == 1)
            {
                auto cullNode = vsg::CullNode::create(bs, children[0]);

                children.clear();
                children.push_back(cullNode);
            }
            else
            {
                auto cullGroup = vsg::CullGroup::create(bs);
                cullGroup->children.swap(children);

                children.clear();
                children.push_back(cullGroup);
            }
        }
    }

    if (children.size() > 1)
    {
        auto group = vsg::Group::create();
        group->children.swap(children);
        children.clear();
        children.push_back(group);
    }

    if (children.empty()) return {};

    vsg::ref_ptr<vsg::Node> vsg_scene = children[0];

    // assign meta data
    assign_name_extras(*gltf_scene, *vsg_scene);

    return vsg_scene;
}

#ifdef vsgXchange_draco
template<typename T>
static bool CopyDracoAttributes(const draco::PointAttribute* draco_attribute, void* ptr, draco::PointIndex::ValueType num_points)
{
    T* dest_ptr = reinterpret_cast<T*>(ptr);

    auto num_components = draco_attribute->num_components();
    for (draco::PointIndex i(0); i < num_points; ++i)
    {
        auto index = draco_attribute->mapped_index(i);
        if (!draco_attribute->ConvertValue(index, num_components, dest_ptr)) return false;

        dest_ptr += num_components;
    }

    return true;
}
#endif

bool gltf::SceneGraphBuilder::decodePrimitiveIfRequired(vsg::ref_ptr<gltf::Primitive> primitive)
{
    if (auto draco_mesh_compression = primitive->extension<KHR_draco_mesh_compression>("KHR_draco_mesh_compression"))
    {
#ifdef vsgXchange_draco
        auto& bufferView = model->bufferViews.values[draco_mesh_compression->bufferView.value];
        auto& buffer = model->buffers.values[bufferView->buffer.value];

        auto bufferViewData = static_cast<const char*>(buffer->data->dataPointer()) + bufferView->byteOffset;
        auto bufferViewSize = bufferView->byteLength;

        draco::DecoderBuffer decodeBuffer;
        decodeBuffer.Init(bufferViewData, bufferViewSize);

        draco::Decoder decoder;
        auto result = decoder.DecodeMeshFromBuffer(&decodeBuffer);

        auto& mesh = result.value();
        auto num_points = mesh->num_points();

        // process indices
        if (primitive->indices)
        {
            auto& indices = model->accessors.values[primitive->indices.value];

            // set the indices bufferView to the what will be the index value of bufferView to be created for the indices
            indices->bufferView.value = static_cast<uint32_t>(model->bufferViews.values.size());

            // hardwire to uint32_t for now
            uint32_t componentSize = (num_points < 65536) ? 2 : 4;
            uint32_t count = mesh->num_faces() * 3;

            // set up BufferView for the indices
            auto indexBufferView = gltf::BufferView::create();
            indexBufferView->buffer.value = static_cast<uint32_t>(model->buffers.values.size());
            indexBufferView->byteOffset = 0;
            indexBufferView->byteLength = count * componentSize;
            model->bufferViews.values.push_back(indexBufferView);

            // set up Buffer for the indices
            auto indexBuffer = gltf::Buffer::create();
            indexBuffer->byteLength = indexBufferView->byteLength;
            indexBuffer->data = vsg::ubyteArray::create(indexBuffer->byteLength);
            model->buffers.values.push_back(indexBuffer);

            if (componentSize == sizeof(draco::PointIndex))
            {
                indices->componentType = COMPONENT_TYPE_UNSIGNED_INT;
                indices->count = count;

                // compatible size so can just copy data directly
                memcpy(indexBuffer->data->dataPointer(), &(mesh->face(draco::FaceIndex(0)))[0], indexBuffer->byteLength);
            }
            else
            {
                indices->componentType = COMPONENT_TYPE_UNSIGNED_SHORT;
                indices->count = count;

                // copy data across value by value converting to ushort type
                uint16_t* dest = static_cast<uint16_t*>(indexBuffer->data->dataPointer());
                for (draco::FaceIndex i(0); i < mesh->num_faces(); ++i)
                {
                    const auto& face = mesh->face(i);
                    *(dest++) = static_cast<uint16_t>(face[0].value());
                    *(dest++) = static_cast<uint16_t>(face[1].value());
                    *(dest++) = static_cast<uint16_t>(face[2].value());
                }
            }
        }

        auto& draco_attributes = draco_mesh_compression->attributes.values;
        auto& primitive_attributes = primitive->attributes.values;

        for (auto& [name, id] : draco_attributes)
        {
            if (auto itr = primitive_attributes.find(name); itr != primitive_attributes.end())
            {
                auto& primitive_attribute = *itr;

                const auto draco_attribute = mesh->GetAttributeByUniqueId(id.value);
                auto& accessor = model->accessors.values[primitive_attribute.second.value];

                // update the attribute accessor to the decode entry
                accessor->bufferView.value = static_cast<uint32_t>(model->bufferViews.values.size());
                accessor->count = num_points;

                auto dataProperties = accessor->getDataProperties();
                uint32_t valueSize = dataProperties.componentSize * dataProperties.componentCount;

                // allocate buffer and bufferView for decoded attribute
                auto attributeBufferView = gltf::BufferView::create();
                attributeBufferView->buffer.value = static_cast<uint32_t>(model->buffers.values.size());
                attributeBufferView->byteLength = num_points * valueSize;
                attributeBufferView->byteOffset = draco_attribute->byte_offset();
                attributeBufferView->byteStride = draco_attribute->byte_stride();
                model->bufferViews.values.push_back(attributeBufferView);

                auto attributeBuffer = gltf::Buffer::create();
                attributeBuffer->byteLength = attributeBufferView->byteLength;
                attributeBuffer->data = vsg::ubyteArray::create(attributeBuffer->byteLength);
                model->buffers.values.push_back(attributeBuffer);

                auto* ptr = attributeBuffer->data->dataPointer();

                // TODO get the attributes from the draco mesh.
                switch (accessor->componentType)
                {
                case (COMPONENT_TYPE_BYTE):
                    CopyDracoAttributes<int8_t>(draco_attribute, ptr, num_points);
                    break;
                case (COMPONENT_TYPE_UNSIGNED_BYTE):
                    CopyDracoAttributes<uint8_t>(draco_attribute, ptr, num_points);
                    break;
                case (COMPONENT_TYPE_SHORT):
                    CopyDracoAttributes<int16_t>(draco_attribute, ptr, num_points);
                    break;
                case (COMPONENT_TYPE_UNSIGNED_SHORT):
                    CopyDracoAttributes<uint16_t>(draco_attribute, ptr, num_points);
                    break;
                case (COMPONENT_TYPE_INT):
                    CopyDracoAttributes<int32_t>(draco_attribute, ptr, num_points);
                    break;
                case (COMPONENT_TYPE_UNSIGNED_INT):
                    CopyDracoAttributes<uint32_t>(draco_attribute, ptr, num_points);
                    break;
                case (COMPONENT_TYPE_FLOAT):
                    CopyDracoAttributes<float>(draco_attribute, ptr, num_points);
                    break;
                case (COMPONENT_TYPE_DOUBLE):
                    CopyDracoAttributes<double>(draco_attribute, ptr, num_points);
                    break;
                default:
                    vsg::info("unsupported type ", dataProperties.componentType);
                    break;
                }
            }
        }

        return true;
#else
        vsg::info("Primitive draco_mesh_compression = ", draco_mesh_compression, " not supported.");
        return false;
#endif
    }
    return true;
}

vsg::ref_ptr<vsg::ShaderSet> gltf::SceneGraphBuilder::getOrCreatePbrShaderSet()
{
    if (pbrShaderSet) return pbrShaderSet;

    pbrShaderSet = vsg::createPhysicsBasedRenderingShaderSet(options);
    if (sharedObjects) sharedObjects->share(pbrShaderSet);

    return pbrShaderSet;
}

vsg::ref_ptr<vsg::ShaderSet> gltf::SceneGraphBuilder::getOrCreateFlatShaderSet()
{
    if (flatShaderSet) return flatShaderSet;

    flatShaderSet = vsg::createFlatShadedShaderSet(options);
    if (sharedObjects) sharedObjects->share(flatShaderSet);

    return flatShaderSet;
}

vsg::ref_ptr<vsg::Object> gltf::SceneGraphBuilder::createSceneGraph(vsg::ref_ptr<gltf::glTF> in_model, vsg::ref_ptr<const vsg::Options> in_options)
{
    model = in_model;
    if (!model) return {};

    if (in_options) options = in_options;

    if (options) sharedObjects = options->sharedObjects;
    if (!sharedObjects) sharedObjects = vsg::SharedObjects::create();

    instanceNodeHint = options ? options->instanceNodeHint : vsg::Options::INSTANCE_NONE;
    cloneAccessors = vsg::value<bool>(cloneAccessors, gltf::clone_accessors, options);
    maxAnisotropy = vsg::value<float>(maxAnisotropy, gltf::maxAnisotropy, options);

    // TODO: need to check that the glTF model is suitable for use of InstanceNode/InstanceDraw

    // vsg::info("gltf::SceneGraphBuilder::createSceneGraph() instanceNodeHint = ", instanceNodeHint);

    vsg::CoordinateConvention destination_coordinateConvention = vsg::CoordinateConvention::Z_UP;
    if (options) destination_coordinateConvention = options->sceneCoordinateConvention;

    vsg::dmat4 rootTransform;
    bool requiresRootTransformNode = vsg::transform(source_coordinateConvention, destination_coordinateConvention, rootTransform);

    if (!default_material)
    {
        default_material = vsg::DescriptorConfigurator::create();
        default_material->shaderSet = getOrCreatePbrShaderSet();

        auto pbrMaterialValue = vsg::PbrMaterialValue::create();
        auto& pbrMaterial = pbrMaterialValue->value();

        // defaults make surface grey and washed out, so reset them to provide something closer to glTF example viewer.
        pbrMaterial.metallicFactor = 0.0f;
        pbrMaterial.roughnessFactor = 0.0f;

        default_material->assignDescriptor("material", pbrMaterialValue);
    }

    for (size_t mi = 0; mi < model->meshes.values.size(); ++mi)
    {
        auto mesh = model->meshes.values[mi];
        for (auto primitive : mesh->primitives.values)
        {
            if (!decodePrimitiveIfRequired(primitive))
            {
                vsg::info("Reqires draco decompression but no support available.");
                return {};
            }
        }
    }

    vsg_buffers.resize(model->buffers.values.size());
    for (size_t bi = 0; bi < model->buffers.values.size(); ++bi)
    {
        vsg_buffers[bi] = createBuffer(model->buffers.values[bi]);
    }

    vsg_bufferViews.resize(model->bufferViews.values.size());
    for (size_t bvi = 0; bvi < model->bufferViews.values.size(); ++bvi)
    {
        vsg_bufferViews[bvi] = createBufferView(model->bufferViews.values[bvi]);
    }

    vsg_accessors.resize(model->accessors.values.size());
    for (size_t ai = 0; ai < model->accessors.values.size(); ++ai)
    {
        vsg_accessors[ai] = createAccessor(model->accessors.values[ai]);
    }

    if (instanceNodeHint != vsg::Options::INSTANCE_NONE)
    {
        requiresRootTransformNode = false;
        //vsg::info("Need to flatten all transforms");

        for (size_t sci = 0; sci < model->scenes.values.size(); ++sci)
        {
            auto gltf_scene = model->scenes.values[sci];
            for (auto& id : gltf_scene->nodes.values)
            {
                flattenTransforms(*(model->nodes.values[id.value]), rootTransform);
            }
        }
    }

    // vsg::info("create cameras = ", model->cameras.values.size());
    vsg_cameras.resize(model->cameras.values.size());
    for (size_t ci = 0; ci < model->cameras.values.size(); ++ci)
    {
        vsg_cameras[ci] = createCamera(model->cameras.values[ci]);
    }

    // set which nodes are joints
    vsg_joints.resize(model->nodes.values.size(), false);
    for (size_t si = 0; si < model->skins.values.size(); ++si)
    {
        auto& gltf_skin = model->skins.values[si];
        for (auto joint : gltf_skin->joints.values)
        {
            vsg_joints[joint.value] = true;
        }
    }

    // vsg::info("create samplers = ", model->samplers.values.size());
    vsg_samplers.resize(model->samplers.values.size());
    std::vector<uint32_t> maxDimensions(model->samplers.values.size(), 0);
    for (size_t sai = 0; sai < model->samplers.values.size(); ++sai)
    {
        vsg_samplers[sai] = createSampler(model->samplers.values[sai]);
    }

    // vsg::info("create images = ", model->images.values.size());
    vsg_images.resize(model->images.values.size());
    for (size_t ii = 0; ii < model->images.values.size(); ++ii)
    {
        if (model->images.values[ii]) vsg_images[ii] = createImage(model->images.values[ii]);
    }

    // vsg::info("create textures = ", model->textures.values.size());
    vsg_textures.resize(model->textures.values.size());
    for (size_t ti = 0; ti < model->textures.values.size(); ++ti)
    {
        auto& gltf_texture = model->textures.values[ti];
        auto& si = vsg_textures[ti] = createTexture(gltf_texture);

        if (si.sampler && si.image)
        {
            auto& maxDimension = maxDimensions[gltf_texture->sampler.value];
            if (si.image->width() > maxDimension) maxDimension = si.image->width();
            if (si.image->height() > maxDimension) maxDimension = si.image->height();
            if (si.image->depth() > maxDimension) maxDimension = si.image->depth();
        }
    }

    // reset the maxLod's to the be appropriate for the dimensions of the images being used.
    for (size_t sai = 0; sai < model->samplers.values.size(); ++sai)
    {
        float maxLod = std::floor(std::log2f(static_cast<float>(maxDimensions[sai])));
        if (vsg_samplers[sai]->maxLod > maxLod)
        {
            vsg_samplers[sai]->maxLod = maxLod;
        }
    }

    // vsg::info("create materials = ", model->materials.values.size());
    vsg_materials.resize(model->materials.values.size());
    for (size_t mi = 0; mi < model->materials.values.size(); ++mi)
    {
        vsg_materials[mi] = createMaterial(model->materials.values[mi]);
    }

    // vsg::info("create meshes = ", model->meshes.values.size());
    // populate vsg_meshes in the createNode method.
    vsg_meshes.resize(model->meshes.values.size());

    if (auto khr_lights = model->extension<KHR_lights_punctual>("KHR_lights_punctual"))
    {
        vsg_lights.resize(khr_lights->lights.values.size());
        for (size_t li = 0; li < khr_lights->lights.values.size(); ++li)
        {
            vsg_lights[li] = createLight(khr_lights->lights.values[li]);
        }
    }

    //vsg::info("create skins = ", model->skins.values.size(), ", model->nodes.values.size() = ", model->nodes.values.size());
    vsg_skins.resize(model->skins.values.size());
    for (size_t si = 0; si < model->skins.values.size(); ++si)
    {
        auto& gltf_skin = model->skins.values[si];

        auto jointSampler = vsg::JointSampler::create();
        jointSampler->jointMatrices = vsg::mat4Array::create(gltf_skin->joints.values.size());
        jointSampler->jointMatrices->properties.dataVariance = vsg::DYNAMIC_DATA;
        jointSampler->offsetMatrices.resize(gltf_skin->joints.values.size());

        auto& offsetMatrices = jointSampler->offsetMatrices;
        auto inverseBindMatrices = vsg_accessors[gltf_skin->inverseBindMatrices.value];
        if (auto floatMatrices = inverseBindMatrices.cast<vsg::mat4Array>())
        {
            for (size_t i = 0; i < gltf_skin->joints.values.size(); ++i)
            {
                offsetMatrices[i] = floatMatrices->at(i);
            }
        }
        else if (auto doubleMatrices = inverseBindMatrices.cast<vsg::dmat4Array>())
        {
            for (size_t i = 0; i < gltf_skin->joints.values.size(); ++i)
            {
                offsetMatrices[i] = doubleMatrices->at(i);
            }
        }

        //vsg::info("skin inverseBindMatrices = ", inverseBindMatrices, ", gltf_skin->joints.values.size() = ", gltf_skin->joints.values.size() );

        vsg_skins[si] = jointSampler;
        assign_name_extras(*gltf_skin, *jointSampler);
    }

    // vsg::info("create nodes = ", model->nodes.values.size());
    vsg_nodes.resize(model->nodes.values.size());
    for (size_t ni = 0; ni < model->nodes.values.size(); ++ni)
    {
        vsg_nodes[ni] = createNode(model->nodes.values[ni], vsg_joints[ni]);
    }

    for (size_t ni = 0; ni < model->nodes.values.size(); ++ni)
    {
        auto& gltf_node = model->nodes.values[ni];

        if (!gltf_node->children.values.empty())
        {

            if (auto vsg_group = vsg_nodes[ni].cast<vsg::Group>())
            {
                for (auto id : gltf_node->children.values)
                {
                    if (auto vsg_child = vsg_nodes[id.value])
                        vsg_group->addChild(vsg_child);
                    else
                        vsg::info("Unassigned vsg_child");
                }
            }
            else if (auto vsg_joint = vsg_nodes[ni].cast<vsg::Joint>())
            {
                for (auto id : gltf_node->children.values)
                {
                    if (auto vsg_child = vsg_nodes[id.value])
                        vsg_joint->addChild(vsg_child);
                    else
                        vsg::info("Unassigned vsg_child");
                }
            }
        }
    }

    //
    for (size_t si = 0; si < model->skins.values.size(); ++si)
    {
        auto& gltf_skin = model->skins.values[si];

        for (size_t i = 0; i < gltf_skin->joints.values.size(); ++i)
        {
            if (auto joint = vsg_nodes[gltf_skin->joints.values[i].value].cast<vsg::Joint>())
            {
                joint->index = i;
            }
        }

        if (gltf_skin->skeleton)
        {
            vsg_skins[si]->subgraph = vsg_nodes[gltf_skin->skeleton.value];
        }
        else if (!gltf_skin->joints.values.empty())
        {
            vsg_skins[si]->subgraph = vsg_nodes[gltf_skin->joints.values[0].value];
        }
    }

    // set up animations
    vsg_animations.resize(model->animations.values.size());
    for (size_t ai = 0; ai < model->animations.values.size(); ++ai)
    {
        vsg_animations[ai] = createAnimation(model->animations.values[ai]);

        // for now just add JointSampler to all animations, do need to check that animation is associted with samplers joints.
        for (auto& jointSampler : vsg_skins)
        {
            vsg_animations[ai]->samplers.push_back(jointSampler);
        }
    }

    // vsg::info("scene = ", model->scene);
    // vsg::info("scenes = ", model->scenes.values.size());

    vsg_scenes.resize(model->scenes.values.size());
    for (size_t sci = 0; sci < model->scenes.values.size(); ++sci)
    {
        vsg_scenes[sci] = createScene(model->scenes.values[sci], requiresRootTransformNode, rootTransform);
    }

    // create root node
    if (vsg_scenes.size() > 1)
    {
        auto vsg_switch = vsg::Switch::create();
        for (size_t sci = 0; sci < model->scenes.values.size(); ++sci)
        {
            auto& vsg_scene = vsg_scenes[sci];
            vsg_switch->addChild(true, vsg_scene);
        }

        vsg_switch->setSingleChildOn(model->scene.value);

        // vsg::info("Created a scenes with a switch");

        return vsg_switch;
    }
    else if (vsg_scenes.size() == 1)
    {
        // vsg::info("Created a single scene");
        return vsg_scenes.front();
    }
    else
    {
        vsg::info("Empty scene");
        return {};
    }
}
