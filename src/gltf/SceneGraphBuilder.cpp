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
#include <vsg/nodes/VertexIndexDraw.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/DepthSorted.h>
#include <vsg/nodes/Switch.h>
#include <vsg/nodes/CullNode.h>
#include <vsg/maths/transform.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsg/utils/ComputeBounds.h>
#include <vsg/state/material.h>

using namespace vsgXchange;

gltf::SceneGraphBuilder::SceneGraphBuilder()
{
    attributeLookup = {
        {"POSITION", "vsg_Vertex"},
        {"NORMAL", "vsg_Normal"},
        {"TEXCOORD_0", "vsg_TexCoord0"},
        {"COLOR", "vsg_Color"}
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

    // TODO: deciode whether we need to do anything with the BufferView.target
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

    vsg::ref_ptr<vsg::Data> vsg_data;
    switch(gltf_accessor->componentType)
    {
        case(5120): // BYTE
            if      (gltf_accessor->type=="SCALAR") vsg_data = vsg::byteArray::create(bufferView, gltf_accessor->byteOffset, 1, gltf_accessor->count);
            else if (gltf_accessor->type=="VEC2")   vsg_data = vsg::bvec2Array::create(bufferView, gltf_accessor->byteOffset, 2, gltf_accessor->count);
            else if (gltf_accessor->type=="VEC3")   vsg_data = vsg::bvec3Array::create(bufferView, gltf_accessor->byteOffset, 3, gltf_accessor->count);
            else if (gltf_accessor->type=="VEC4")   vsg_data = vsg::bvec4Array::create(bufferView, gltf_accessor->byteOffset, 4, gltf_accessor->count);
            else vsg::warn("Unsupported gltf_accessor->componentType = ", gltf_accessor->componentType);
            break;
        case(5121): // UNSIGNED_BYTE
            if      (gltf_accessor->type=="SCALAR") vsg_data = vsg::ubyteArray::create(bufferView, gltf_accessor->byteOffset, 1, gltf_accessor->count);
            else if (gltf_accessor->type=="VEC2")   vsg_data = vsg::ubvec2Array::create(bufferView, gltf_accessor->byteOffset, 2, gltf_accessor->count);
            else if (gltf_accessor->type=="VEC3")   vsg_data = vsg::ubvec3Array::create(bufferView, gltf_accessor->byteOffset, 3, gltf_accessor->count);
            else if (gltf_accessor->type=="VEC4")   vsg_data = vsg::ubvec4Array::create(bufferView, gltf_accessor->byteOffset, 4, gltf_accessor->count);
            else vsg::warn("Unsupported gltf_accessor->componentType = ", gltf_accessor->componentType);
            break;
        case(5122): // SHORT
            if      (gltf_accessor->type=="SCALAR") vsg_data = vsg::shortArray::create(bufferView, gltf_accessor->byteOffset, 2, gltf_accessor->count);
            else if (gltf_accessor->type=="VEC2")   vsg_data = vsg::svec2Array::create(bufferView, gltf_accessor->byteOffset, 3, gltf_accessor->count);
            else if (gltf_accessor->type=="VEC3")   vsg_data = vsg::svec3Array::create(bufferView, gltf_accessor->byteOffset, 6, gltf_accessor->count);
            else if (gltf_accessor->type=="VEC4")   vsg_data = vsg::svec4Array::create(bufferView, gltf_accessor->byteOffset, 8, gltf_accessor->count);
            else vsg::warn("Unsupported gltf_accessor->componentType = ", gltf_accessor->componentType);
            break;
        case(5123): // UNSIGNED_SHORT
            if      (gltf_accessor->type=="SCALAR") vsg_data = vsg::ushortArray::create(bufferView, gltf_accessor->byteOffset, 2, gltf_accessor->count);
            else if (gltf_accessor->type=="VEC2")   vsg_data = vsg::usvec2Array::create(bufferView, gltf_accessor->byteOffset, 4, gltf_accessor->count);
            else if (gltf_accessor->type=="VEC3")   vsg_data = vsg::usvec3Array::create(bufferView, gltf_accessor->byteOffset, 6, gltf_accessor->count);
            else if (gltf_accessor->type=="VEC4")   vsg_data = vsg::usvec4Array::create(bufferView, gltf_accessor->byteOffset, 8, gltf_accessor->count);
            else vsg::warn("Unsupported gltf_accessor->componentType = ", gltf_accessor->componentType);
            break;
        case(5125): // UNSIGNED_INT
            if      (gltf_accessor->type=="SCALAR") vsg_data = vsg::uintArray::create(bufferView, gltf_accessor->byteOffset, 4, gltf_accessor->count);
            else if (gltf_accessor->type=="VEC2")   vsg_data = vsg::uivec2Array::create(bufferView, gltf_accessor->byteOffset, 8, gltf_accessor->count);
            else if (gltf_accessor->type=="VEC3")   vsg_data = vsg::uivec3Array::create(bufferView, gltf_accessor->byteOffset, 12, gltf_accessor->count);
            else if (gltf_accessor->type=="VEC4")   vsg_data = vsg::uivec4Array::create(bufferView, gltf_accessor->byteOffset, 16, gltf_accessor->count);
            else vsg::warn("Unsupported gltf_accessor->componentType = ", gltf_accessor->componentType);
            break;
        case(5126): // FLOAT
            if      (gltf_accessor->type=="SCALAR") vsg_data = vsg::byteArray::create(bufferView, gltf_accessor->byteOffset, 4, gltf_accessor->count);
            else if (gltf_accessor->type=="VEC2")   vsg_data = vsg::vec2Array::create(bufferView, gltf_accessor->byteOffset, 8, gltf_accessor->count);
            else if (gltf_accessor->type=="VEC3")   vsg_data = vsg::vec3Array::create(bufferView, gltf_accessor->byteOffset, 12, gltf_accessor->count);
            else if (gltf_accessor->type=="VEC4")   vsg_data = vsg::vec4Array::create(bufferView, gltf_accessor->byteOffset, 16, gltf_accessor->count);
            //else if (gltf_accessor->type=="MAT2")   vsg_data = vsg::mat2Array::create(bufferView, gltf_accessor->byteOffset, 16, gltf_accessor->count);
            //else if (gltf_accessor->type=="MAT3")   vsg_data = vsg::mat3Array::create(bufferView, gltf_accessor->byteOffset, 36, gltf_accessor->count);
            else if (gltf_accessor->type=="MAT4")   vsg_data = vsg::mat4Array::create(bufferView, gltf_accessor->byteOffset, 64, gltf_accessor->count);
            else vsg::warn("Unsupported gltf_accessor->componentType = ", gltf_accessor->componentType);
            break;
    }
#if 0
    //if (vsg_data->storage())
    {
        vsg::info("clonning vsg_data ", vsg_data);
        vsg_data = vsg::clone(vsg_data);
        vsg::info("clonned vsg_data ", vsg_data);
    }
#endif
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

    switch(gltf_sampler->minFilter)
    {
        case(9728) : // NEAREST
            vsg_sampler->minFilter = VK_FILTER_NEAREST;
            vsg_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        case(9729) : // LINEAR
            vsg_sampler->minFilter = VK_FILTER_LINEAR;
            vsg_sampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
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
            vsg::warn("gltf_sampler->minFilter value of ", gltf_sampler->minFilter, " not supported.");
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
            vsg::warn("gltf_sampler->magFilter value of ", gltf_sampler->magFilter, " not supported.");
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

vsg::ref_ptr<vsg::DescriptorConfigurator> gltf::SceneGraphBuilder::createMaterial(vsg::ref_ptr<gltf::Material> gltf_material)
{
    auto vsg_material = vsg::DescriptorConfigurator::create();

    vsg_material->shaderSet = shaderSet;

    vsg_material->two_sided = gltf_material->doubleSided;
    // TODO: vsg_material->defines.insert("VSG_TWO_SIDED_LIGHTING");

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


    pbrMaterial.alphaMaskCutoff = gltf_material->alphaCutoff;

    // TODO: vsg_material->defines.insert("VSG_ALPHA_TEST");
    // TODO: pbrMaterial.alphaMask = 1.0f;
    // TODO: material.alphaMode string?

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

vsg::ref_ptr<vsg::Node> gltf::SceneGraphBuilder::createMesh(vsg::ref_ptr<gltf::Mesh> gltf_mesh)
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
        auto vsg_material = vsg_materials[primitive->material.value];

        auto config = vsg::GraphicsPipelineConfigurator::create(vsg_material->shaderSet);
        config->descriptorConfigurator = vsg_material;
        // TODO: if (options) config->assignInheritedState(options->inheritedState);

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


        auto vid = vsg::VertexIndexDraw::create();

        assign_extras(*primitive, *vid);

        vsg::DataList vertexArrays;

        auto assignArray = [&](const std::string& attribute_name) -> bool
        {
            auto array_itr = primitive->attributes.values.find(attribute_name);
            if (array_itr == primitive->attributes.values.end()) return false;

            auto name_itr = attributeLookup.find(attribute_name);
            if (name_itr == attributeLookup.end()) return true;

            config->assignArray(vertexArrays, name_itr->second, VK_VERTEX_INPUT_RATE_VERTEX, vsg_accessors[array_itr->second.value]);
            return true;
        };

        assignArray("POSITION");

        assignArray("NORMAL");

        assignArray("TEXCOORD_0");

        if (!assignArray("COLOR_0"))
        {
            auto defaultColor = vsg::vec4Value::create(1.0f, 1.0f, 1.0f, 1.0f);
            config->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, defaultColor);
        }

        vid->assignArrays(vertexArrays);

        if (primitive->indices)
        {
            auto indices = vsg_accessors[primitive->indices.value];
            vid->assignIndices(indices);
            vid->indexCount = static_cast<uint32_t>(indices->valueCount());
        } // TODO: else use VertexDraw?


        vid->instanceCount = 1;

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

        stateGroup->addChild(vid);

        if (vsg_material->blending)
        {
            vsg::ComputeBounds computeBounds;
            vid->accept(computeBounds);
            vsg::dvec3 center = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
            double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.5;

            auto depthSorted = vsg::DepthSorted::create();
            depthSorted->binNumber = 10;
            depthSorted->bound.set(center[0], center[1], center[2], radius);
            depthSorted->child = stateGroup;

            nodes.push_back(depthSorted);
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

vsg::ref_ptr<vsg::Node> gltf::SceneGraphBuilder::createNode(vsg::ref_ptr<gltf::Node> gltf_node)
{
    vsg::ref_ptr<vsg::Node> vsg_node;

    bool isTransform = !(gltf_node->matrix.values.empty()) ||
                        !(gltf_node->rotation.values.empty()) ||
                        !(gltf_node->scale.values.empty()) ||
                        !(gltf_node->translation.values.empty());

    size_t numChildren = gltf_node->children.values.size();
    if (gltf_node->camera) ++numChildren;
    if (gltf_node->skin) ++numChildren;
    if (gltf_node->mesh) ++numChildren;

    if (isTransform)
    {
        auto transform = vsg::MatrixTransform::create();
        if (gltf_node->camera) transform->addChild(vsg_cameras[gltf_node->camera.value]);
        else if (gltf_node->skin) transform->addChild(vsg_skins[gltf_node->skin.value]);
        else if (gltf_node->mesh) transform->addChild(vsg_meshes[gltf_node->mesh.value]);

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
        else if (gltf_node->mesh) group->addChild(vsg_meshes[gltf_node->mesh.value]);

        vsg_node = group;
    }
    else
    {
        if (gltf_node->camera) vsg_node = vsg_cameras[gltf_node->camera.value];
        else if (gltf_node->skin) vsg_node = vsg_skins[gltf_node->skin.value];
        else if (gltf_node->mesh) vsg_node = vsg_meshes[gltf_node->mesh.value];
        else vsg_node = vsg::Group::create(); // TODO: single child so should this just point to the child?
    }

    assign_name_extras(*gltf_node, *vsg_node);

    return vsg_node;
}

vsg::ref_ptr<vsg::Node> gltf::SceneGraphBuilder::createScene(vsg::ref_ptr<gltf::Scene> gltf_scene)
{
    if (gltf_scene->nodes.values.empty())
    {
        vsg::warn("Cannot create scene graph from empty gltf::Scene.");
        return {};
    }

    vsg::CoordinateConvention source_coordinateConvention = vsg::CoordinateConvention::Y_UP;
    vsg::CoordinateConvention destination_coordinateConvention = vsg::CoordinateConvention::Z_UP;
    if (options) destination_coordinateConvention = options->sceneCoordinateConvention;

    vsg::ref_ptr<vsg::Node> vsg_scene;

    vsg::dmat4 matrix;
    if (vsg::transform(source_coordinateConvention, destination_coordinateConvention, matrix))
    {
        auto mt = vsg::MatrixTransform::create(matrix);

        for(auto& id : gltf_scene->nodes.values)
        {
            mt->addChild(vsg_nodes[id.value]);
        }

        vsg_scene = mt;
    }
    else if (gltf_scene->nodes.values.size()>1)
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

    bool culling = vsg::value<bool>(true, gltf::culling, options);
    if (culling)
    {
        auto bounds = vsg::visit<vsg::ComputeBounds>(vsg_scene).bounds;
        vsg::dsphere bs((bounds.max + bounds.min) * 0.5, vsg::length(bounds.max - bounds.min) * 0.5);

        auto cullNode = vsg::CullNode::create(bs, vsg_scene);
        vsg_scene = cullNode;
    }

    // assign meta data
    assign_name_extras(*gltf_scene, *vsg_scene);
    return vsg_scene;
}

vsg::ref_ptr<vsg::Object> gltf::SceneGraphBuilder::createSceneGraph(vsg::ref_ptr<gltf::glTF> root, vsg::ref_ptr<const vsg::Options> options)
{
    if (!root) return {};

    if (options) sharedObjects = options->sharedObjects;
    if (!sharedObjects) sharedObjects = vsg::SharedObjects::create();

    if (!shaderSet)
    {
        shaderSet = vsg::createPhysicsBasedRenderingShaderSet(options);
        if (sharedObjects) sharedObjects->share(shaderSet);
    }

    vsg_buffers.resize(root->buffers.values.size());
    for(size_t bi = 0; bi<root->buffers.values.size(); ++bi)
    {
        vsg_buffers[bi] = createBuffer(root->buffers.values[bi]);
    }

    vsg_bufferViews.resize(root->bufferViews.values.size());
    for(size_t bvi = 0; bvi<root->bufferViews.values.size(); ++bvi)
    {
        vsg_bufferViews[bvi] = createBufferView(root->bufferViews.values[bvi]);
    }

    vsg_accessors.resize(root->accessors.values.size());
    for(size_t ai = 0; ai<root->accessors.values.size(); ++ai)
    {
        vsg_accessors[ai] = createAccessor(root->accessors.values[ai]);
    }

    // vsg::info("create cameras = ", root->cameras.values.size());
    vsg_cameras.resize(root->cameras.values.size());
    for(size_t ci=0; ci<root->cameras.values.size(); ++ci)
    {
        vsg_cameras[ci] = createCamera(root->cameras.values[ci]);
    }

    // vsg::info("create skins = ", root->skins.values.size());
    vsg_skins.resize(root->skins.values.size());
    for(size_t si=0; si<root->skins.values.size(); ++si)
    {
        auto& gltf_skin = root->skins.values[si];
        auto& vsg_skin = vsg_skins[si];
        vsg_skin = vsg::Node::create();

        assign_name_extras(*gltf_skin, *vsg_skin);
    }

    // vsg::info("create samplers = ", root->samplers.values.size());
    vsg_samplers.resize(root->samplers.values.size());
    for(size_t sai=0; sai<root->samplers.values.size(); ++sai)
    {
        vsg_samplers[sai] = createSampler(root->samplers.values[sai]);
    }

    // vsg::info("create images = ", root->images.values.size());
    vsg_images.resize(root->images.values.size());
    for(size_t ii=0; ii<root->images.values.size(); ++ii)
    {
         if (root->images.values[ii]) vsg_images[ii] = createImage(root->images.values[ii]);
    }

    // vsg::info("create textures = ", root->textures.values.size());
    vsg_textures.resize(root->textures.values.size());
    for(size_t ti=0; ti<root->textures.values.size(); ++ti)
    {
        vsg_textures[ti] = createTexture(root->textures.values[ti]);
    }

    // vsg::info("create materials = ", root->materials.values.size());
    vsg_materials.resize(root->materials.values.size());
    for(size_t mi=0; mi<root->materials.values.size(); ++mi)
    {
        vsg_materials[mi] = createMaterial(root->materials.values[mi]);
    }

    // vsg::info("create meshes = ", root->meshes.values.size());
    vsg_meshes.resize(root->meshes.values.size());
    for(size_t mi=0; mi<root->meshes.values.size(); ++mi)
    {
        vsg_meshes[mi] = createMesh(root->meshes.values[mi]);
    }

    // vsg::info("create nodes = ", root->nodes.values.size());
    vsg_nodes.resize(root->nodes.values.size());
    for(size_t ni=0; ni<root->nodes.values.size(); ++ni)
    {
        vsg_nodes[ni] = createNode(root->nodes.values[ni]);
    }

    for(size_t ni=0; ni<root->nodes.values.size(); ++ni)
    {
        auto& gltf_node = root->nodes.values[ni];

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

    // vsg::info("scene = ", root->scene);
    // vsg::info("scenes = ", root->scenes.values.size());

    vsg_scenes.resize(root->scenes.values.size());
    for(size_t sci = 0; sci < root->scenes.values.size(); ++sci)
    {
        vsg_scenes[sci] = createScene(root->scenes.values[sci]);
    }

    // create root node
    if (vsg_scenes.size() > 1)
    {
        auto vsg_switch = vsg::Switch::create();
        for(size_t sci = 0; sci < root->scenes.values.size(); ++sci)
        {
            auto& vsg_scene = vsg_scenes[sci];
            vsg_switch->addChild(true, vsg_scene);
        }

        vsg_switch->setSingleChildOn(root->scene.value);

        // vsg::info("Created a scenes with a switch");

        return vsg_switch;
    }
    else
    {
        // vsg::info("Created a single scene");
        return vsg_scenes.front();
    }
}

