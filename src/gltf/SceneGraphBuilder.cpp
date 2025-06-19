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

#include <vsg/nodes/Group.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/nodes/VertexDraw.h>
#include <vsg/nodes/VertexIndexDraw.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/DepthSorted.h>
#include <vsg/nodes/Switch.h>
#include <vsg/nodes/CullNode.h>
#include <vsg/nodes/InstanceDraw.h>
#include <vsg/nodes/InstanceDrawIndexed.h>
#include <vsg/nodes/Layer.h>
#include <vsg/maths/transform.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsg/utils/ComputeBounds.h>
#include <vsg/state/material.h>

#ifdef vsgXchange_draco
#include "draco/core/decoder_buffer.h"
#include "draco/compression/decode.h"
#endif

using namespace vsgXchange;

gltf::SceneGraphBuilder::SceneGraphBuilder()
{
    attributeLookup = {
        {"POSITION", "vsg_Vertex"},
        {"NORMAL", "vsg_Normal"},
        {"TEXCOORD_0", "vsg_TexCoord0"},
        {"COLOR", "vsg_Color"},
        {"COLOR_0", "vsg_Color"},
        {"TRANSLATION", "vsg_Translation"},
        {"ROTATION", "vsg_Rotation"},
        {"SCALE", "vsg_Scale"}
    };
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
    auto vsg_buffer =  vsg::ubyteArray::create(vsg_buffers[gltf_bufferView->buffer.value],
                                               gltf_bufferView->byteOffset,
                                               gltf_bufferView->byteStride,
                                               gltf_bufferView->byteLength / gltf_bufferView->byteStride);
    return vsg_buffer;
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

    auto bufferView = vsg_bufferViews[gltf_accessor->bufferView.value];

    auto stride = [&](uint32_t s) -> uint32_t
    {
        return std::max(bufferView->properties.stride, s);
    };

    vsg::ref_ptr<vsg::Data> vsg_data;
    switch(gltf_accessor->componentType)
    {
        case(COMPONENT_TYPE_BYTE):
            if      (gltf_accessor->type=="SCALAR") vsg_data = vsg::byteArray::create(bufferView, gltf_accessor->byteOffset, stride(1), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC2")   vsg_data = vsg::bvec2Array::create(bufferView, gltf_accessor->byteOffset, stride(2), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC3")   vsg_data = vsg::bvec3Array::create(bufferView, gltf_accessor->byteOffset, stride(3), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC4")   vsg_data = vsg::bvec4Array::create(bufferView, gltf_accessor->byteOffset, stride(4), gltf_accessor->count);
            else vsg::warn("Unsupported gltf_accessor->componentType = ", gltf_accessor->componentType);
            break;
        case(COMPONENT_TYPE_UNSIGNED_BYTE):
            if      (gltf_accessor->type=="SCALAR") vsg_data = vsg::ubyteArray::create(bufferView, gltf_accessor->byteOffset, stride(1), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC2")   vsg_data = vsg::ubvec2Array::create(bufferView, gltf_accessor->byteOffset, stride(2), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC3")   vsg_data = vsg::ubvec3Array::create(bufferView, gltf_accessor->byteOffset, stride(3), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC4")   vsg_data = vsg::ubvec4Array::create(bufferView, gltf_accessor->byteOffset, stride(4), gltf_accessor->count);
            else vsg::warn("Unsupported gltf_accessor->componentType = ", gltf_accessor->componentType);
            break;
        case(COMPONENT_TYPE_SHORT):
            if      (gltf_accessor->type=="SCALAR") vsg_data = vsg::shortArray::create(bufferView, gltf_accessor->byteOffset, stride(2), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC2")   vsg_data = vsg::svec2Array::create(bufferView, gltf_accessor->byteOffset, stride(4), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC3")   vsg_data = vsg::svec3Array::create(bufferView, gltf_accessor->byteOffset, stride(6), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC4")   vsg_data = vsg::svec4Array::create(bufferView, gltf_accessor->byteOffset, stride(8), gltf_accessor->count);
            else vsg::warn("Unsupported gltf_accessor->componentType = ", gltf_accessor->componentType);
            break;
        case(COMPONENT_TYPE_UNSIGNED_SHORT):
            if      (gltf_accessor->type=="SCALAR") vsg_data = vsg::ushortArray::create(bufferView, gltf_accessor->byteOffset, stride(2), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC2")   vsg_data = vsg::usvec2Array::create(bufferView, gltf_accessor->byteOffset, stride(4), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC3")   vsg_data = vsg::usvec3Array::create(bufferView, gltf_accessor->byteOffset, stride(6), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC4")   vsg_data = vsg::usvec4Array::create(bufferView, gltf_accessor->byteOffset, stride(8), gltf_accessor->count);
            else vsg::warn("Unsupported gltf_accessor->componentType = ", gltf_accessor->componentType);
            break;
        case(COMPONENT_TYPE_INT):
            if      (gltf_accessor->type=="SCALAR") vsg_data = vsg::intArray::create(bufferView, gltf_accessor->byteOffset, stride(4), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC2")   vsg_data = vsg::ivec2Array::create(bufferView, gltf_accessor->byteOffset, stride(8), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC3")   vsg_data = vsg::ivec3Array::create(bufferView, gltf_accessor->byteOffset, stride(12), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC4")   vsg_data = vsg::ivec4Array::create(bufferView, gltf_accessor->byteOffset, stride(16), gltf_accessor->count);
            else vsg::warn("Unsupported gltf_accessor->componentType = ", gltf_accessor->componentType);
            break;
        case(COMPONENT_TYPE_UNSIGNED_INT):
            if      (gltf_accessor->type=="SCALAR") vsg_data = vsg::uintArray::create(bufferView, gltf_accessor->byteOffset, stride(4), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC2")   vsg_data = vsg::uivec2Array::create(bufferView, gltf_accessor->byteOffset, stride(8), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC3")   vsg_data = vsg::uivec3Array::create(bufferView, gltf_accessor->byteOffset, stride(12), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC4")   vsg_data = vsg::uivec4Array::create(bufferView, gltf_accessor->byteOffset, stride(16), gltf_accessor->count);
            else vsg::warn("Unsupported gltf_accessor->componentType = ", gltf_accessor->componentType);
            break;
        case(COMPONENT_TYPE_FLOAT):
            if      (gltf_accessor->type=="SCALAR") vsg_data = vsg::floatArray::create(bufferView, gltf_accessor->byteOffset, stride(4), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC2")   vsg_data = vsg::vec2Array::create(bufferView, gltf_accessor->byteOffset, stride(8), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC3")   vsg_data = vsg::vec3Array::create(bufferView, gltf_accessor->byteOffset, stride(12), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC4")   vsg_data = vsg::vec4Array::create(bufferView, gltf_accessor->byteOffset, stride(16), gltf_accessor->count);
            //else if (gltf_accessor->type=="MAT2")   vsg_data = vsg::mat2Array::create(bufferView, gltf_accessor->byteOffset, stride(16), gltf_accessor->count);
            //else if (gltf_accessor->type=="MAT3")   vsg_data = vsg::mat3Array::create(bufferView, gltf_accessor->byteOffset, stride(36), gltf_accessor->count);
            else if (gltf_accessor->type=="MAT4")   vsg_data = vsg::mat4Array::create(bufferView, gltf_accessor->byteOffset, stride(64), gltf_accessor->count);
            else vsg::warn("Unsupported gltf_accessor->componentType = ", gltf_accessor->componentType);
            break;
        case(COMPONENT_TYPE_DOUBLE):
            if      (gltf_accessor->type=="SCALAR") vsg_data = vsg::doubleArray::create(bufferView, gltf_accessor->byteOffset, stride(8), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC2")   vsg_data = vsg::dvec2Array::create(bufferView, gltf_accessor->byteOffset, stride(16), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC3")   vsg_data = vsg::dvec3Array::create(bufferView, gltf_accessor->byteOffset, stride(24), gltf_accessor->count);
            else if (gltf_accessor->type=="VEC4")   vsg_data = vsg::dvec4Array::create(bufferView, gltf_accessor->byteOffset, stride(32), gltf_accessor->count);
            //else if (gltf_accessor->type=="MAT2")   vsg_data = vsg::dmat2Array::create(bufferView, gltf_accessor->byteOffset, stride(32), gltf_accessor->count);
            //else if (gltf_accessor->type=="MAT3")   vsg_data = vsg::dmat3Array::create(bufferView, gltf_accessor->byteOffset, stride(72), gltf_accessor->count);
            else if (gltf_accessor->type=="MAT4")   vsg_data = vsg::dmat4Array::create(bufferView, gltf_accessor->byteOffset, stride(128), gltf_accessor->count);
            else vsg::warn("Unsupported gltf_accessor->componentType = ", gltf_accessor->componentType);
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
        double halfWidth = orthographic->xmag; // TODO: figure how to map to GLU/VSG style orthographic
        double halfHeight = orthographic->ymag; // TODO: figure how to mapto GLU/VSG style orthographic
        vsg_camera->projectionMatrix = vsg::Orthographic::create(-halfWidth, halfWidth, -halfHeight, halfHeight, orthographic->znear, orthographic->zfar);
    }

    vsg::info("Assigned camera");

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
    switch(gltf_sampler->minFilter)
    {
        case(9728) : // NEAREST
            vsg_sampler->minFilter = VK_FILTER_NEAREST;
            vsg_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            vsg_sampler->minLod = 0.0f;
            vsg_sampler->maxLod = 0.25f;
            break;
        case(9729) : // LINEAR
            vsg_sampler->minFilter = VK_FILTER_LINEAR;
            vsg_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            vsg_sampler->minLod = 0.0f;
            vsg_sampler->maxLod = 0.25f;
            break;
        case(9984) : // NEAREST_MIPMAP_NEAREST
            vsg_sampler->minFilter = VK_FILTER_NEAREST;
            vsg_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        case(9985) : // LINEAR_MIPMAP_NEAREST
            vsg_sampler->minFilter = VK_FILTER_LINEAR;
            vsg_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        case(9986) : // NEAREST_MIPMAP_LINEAR
            vsg_sampler->minFilter = VK_FILTER_NEAREST;
            vsg_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
        case(9987) : // LINEAR_MIPMAP_LINEAR
            vsg_sampler->minFilter = VK_FILTER_LINEAR;
            vsg_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
        default:
            vsg::debug("gltf_sampler->minFilter value of ", gltf_sampler->minFilter, " not set, using linear mipmap linear.");
            vsg_sampler->minFilter = VK_FILTER_LINEAR;
            vsg_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
    }

    switch(gltf_sampler->magFilter)
    {
        case(9728) :
            vsg_sampler->magFilter = VK_FILTER_NEAREST;
            break;
        case(9729) :
            vsg_sampler->magFilter = VK_FILTER_LINEAR;
            break;
        default:
            vsg::debug("gltf_sampler->magFilter value of ", gltf_sampler->magFilter, " not set, using default of linear.");
            vsg_sampler->magFilter = VK_FILTER_LINEAR;
            break;
    }


    auto addressMode = [](uint32_t wrap) -> VkSamplerAddressMode
    {
        switch(wrap)
        {
            case(33071) : // CLAMP_TO_EDGE
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case(33648) : // MIRRORED_REPEAT
                return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            case(10497) : // REPEAT
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

    if (gltf_material->pbrMetallicRoughness.baseColorFactor.values.size()==4)
    {
        auto& baseColorFactor = gltf_material->pbrMetallicRoughness.baseColorFactor.values;
        pbrMaterial.baseColorFactor.set(baseColorFactor[0], baseColorFactor[1], baseColorFactor[2], baseColorFactor[3]);
        // vsg::info("Assigned baseColorFacator ", pbrMaterial.baseColorFactor);
    }

    if (gltf_material->pbrMetallicRoughness.baseColorTexture.index)
    {
        auto& texture = vsg_textures[gltf_material->pbrMetallicRoughness.baseColorTexture.index.value];
        if (texture.image)
        {
            // vsg::info("Assigned diffuseMap ", texture.image, ", ", texture.sampler);
            vsg_material->assignTexture("diffuseMap", texture.image, texture.sampler);

            auto& textureInfo = gltf_material->pbrMetallicRoughness.baseColorTexture;
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

    pbrMaterial.metallicFactor = gltf_material->pbrMetallicRoughness.metallicFactor;
    pbrMaterial.roughnessFactor = gltf_material->pbrMetallicRoughness.roughnessFactor;

    if (gltf_material->pbrMetallicRoughness.metallicRoughnessTexture.index)
    {
        auto& texture = vsg_textures[gltf_material->pbrMetallicRoughness.metallicRoughnessTexture.index.value];
        if (texture.image)
        {
            // vsg::info("Assigned metallicRoughnessTexture ", texture.image, ", ", texture.sampler);
            vsg_material->assignTexture("mrMap", texture.image, texture.sampler);
        }
        else
        {
            vsg::warn("Could not assign metallicRoughnessTexture ", gltf_material->pbrMetallicRoughness.metallicRoughnessTexture.index);
        }
    }

    // TODO : pbrMaterial.diffuseFactor? No glTF mapping?

    if (gltf_material->normalTexture.index)
    {
        // TODO: gltf_material->normalTexture.scale

        auto& texture = vsg_textures[gltf_material->normalTexture.index.value];
        if (texture.image)
        {
            // vsg::info("Assigned normalTexture ", texture.image, ", ", texture.sampler, ", scale = ", gltf_material->normalTexture.scale);
            vsg_material->assignTexture("normalMap", texture.image, texture.sampler);
        }
        else
        {
            vsg::warn("Could not assign normalTexture ", gltf_material->normalTexture.index);
        }
    }

    if (gltf_material->occlusionTexture.index)
    {
        // TODO: gltf_material->occlusionTexture.strength

        auto& texture = vsg_textures[gltf_material->occlusionTexture.index.value];
        if (texture.image)
        {
            // vsg::info("Assigned occlusionTexture ", texture.image, ", ", texture.sampler, ", strength = ", gltf_material->occlusionTexture.strength);
            vsg_material->assignTexture("aoMap", texture.image, texture.sampler);
        }
        else
        {
            vsg::warn("Could not assign occlusionTexture ", gltf_material->occlusionTexture.index);
        }
    }

    if (gltf_material->emissiveTexture.index)
    {
        auto& texture = vsg_textures[gltf_material->emissiveTexture.index.value];
        if (texture.image)
        {
            // vsg::info("Assigned emissiveTexture ", texture.image, ", ", texture.sampler);
            vsg_material->assignTexture("emissiveMap", texture.image, texture.sampler);
        }
        else
        {
            vsg::warn("Could not assign emissiveTexture ", gltf_material->emissiveTexture.index);
        }
    }

    if (gltf_material->emissiveFactor.values.size()>=3)
    {
        pbrMaterial.emissiveFactor.set(gltf_material->emissiveFactor.values[0], gltf_material->emissiveFactor.values[1], gltf_material->emissiveFactor.values[2], 1.0);
        // vsg::info("Set pbrMaterial.emissiveFactor = ", pbrMaterial.emissiveFactor);
    }


    if (gltf_material->alphaMode=="BLEND")
    {
        vsg_material->blending = true;
    }
    else if (gltf_material->alphaMode=="MASK")
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

    if (gltf_material->pbrMetallicRoughness.baseColorFactor.values.size()==4)
    {
        auto& baseColorFactor = gltf_material->pbrMetallicRoughness.baseColorFactor.values;
        phongMaterial.diffuse.set(baseColorFactor[0], baseColorFactor[1], baseColorFactor[2], baseColorFactor[3]);
        // vsg::info("Assigned phongMaterial.diffuse ", pbrMaterial.baseColorFactor);
    }

    if (gltf_material->pbrMetallicRoughness.baseColorTexture.index)
    {
        auto& texture = vsg_textures[gltf_material->pbrMetallicRoughness.baseColorTexture.index.value];
        if (texture.image)
        {
            // vsg::info("Assigned diffuseMap ", texture.image, ", ", texture.sampler);
            vsg_material->assignTexture("diffuseMap", texture.image, texture.sampler);

            auto& textureInfo = gltf_material->pbrMetallicRoughness.baseColorTexture;
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

    if (gltf_material->alphaMode=="BLEND")
    {
        vsg_material->blending = true;
    }
    else if (gltf_material->alphaMode=="MASK")
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
    if (auto materials_unlit = gltf_material->extension<KHR_materials_unlit>("KHR_materials_unlit")) return createUnlitMaterial(gltf_material);
    else return createPbrMaterial(gltf_material);
}

vsg::ref_ptr<vsg::Node> gltf::SceneGraphBuilder::createMesh(vsg::ref_ptr<gltf::Mesh> gltf_mesh, vsg::ref_ptr<gltf::Attributes> instancedAttributes)
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
            VK_PRIMITIVE_TOPOLOGY_POINT_LIST, // 0, POINTS
            VK_PRIMITIVE_TOPOLOGY_LINE_LIST, // 1, LINES
            VK_PRIMITIVE_TOPOLOGY_LINE_LIST, // 2, LINE_LOOP, need special handling
            VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, // 3, LINE_STRIP
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // 4, TRIANGLES
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, // 5, TRIANGLE_STRIP
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN // 6, TRIANGLE_FAN
    };
#if 0
    vsg::info("mesh = {");
    vsg::info("    primitives = ", gltf_mesh->primitives.values.size());
    vsg::info("    weight = ", gltf_mesh->weights.values.size());
#endif

    std::vector<vsg::ref_ptr<vsg::Node>> nodes;

    for(auto& primitive : gltf_mesh->primitives.values)
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

        auto assignArray = [&](Attributes& attrib, VkVertexInputRate vertexInputRate, const std::string& attribute_name) -> bool
        {
            auto array_itr = attrib.values.find(attribute_name);
            if (array_itr == attrib.values.end()) return false;

            auto name_itr = attributeLookup.find(attribute_name);
            if (name_itr == attributeLookup.end()) return false;

            vsg::ref_ptr<vsg::Data> array = vsg_accessors[array_itr->second.value];
            if (attribute_name=="ROTATION")
            {
                if (auto vec4Rotations = array.cast<vsg::vec4Array>())
                {
                    auto quatArray = vsg::quatArray::create(array, 0, 12, vec4Rotations->size());
                    array = quatArray;
                }
            }
            else if (attribute_name=="TEXCOORD_0")
            {
                if (auto texture_transform = vsg_material->getObject<KHR_texture_transform>("KHR_texture_transform"))
                {
                    vsg::vec2 offset(0.0f, 0.0f);
                    vsg::vec2 scale(1.0f, 1.0f);
                    float rotation = texture_transform->rotation;
                    if (texture_transform->offset.values.size()>=2) offset.set(texture_transform->offset.values[0],  texture_transform->offset.values[1]);
                    if (texture_transform->scale.values.size()>=2) scale.set(texture_transform->scale.values[0],  texture_transform->scale.values[1]);

                    if (auto texCoords = array.cast<vsg::vec2Array>())
                    {
                        float sin_rotation = std::sin(rotation);
                        float cos_rotation = std::cos(rotation);
                        auto transformedTexCoords = vsg::vec2Array::create(texCoords->size());
                        auto dest_itr = transformedTexCoords->begin();
                        for(auto& tc : *texCoords)
                        {
                            auto& dest_tc = *(dest_itr++);
                            dest_tc.x = offset.x + (tc.x * scale.x) * cos_rotation + (tc.y * scale.y) * sin_rotation;
                            dest_tc.y = offset.y + (tc.y * scale.y) * cos_rotation - (tc.x * scale.x) * sin_rotation;
                        }
                        array = transformedTexCoords;
                    }
                }
            }

            config->assignArray(vertexArrays, name_itr->second, vertexInputRate, array);
            return true;
        };

        assignArray(primitive->attributes, VK_VERTEX_INPUT_RATE_VERTEX, "POSITION");

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

        uint32_t vertexCount = vertexArrays.front()->valueCount();
        uint32_t instanceCount = 1;
        if (instancedAttributes)
        {
            for(auto& [name, id] : instancedAttributes->values)
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

        if (instancedAttributes)
        {
            assignArray(*instancedAttributes, VK_VERTEX_INPUT_RATE_INSTANCE, "TRANSLATION");
            assignArray(*instancedAttributes, VK_VERTEX_INPUT_RATE_INSTANCE, "ROTATION");
            assignArray(*instancedAttributes, VK_VERTEX_INPUT_RATE_INSTANCE, "SCALE");
        }

        vsg::ref_ptr<vsg::Node> draw;

        if (!instancedAttributes && instanceNodeHint != vsg::Options::INSTANCE_NONE)
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
                if (auto ubyte_indices = indices.cast<vsg::ubyteArray>())
                {
                    // need to promote ubyte indices to ushort as Vulkan requires an extension to be enabled for ubyte indices.
                    auto ushort_indices = vsg::ushortArray::create(ubyte_indices->size());
                    auto itr = ushort_indices->begin();
                    for(auto value : *ubyte_indices)
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
            if (auto ubyte_indices = indices.cast<vsg::ubyteArray>())
            {
                // need to promote ubyte indices to ushort as Vulkan requires an extension to be enabled for ubyte indices.
                auto ushort_indices = vsg::ushortArray::create(ubyte_indices->size());
                auto itr = ushort_indices->begin();
                for(auto value : *ubyte_indices)
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
            if (instancedAttributes || instanceNodeHint != vsg::Options::INSTANCE_NONE)
            {
                auto layer = vsg::Layer::create();
                layer->binNumber = 10;
                layer->child = stateGroup;

                nodes.push_back(layer);
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
    if (nodes.size()==1)
    {
        // vsg::info("Mesh with single primtiive");
        vsg_mesh = nodes.front();
    }
    else
    {
        // vsg::info("Mesh with multiple primtiives - could possible use vsg::Geomterty.");
        auto group = vsg::Group::create();
        for(auto node : nodes)
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
    if (node.matrix.values.size()==16)
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

        if (t.size()>=3) vsg_t.set(t[0], t[1], t[2]);
        if (r.size()>=4) vsg_r.set(r[0], r[1], r[2], r[3]);
        if (s.size()>=3) vsg_s.set(s[0], s[1], s[2]);

        matrix = vsg::translate(vsg_t) * vsg::rotate(vsg_r) * vsg::scale(vsg_s);
        return true;
    }
    else
    {
        return false;
    }
}

vsg::ref_ptr<vsg::Node> gltf::SceneGraphBuilder::createNode(vsg::ref_ptr<gltf::Node> gltf_node)
{
    vsg::ref_ptr<vsg::Node> vsg_node;

    vsg::ref_ptr<vsg::Node> vsg_mesh;
    if (gltf_node->mesh)
    {
        vsg_mesh = vsg_meshes[gltf_node->mesh.value];
        auto gltf_mesh = model->meshes.values[gltf_node->mesh.value];
        if (auto mesh_gpu_instancing = gltf_node->extension<EXT_mesh_gpu_instancing>("EXT_mesh_gpu_instancing"))
        {
            vsg_mesh = createMesh(gltf_mesh, mesh_gpu_instancing->attributes);
        }
        else
        {
            if (!vsg_meshes[gltf_node->mesh.value])
            {
                vsg_meshes[gltf_node->mesh.value] = createMesh(gltf_mesh);
            }

            vsg_mesh = vsg_meshes[gltf_node->mesh.value];
        }
    }

    bool isTransform = !(gltf_node->matrix.values.empty()) ||
                        !(gltf_node->rotation.values.empty()) ||
                        !(gltf_node->scale.values.empty()) ||
                        !(gltf_node->translation.values.empty());

    size_t numChildren = gltf_node->children.values.size();
    if (gltf_node->camera) ++numChildren;
    if (gltf_node->skin) ++numChildren;
    if (vsg_mesh) ++numChildren;

    if (isTransform)
    {
        auto transform = vsg::MatrixTransform::create();
        if (gltf_node->camera) transform->addChild(vsg_cameras[gltf_node->camera.value]);
        else if (gltf_node->skin) transform->addChild(vsg_skins[gltf_node->skin.value]);
        else if (vsg_mesh) transform->addChild(vsg_mesh);

        if (gltf_node->matrix.values.size()==16)
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

            if (t.size()>=3) vsg_t.set(t[0], t[1], t[2]);
            if (r.size()>=4) vsg_r.set(r[0], r[1], r[2], r[3]);
            if (s.size()>=3) vsg_s.set(s[0], s[1], s[2]);

            transform->matrix = vsg::translate(vsg_t) * vsg::rotate(vsg_r) * vsg::scale(vsg_s);
        }

        vsg_node = transform;
    }
    else if (numChildren > 1)
    {
        auto group = vsg::Group::create();

        if (gltf_node->camera) group->addChild(vsg_cameras[gltf_node->camera.value]);
        else if (gltf_node->skin) group->addChild(vsg_skins[gltf_node->skin.value]);
        else if (vsg_mesh) group->addChild(vsg_mesh);

        vsg_node = group;
    }
    else
    {
        if (gltf_node->camera) vsg_node = vsg_cameras[gltf_node->camera.value];
        else if (gltf_node->skin) vsg_node = vsg_skins[gltf_node->skin.value];
        else if (vsg_mesh) vsg_node = vsg_mesh;
        else vsg_node = vsg::Group::create(); // TODO: single child so should this just point to the child?
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
        for(auto primitive : mesh->primitives.values)
        {
            if (auto position_itr = primitive->attributes.values.find("POSITION"); position_itr != primitive->attributes.values.end())
            {
                auto data = vsg_accessors[position_itr->second.value];
                auto vertices = data.cast<vsg::vec3Array>();

                for(auto& v : *vertices)
                {
                    v = accumulatedTransform * vsg::dvec3(v);
                }
            }
            if (auto normal_itr = primitive->attributes.values.find("NORMAL"); normal_itr != primitive->attributes.values.end())
            {
                auto data = vsg_accessors[normal_itr->second.value];
                auto normals = data.cast<vsg::vec3Array>();

                for(auto& n : *normals)
                {
                    n = vsg::dvec3(n) * inverse_accumulatedTransform;
                }
            }
        }
    }

    for(auto& id : node.children.values)
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

    vsg::ref_ptr<vsg::Node> vsg_scene;

    if (requiresRootTransformNode)
    {
        auto mt = vsg::MatrixTransform::create(rootTransform);

        for(auto& id : gltf_scene->nodes.values)
        {
            mt->addChild(vsg_nodes[id.value]);
        }

        vsg_scene = mt;
    }
    else

    if (gltf_scene->nodes.values.size()>1)
    {
         auto group = vsg::Group::create();
        for(auto& id : gltf_scene->nodes.values)
        {
            group->addChild(vsg_nodes[id.value]);
        }
        vsg_scene = group;
    }
    else
    {
        vsg_scene = vsg_nodes[gltf_scene->nodes.values[0].value];
    }

    bool culling = vsg::value<bool>(true, gltf::culling, options) && (instanceNodeHint == vsg::Options::INSTANCE_NONE);
    if (culling)
    {
        if (auto bounds = vsg::visit<vsg::ComputeBounds>(vsg_scene).bounds)
        {
            vsg::dsphere bs((bounds.max + bounds.min) * 0.5, vsg::length(bounds.max - bounds.min) * 0.5);
            auto cullNode = vsg::CullNode::create(bs, vsg_scene);
            vsg_scene = cullNode;
        }
    }

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
    for(draco::PointIndex i(0); i < num_points; ++i)
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
            uint32_t componentSize =  (num_points < 65536) ? 2 : 4;
            uint32_t count =  mesh->num_faces() * 3;

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

            if (componentSize==sizeof(draco::PointIndex))
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
                for(draco::FaceIndex i(0); i < mesh->num_faces(); ++i)
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

        for(auto& [name, id] : draco_attributes)
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

                auto attributeBuffer =  gltf::Buffer::create();
                attributeBuffer->byteLength = attributeBufferView->byteLength;
                attributeBuffer->data = vsg::ubyteArray::create(attributeBuffer->byteLength);
                model->buffers.values.push_back(attributeBuffer);

                auto* ptr = attributeBuffer->data->dataPointer();

                // TODO get the attributes from the draco mesh.
                switch(accessor->componentType)
                {
                    case(COMPONENT_TYPE_BYTE):
                        CopyDracoAttributes<int8_t>(draco_attribute, ptr, num_points);
                        break;
                    case(COMPONENT_TYPE_UNSIGNED_BYTE):
                        CopyDracoAttributes<uint8_t>(draco_attribute, ptr, num_points);
                        break;
                    case(COMPONENT_TYPE_SHORT):
                        CopyDracoAttributes<int16_t>(draco_attribute, ptr, num_points);
                        break;
                    case(COMPONENT_TYPE_UNSIGNED_SHORT):
                        CopyDracoAttributes<uint16_t>(draco_attribute, ptr, num_points);
                        break;
                    case(COMPONENT_TYPE_INT):
                        CopyDracoAttributes<int32_t>(draco_attribute, ptr, num_points);
                        break;
                    case(COMPONENT_TYPE_UNSIGNED_INT):
                        CopyDracoAttributes<uint32_t>(draco_attribute, ptr, num_points);
                        break;
                    case(COMPONENT_TYPE_FLOAT):
                        CopyDracoAttributes<float>(draco_attribute, ptr, num_points);
                        break;
                    case(COMPONENT_TYPE_DOUBLE):
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

    for(size_t mi=0; mi<model->meshes.values.size(); ++mi)
    {
        auto mesh = model->meshes.values[mi];
        for(auto primitive : mesh->primitives.values)
        {
            if (!decodePrimitiveIfRequired(primitive))
            {
                vsg::info("Reqires draco decompression but no support available.");
                return {};
            }
        }
    }

    vsg_buffers.resize(model->buffers.values.size());
    for(size_t bi = 0; bi<model->buffers.values.size(); ++bi)
    {
        vsg_buffers[bi] = createBuffer(model->buffers.values[bi]);
    }

    vsg_bufferViews.resize(model->bufferViews.values.size());
    for(size_t bvi = 0; bvi<model->bufferViews.values.size(); ++bvi)
    {
        vsg_bufferViews[bvi] = createBufferView(model->bufferViews.values[bvi]);
    }

    vsg_accessors.resize(model->accessors.values.size());
    for(size_t ai = 0; ai<model->accessors.values.size(); ++ai)
    {
        vsg_accessors[ai] = createAccessor(model->accessors.values[ai]);
    }

    if (instanceNodeHint != vsg::Options::INSTANCE_NONE)
    {
        requiresRootTransformNode = false;
        //vsg::info("Need to flatten all transforms");

        for(size_t sci = 0; sci < model->scenes.values.size(); ++sci)
        {
            auto gltf_scene = model->scenes.values[sci];
            for(auto& id : gltf_scene->nodes.values)
            {
                flattenTransforms(*(model->nodes.values[id.value]), rootTransform);
            }
        }
    }

    // vsg::info("create cameras = ", model->cameras.values.size());
    vsg_cameras.resize(model->cameras.values.size());
    for(size_t ci=0; ci<model->cameras.values.size(); ++ci)
    {
        vsg_cameras[ci] = createCamera(model->cameras.values[ci]);
    }

    // vsg::info("create skins = ", model->skins.values.size());
    vsg_skins.resize(model->skins.values.size());
    for(size_t si=0; si<model->skins.values.size(); ++si)
    {
        auto& gltf_skin = model->skins.values[si];

        auto& vsg_skin = vsg_skins[si];
        vsg_skin = vsg::Node::create();

        assign_name_extras(*gltf_skin, *vsg_skin);
    }

    // vsg::info("create samplers = ", model->samplers.values.size());
    vsg_samplers.resize(model->samplers.values.size());
    std::vector<uint32_t> maxDimensions(model->samplers.values.size(), 0);
    for(size_t sai=0; sai<model->samplers.values.size(); ++sai)
    {
        vsg_samplers[sai] = createSampler(model->samplers.values[sai]);
    }

    // vsg::info("create images = ", model->images.values.size());
    vsg_images.resize(model->images.values.size());
    for(size_t ii=0; ii<model->images.values.size(); ++ii)
    {
         if (model->images.values[ii]) vsg_images[ii] = createImage(model->images.values[ii]);
    }


    // vsg::info("create textures = ", model->textures.values.size());
    vsg_textures.resize(model->textures.values.size());
    for(size_t ti=0; ti<model->textures.values.size(); ++ti)
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
    for(size_t sai=0; sai<model->samplers.values.size(); ++sai)
    {
        float maxLod = std::floor(std::log2f(static_cast<float>(maxDimensions[sai])));
        if (vsg_samplers[sai]->maxLod > maxLod)
        {
            vsg_samplers[sai]->maxLod = maxLod;
        }

    }

    // vsg::info("create materials = ", model->materials.values.size());
    vsg_materials.resize(model->materials.values.size());
    for(size_t mi=0; mi<model->materials.values.size(); ++mi)
    {
        vsg_materials[mi] = createMaterial(model->materials.values[mi]);
    }

    // vsg::info("create meshes = ", model->meshes.values.size());
    // populate vsg_meshes in the createNode method.
    vsg_meshes.resize(model->meshes.values.size());

    // vsg::info("create nodes = ", model->nodes.values.size());
    vsg_nodes.resize(model->nodes.values.size());
    for(size_t ni=0; ni<model->nodes.values.size(); ++ni)
    {
        vsg_nodes[ni] = createNode(model->nodes.values[ni]);
    }

    for(size_t ni=0; ni<model->nodes.values.size(); ++ni)
    {
        auto& gltf_node = model->nodes.values[ni];

        if (!gltf_node->children.values.empty())
        {
            auto vsg_group = vsg_nodes[ni].cast<vsg::Group>();
            for(auto id : gltf_node->children.values)
            {
                auto vsg_child = vsg_nodes[id.value];
                if (vsg_child) vsg_group->addChild(vsg_child);
                else vsg::info("Unassigned vsg_child");
            }
        }
    }

    // vsg::info("scene = ", model->scene);
    // vsg::info("scenes = ", model->scenes.values.size());

    vsg_scenes.resize(model->scenes.values.size());
    for(size_t sci = 0; sci < model->scenes.values.size(); ++sci)
    {
        vsg_scenes[sci] = createScene(model->scenes.values[sci], requiresRootTransformNode, rootTransform);
    }

    // create root node
    if (vsg_scenes.size() > 1)
    {
        auto vsg_switch = vsg::Switch::create();
        for(size_t sci = 0; sci < model->scenes.values.size(); ++sci)
        {
            auto& vsg_scene = vsg_scenes[sci];
            vsg_switch->addChild(true, vsg_scene);
        }

        vsg_switch->setSingleChildOn(model->scene.value);

        // vsg::info("Created a scenes with a switch");

        return vsg_switch;
    }
    else if (vsg_scenes.size()==1)
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

