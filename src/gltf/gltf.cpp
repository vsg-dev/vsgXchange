/* <editor-fold desc="MIT License">

Copyright(c) 2025 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgXchange/gltf.h>

#include <vsg/io/Path.h>
#include <vsg/io/mem_stream.h>
#include <vsg/io/read.h>
#include <vsg/io/write.h>
#include <vsg/threading/OperationThreads.h>
#include <vsg/utils/CommandLine.h>

#include <fstream>

using namespace vsgXchange;

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Extensions
//
void gltf::Extensions::report(vsg::LogOutput& output)
{
    output.enter("extensions = {");
    for (auto& [name, ext] : values)
    {
        output("{", name, ", ", ext->className(), " }");
        if (auto draco_extension = ext->cast<KHR_draco_mesh_compression>())
        {
            draco_extension->report(output);
        }
        else if (auto instancing_extension = ext->cast<EXT_mesh_gpu_instancing>())
        {
            instancing_extension->report(output);
        }
    }
    output.leave();
}

void gltf::Extensions::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    std::string str_property(property);
    auto schema = parser.getRefObject<vsg::JSONParser::Schema>(str_property);
    if (schema)
    {
        if (auto extension = vsg::clone(schema))
        {
            parser.read_object(*extension);
            values[str_property] = extension;
            return;
        }
    }

    vsg::info("gltf::Extensions::read_object() ", property, " not supported.");

    auto extensionAsMetaData = JSONtoMetaDataSchema::create();
    parser.read_object(*extensionAsMetaData);
    values[str_property] = extensionAsMetaData;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// ExtensionsExtras
//
void gltf::ExtensionsExtras::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "extensions")
    {
        if (!extensions) extensions = Extensions::create();
        parser.read_object(*extensions);
    }
    else if (property == "extras")
    {
        if (!extras) extras = Extras::create();
        parser.read_object(*extras);
    }
    else
        parser.warning();
};

void gltf::ExtensionsExtras::report(vsg::LogOutput& output)
{
    if (extensions) extensions->report(output);
    if (extras) vsg::info("    extras = ", extras);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// NameExtensionsExtras
//
void gltf::NameExtensionsExtras::read_string(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "name")
        parser.read_string(name);
    else
        parser.warning();
}

void gltf::NameExtensionsExtras::report(vsg::LogOutput& output)
{
    if (!name.empty()) output("name = ", name);
    ExtensionsExtras::report(output);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Accessor
//

void gltf::SparseIndices::report(vsg::LogOutput& output)
{
    output.enter("indices {");
    output("bufferView: ", bufferView);
    output("byteOffset: ", byteOffset);
    output("componentType: ", componentType);
    output.leave();
}

void gltf::SparseIndices::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "bufferView")
        input >> bufferView;
    else if (property == "byteOffset")
        input >> byteOffset;
    else if (property == "componentType")
        input >> componentType;
    else
        parser.warning();
}

void gltf::SparseValues::report(vsg::LogOutput& output)
{
    output.enter("values {");
    output("bufferView: ", bufferView);
    output("byteOffset: ", byteOffset);
    output.leave();
}

void gltf::SparseValues::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "bufferView")
        input >> bufferView;
    else if (property == "byteOffset")
        input >> byteOffset;
    else
        parser.warning();
}

void gltf::Sparse::report(vsg::LogOutput& output)
{
    output("sparse {");
    NameExtensionsExtras::report(output);
    output("count = ", count);
    if (indices) indices->report(output);
    if (values) values->report(output);
    output.leave();
}

void gltf::Sparse::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "count")
        input >> count;
    else
        parser.warning();
}

void gltf::Sparse::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "indices")
    {
        if (!indices) indices = SparseIndices::create();
        parser.read_object(*indices);
    }
    else if (property == "values")
    {
        if (!values) values = SparseValues::create();
        parser.read_object(*values);
    }
    else
        NameExtensionsExtras::read_object(parser, property);
}

void gltf::Accessor::report(vsg::LogOutput& output)
{
    output.enter("Accessor {");
    NameExtensionsExtras::report(output);
    output("bufferView: ", bufferView);
    output("byteOffset: ", byteOffset);
    output("componentType: ", componentType);
    output("normalized: ", normalized);
    output("count: ", count);
    output("type: ", type);
    for (auto& value : min.values) { output("min : ", value); }
    for (auto& value : max.values) { output("max : ", value); }
    if (sparse) sparse->report(output);
    output.leave();
}

void gltf::Accessor::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "min")
        parser.read_array(min);
    else if (property == "max")
        parser.read_array(max);
    else
        parser.warning();
}

void gltf::Accessor::read_string(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "type")
        parser.read_string(type);
    else
        NameExtensionsExtras::read_string(parser, property);
}

void gltf::Accessor::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "bufferView")
        input >> bufferView;
    else if (property == "byteOffset")
        input >> byteOffset;
    else if (property == "componentType")
        input >> componentType;
    else if (property == "count")
        input >> count;
    else
        parser.warning();
}

void gltf::Accessor::read_bool(vsg::JSONParser& parser, const std::string_view& property, bool value)
{
    if (property == "normalized")
        normalized = value;
    else
        parser.warning();
}

void gltf::Accessor::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "sparse")
    {
        if (!sparse) sparse = Sparse::create();
        parser.read_object(*sparse);
    }
    else
        NameExtensionsExtras::read_object(parser, property);
}

gltf::DataProperties gltf::Accessor::getDataProperties() const
{
    gltf::DataProperties properties;
    properties.componentType = componentType;
    switch (componentType)
    {
    case (COMPONENT_TYPE_BYTE):
    case (COMPONENT_TYPE_UNSIGNED_BYTE): properties.componentSize = 1; break;
    case (COMPONENT_TYPE_SHORT):
    case (COMPONENT_TYPE_UNSIGNED_SHORT): properties.componentSize = 2; break;
    case (COMPONENT_TYPE_INT):
    case (COMPONENT_TYPE_UNSIGNED_INT): properties.componentSize = 4; break;
    case (COMPONENT_TYPE_FLOAT): properties.componentSize = 4; break;
    case (COMPONENT_TYPE_DOUBLE): properties.componentSize = 4; break;
    }

    if (type == "SCALAR")
        properties.componentCount = 1;
    else if (type == "VEC2")
        properties.componentCount = 2;
    else if (type == "VEC3")
        properties.componentCount = 3;
    else if (type == "VEC4")
        properties.componentCount = 4;
    else if (type == "MAT2")
        properties.componentCount = 4;
    else if (type == "MAT3")
        properties.componentCount = 9;
    else if (type == "MAT4")
        properties.componentCount = 16;

    return properties;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// asset_schema
//
void gltf::Asset::report(vsg::LogOutput& output)
{
    output.enter("Asset = {");
    ExtensionsExtras::report(output);
    output("copyright = ", copyright, ", generator = ", generator, ", version = ", version, ", minVersion = ", minVersion, " } ", this);
    output.leave("} ", this);
}

void gltf::Asset::read_string(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "copyright") { parser.read_string(copyright); }
    else if (property == "generator") { parser.read_string(generator); }
    else if (property == "version") { parser.read_string(version); }
    else if (property == "minVersion") { parser.read_string(minVersion); }
    else
        parser.warning();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BufferView
//
void gltf::BufferView::report(vsg::LogOutput& output)
{
    output.enter("BufferView { ");
    NameExtensionsExtras::report(output);
    output("buffer: ", buffer);
    output("byteOffset: ", byteOffset);
    output("byteLength: ", byteLength);
    output("byteStride: ", byteStride);
    output("target: ", target);
    output.leave();
}

void gltf::BufferView::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "buffer")
        input >> buffer;
    else if (property == "byteOffset")
        input >> byteOffset;
    else if (property == "byteLength")
        input >> byteLength;
    else if (property == "byteStride")
        input >> byteStride;
    else if (property == "target")
        input >> target;
    else
        parser.warning();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Buffer
//
void gltf::Buffer::report(vsg::LogOutput& output)
{
    output.enter("Buffer { ");
    NameExtensionsExtras::report(output);
    if (uri.size() < 128)
        output("uri: ", uri, " data: ", data);
    else
        output("uri: first 128 bytes [ ", std::string_view(uri.data(), 128), " ] data: ", data);
    output("byteLength: ", byteLength);
    output.leave();
}

void gltf::Buffer::read_string(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "uri") { parser.read_string_view(uri); }
    else
        NameExtensionsExtras::read_string(parser, property);
}

void gltf::Buffer::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "byteLength")
        input >> byteLength;
    else
        parser.warning();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Image
//
void gltf::Image::report(vsg::LogOutput& output)
{
    output.enter("Image { ");
    NameExtensionsExtras::report(output);
    if (uri.size() < 128)
        output("uri: ", uri, " data: ", data);
    else
        output("uri: first 128 bytes [ ", std::string_view(uri.data(), 128), " ] data: ", data);
    output("mimeType: ", mimeType);
    output("bufferView: ", bufferView);
    output.leave("}");
}

void gltf::Image::read_string(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "uri") { parser.read_string_view(uri); }
    else if (property == "mimeType")
        parser.read_string(mimeType);
    else
        NameExtensionsExtras::read_string(parser, property);
}

void gltf::Image::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "bufferView")
        input >> bufferView;
    else
        parser.warning();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Material
//
void gltf::TextureInfo::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "index")
        input >> index;
    else if (property == "texCoord")
        input >> texCoord;
    else
        parser.warning();
}

void gltf::PbrMetallicRoughness::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "baseColorFactor")
        parser.read_array(baseColorFactor);
    else
        parser.warning();
}

void gltf::PbrMetallicRoughness::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "baseColorTexture")
        parser.read_object(baseColorTexture);
    else if (property == "metallicRoughnessTexture")
        parser.read_object(metallicRoughnessTexture);
    else
        ExtensionsExtras::read_object(parser, property);
}

void gltf::PbrMetallicRoughness::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "metallicFactor")
        input >> metallicFactor;
    else if (property == "roughnessFactor")
        input >> roughnessFactor;
    else
        parser.warning();
}

void gltf::NormalTextureInfo::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "scale")
        input >> scale;
    else
        TextureInfo::read_number(parser, property, input);
}

void gltf::OcclusionTextureInfo::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "strength")
        input >> strength;
    else
        TextureInfo::read_number(parser, property, input);
}

void gltf::KHR_materials_specular::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "specularColorFactor")
        parser.read_array(specularColorFactor);
    else
        parser.warning();
}

void gltf::KHR_materials_specular::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "specularTexture")
        parser.read_object(specularTexture);
    else if (property == "specularColorTexture")
        parser.read_object(specularColorTexture);
    else
        parser.warning();
}

void gltf::KHR_materials_specular::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "specularFactor")
        input >> specularFactor;
    else
        parser.warning();
}

void gltf::KHR_materials_pbrSpecularGlossiness::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "diffuseFactor")
        parser.read_array(diffuseFactor);
    else if (property == "specularFactor")
        parser.read_array(specularFactor);
    else
        parser.warning();
}

void gltf::KHR_materials_pbrSpecularGlossiness::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "diffuseTexture")
        parser.read_object(diffuseTexture);
    else if (property == "specularGlossinessTexture")
        parser.read_object(specularGlossinessTexture);
    else
        parser.warning();
}

void gltf::KHR_materials_pbrSpecularGlossiness::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "glossinessFactor")
        input >> glossinessFactor;
    else
        parser.warning();
}

void gltf::KHR_materials_emissive_strength::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "emissiveStrength")
        input >> emissiveStrength;
    else
        parser.warning();
}

void gltf::KHR_materials_ior::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "ior")
        input >> ior;
    else
        parser.warning();
}

void gltf::KHR_texture_transform::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "offset")
        parser.read_array(offset);
    else if (property == "scale")
        parser.read_array(scale);
    else
        parser.warning();
}

void gltf::KHR_texture_transform::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "rotation")
        input >> rotation;
    else if (property == "texCoord")
        input >> texCoord;
    else
        parser.warning();
}

void gltf::Material::report(vsg::LogOutput& output)
{
    output.enter("Material {");
    NameExtensionsExtras::report(output);
    output("pbrMetallicRoughness.baseColorFactor = ", pbrMetallicRoughness.baseColorFactor.values.size(), " }");
    output("pbrMetallicRoughness.baseColorTexture = { ", pbrMetallicRoughness.baseColorTexture.index, ", ", pbrMetallicRoughness.baseColorTexture.texCoord, " }");
    output("pbrMetallicRoughness.metallicFactor ", pbrMetallicRoughness.metallicFactor);
    output("pbrMetallicRoughness.roughnessFactor ", pbrMetallicRoughness.roughnessFactor);
    output("pbrMetallicRoughness.metallicRoughnessTexture = { ", pbrMetallicRoughness.metallicRoughnessTexture.index, ", ", pbrMetallicRoughness.metallicRoughnessTexture.texCoord, " }");
    output("normalTexture = { ", normalTexture.index, ", ", normalTexture.texCoord, " }");
    output("occlusionTexture = { ", occlusionTexture.index, ", ", occlusionTexture.texCoord, " }");
    output("emissiveTexture = { ", emissiveTexture.index, ", ", emissiveTexture.texCoord, " }");
    output.enter("emissiveFactor : ", emissiveFactor.values.size(), " {");
    for (auto value : emissiveFactor.values) output(" ", value);
    output.leave();
    output("alphaMode : ", alphaMode);
    output("alphaCutoff : ", alphaCutoff);
    output("doubleSided : ", doubleSided);
    output.leave();
}

void gltf::Material::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "emissiveFactor")
        parser.read_array(emissiveFactor);
    else
        parser.warning();
}

void gltf::Material::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "pbrMetallicRoughness")
        parser.read_object(pbrMetallicRoughness);
    else if (property == "normalTexture")
        parser.read_object(normalTexture);
    else if (property == "occlusionTexture")
        parser.read_object(occlusionTexture);
    else if (property == "emissiveTexture")
        parser.read_object(emissiveTexture);
    else
        NameExtensionsExtras::read_object(parser, property);
}

void gltf::Material::read_string(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "alphaMode")
        parser.read_string(alphaMode);
    else
        NameExtensionsExtras::read_string(parser, property);
}

void gltf::Material::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "alphaCutoff")
        input >> alphaCutoff;
    else
        parser.warning();
}

void gltf::Material::read_bool(vsg::JSONParser& parser, const std::string_view& property, bool value)
{
    if (property == "doubleSided")
        doubleSided = value;
    else
        parser.warning();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Mesh
//
void gltf::Attributes::read_number(vsg::JSONParser&, const std::string_view& property, std::istream& input)
{
    input >> values[std::string(property)];
}

void gltf::Primitive::report(vsg::LogOutput& output)
{
    output.enter("Primitive { ");
    ExtensionsExtras::report(output);
    output.enter("attributes = {");
    for (auto& [semantic, id] : attributes.values) output("    ", semantic, ", ", id);
    output.leave();
    output("indices = ", indices);
    output("material = ", material);
    output("mode = ", mode);
    output.enter("targets = [");
    for (auto& value : targets.values)
    {
        output.enter();
        for (auto& [semantic, id] : value->values) output("        ", semantic, ", ", id);
        output.leave();
    }
    output.leave("]");
    output.leave();
}

void gltf::Primitive::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "indices")
        input >> indices;
    else if (property == "material")
        input >> material;
    else if (property == "mode")
        input >> mode;
    else
        parser.warning();
}

void gltf::Primitive::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "targets")
        parser.read_array(targets);
    else
        parser.warning();
}

void gltf::Primitive::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "attributes")
        parser.read_object(attributes);
    else
        ExtensionsExtras::read_object(parser, property);
}

void gltf::Mesh::report(vsg::LogOutput& output)
{
    output.enter("Mesh {");
    NameExtensionsExtras::report(output);
    output("primitives: ", primitives.values);
    for (auto& primitive : primitives.values) primitive->report(output);
    output("weights: ", weights.values);
    output.leave();
}

void gltf::Mesh::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "primitives")
        parser.read_array(primitives);
    else if (property == "weights")
        parser.read_array(weights);
    else
        parser.warning();
}

void gltf::KHR_draco_mesh_compression::report(vsg::LogOutput& output)
{
    output.enter("KHR_draco_mesh_compression {");
    ExtensionsExtras::report(output);
    output.enter("attributes = {");
    for (auto& [semantic, id] : attributes.values) output("    ", semantic, ", ", id);
    output.leave();
    output("bufferView = ", bufferView);
    output.leave();
}

void gltf::KHR_draco_mesh_compression::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "bufferView")
        input >> bufferView;
    else
        parser.warning();
}

void gltf::KHR_draco_mesh_compression::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "attributes")
        parser.read_object(attributes);
    else
        ExtensionsExtras::read_object(parser, property);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Node
//
void gltf::Node::report(vsg::LogOutput& output)
{
    output.enter("Node {");
    NameExtensionsExtras::report(output);
    if (camera) output("camera: ", camera);
    if (skin) output("skin: ", skin);
    if (mesh) output("mesh: ", mesh);
    output("children: ", children.values.size(), ", values = ", children.values);
    output("matrix: ", matrix.values.size(), ", values = ", matrix.values);
    output("rotation: ", rotation.values.size(), " values = ", rotation.values);
    output("scale: ", scale.values.size(), " values = ", scale.values);
    output("translation: ", translation.values.size());
    output("weights: ", weights.values.size());
    output.leave();
}

void gltf::Node::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "children")
        parser.read_array(children);
    else if (property == "matrix")
        parser.read_array(matrix);
    else if (property == "rotation")
        parser.read_array(rotation);
    else if (property == "scale")
        parser.read_array(scale);
    else if (property == "translation")
        parser.read_array(translation);
    else if (property == "weights")
        parser.read_array(weights);
    else
        parser.warning();
}

void gltf::Node::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "camera")
        input >> camera;
    else if (property == "skin")
        input >> skin;
    else if (property == "mesh")
        input >> mesh;
    else
        parser.warning();
}

void gltf::EXT_mesh_gpu_instancing::report(vsg::LogOutput& output)
{
    output.enter("EXT_mesh_gpu_instancing {");
    ExtensionsExtras::report(output);
    output.enter("attributes = {");
    if (attributes)
    {
        for (auto& [semantic, id] : attributes->values) output(semantic, ", ", id);
    }
    output.leave();
    output.leave();
}

void gltf::EXT_mesh_gpu_instancing::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "attributes")
    {
        if (!attributes) attributes = Attributes::create();
        parser.read_object(*attributes);
    }
    else
        ExtensionsExtras::read_object(parser, property);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Sampler
//
void gltf::Sampler::report(vsg::LogOutput& output)
{
    output.enter("Sampler {");
    NameExtensionsExtras::report(output);
    output("minFilter: ", minFilter);
    output("magFilter: ", magFilter);
    output("wrapS: ", wrapS);
    output("wrapT: ", wrapT);
    output.leave();
}

void gltf::Sampler::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "minFilter")
        input >> minFilter;
    else if (property == "magFilter")
        input >> magFilter;
    else if (property == "wrapS")
        input >> wrapS;
    else if (property == "wrapT")
        input >> wrapT;
    else if (property == "wrapR")
        input >> wrapR;
    else
        parser.warning();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Scene
//
void gltf::Scene::report(vsg::LogOutput& output)
{
    output.enter("Scene = {");
    NameExtensionsExtras::report(output);
    output("nodes = ", nodes.values.size());
    output.leave("} ", this);
}

void gltf::Scene::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "nodes")
        parser.read_array(nodes);
    else
        parser.warning();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Texture
//
void gltf::Texture::report(vsg::LogOutput& output)
{
    output.enter("Texture = {");
    NameExtensionsExtras::report(output);
    output("sampler = ", sampler, ", ", source);
    output.leave("} ", this);
}

void gltf::Texture::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "sampler")
        input >> sampler;
    else if (property == "source")
        input >> source;
    else
        parser.warning();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Animation
//
void gltf::AnimationTarget::report(vsg::LogOutput& output)
{
    output.enter("AnimationTarget {");
    output("node = ", node);
    output("path = ", path);
    output.leave();
}

void gltf::AnimationTarget::read_string(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "path")
        parser.read_string(path);
    else
        ExtensionsExtras::read_string(parser, property);
}

void gltf::AnimationTarget::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "node")
        input >> node;
    else
        parser.warning();
}

void gltf::AnimationChannel::report(vsg::LogOutput& output)
{
    output.enter("AnimationChannel {");
    output("sampler = ", sampler);
    target.report(output);
    output.leave();
}

void gltf::AnimationChannel::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "sampler")
        input >> sampler;
    else
        parser.warning();
}

void gltf::AnimationChannel::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "target")
        parser.read_object(target);
    else
        ExtensionsExtras::read_object(parser, property);
}

void gltf::AnimationSampler::report(vsg::LogOutput& out)
{
    out.enter("AnimationSampler {");

    out("input = ", input);
    out("interpolation = ", interpolation);
    out("output = ", output);

    out.leave();
}

void gltf::AnimationSampler::read_string(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "interpolation")
        parser.read_string(interpolation);
    else
        parser.warning();
}

void gltf::AnimationSampler::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& in_input)
{
    if (property == "input")
        in_input >> input;
    else if (property == "output")
        in_input >> output;
    else
        parser.warning();
}

void gltf::Animation::report(vsg::LogOutput& output)
{
    output.enter("Animation {");
    NameExtensionsExtras::report(output);
    output("channels.size() = ", channels.values.size());
    if (!channels.values.empty())
    {
        output.enter();
        for (auto& channel : channels.values)
        {
            channel->report(output);
        }
        output.leave();
    }

    output("samplers.size() = ", samplers.values.size());
    if (!samplers.values.empty())
    {
        output.enter();
        for (auto& sampler : samplers.values)
        {
            sampler->report(output);
        }
        output.leave();
    }

    output.leave();
}

void gltf::Animation::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "channels")
        parser.read_array(channels);
    else if (property == "samplers")
        parser.read_array(samplers);
    else
        parser.warning();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Camera
//
void gltf::Orthographic::report(vsg::LogOutput& output)
{
    output("Orthographic = { xmag = ", xmag, ", ymag = ", ymag, ", znear = ", znear, ", zfar = ", zfar, " }");
}

void gltf::Orthographic::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "xmag")
        input >> xmag;
    else if (property == "ymag")
        input >> ymag;
    else if (property == "znear")
        input >> znear;
    else if (property == "zfar")
        input >> zfar;
    else
        parser.warning();
}

void gltf::Perspective::report(vsg::LogOutput& output)
{
    output("Perspective = { aspectRatio = ", aspectRatio, ", yfov = ", yfov, ", znear = ", znear, ", zfar = ", zfar, " }");
}

void gltf::Perspective::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "aspectRatio")
        input >> aspectRatio;
    else if (property == "yfov")
        input >> yfov;
    else if (property == "znear")
        input >> znear;
    else if (property == "zfar")
        input >> zfar;
    else
        parser.warning();
}

void gltf::Camera::report(vsg::LogOutput& output)
{
    output.enter("Camera {");
    vsg::info("type = ", type);
    if (perspective) perspective->report(output);
    if (orthographic) orthographic->report(output);
    output.leave();
}

void gltf::Camera::read_string(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "type")
        parser.read_string(type);
    else
        NameExtensionsExtras::read_string(parser, property);
}

void gltf::Camera::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "orthographic")
    {
        if (!orthographic) orthographic = Orthographic::create();
        parser.read_object(*orthographic);
    }
    else if (property == "perspective")
    {
        if (!perspective) perspective = Perspective::create();
        parser.read_object(*perspective);
    }
    else
        NameExtensionsExtras::read_object(parser, property);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Skins
//
void gltf::Skins::report(vsg::LogOutput& output)
{
    output.enter("Skins {");
    output("inverseBindMatrices = ", inverseBindMatrices);
    output("skeleton = ", skeleton);
    output("joints = ", joints.values.size());
    output.leave();
}

void gltf::Skins::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "inverseBindMatrices")
        input >> inverseBindMatrices;
    else if (property == "skeleton")
        input >> skeleton;
    else
        parser.warning();
}

void gltf::Skins::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "joints")
        parser.read_array(joints);
    else
        parser.warning();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// KHR_lights_punctual
//
void gltf::Spot::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "innerConeAngle")
        input >> innerConeAngle;
    else if (property == "outerConeAngle")
        input >> outerConeAngle;
    else
        parser.warning();
}

void gltf::Light::read_string(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "type")
        parser.read_string(type);
    else
        NameExtensionsExtras::read_string(parser, property);
}

void gltf::Light::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "intensity")
        input >> intensity;
    else if (property == "range")
        input >> range;
    else
        parser.warning();
}

void gltf::Light::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "color")
        parser.read_array(color);
    else
        parser.warning();
}

void gltf::Light::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "spot")
    {
        spot = gltf::Spot::create();
        parser.read_object(*spot);
    }
    else
        NameExtensionsExtras::read_object(parser, property);
}

void gltf::KHR_lights_punctual::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "light")
        input >> light;
    else
        parser.warning();
}

void gltf::KHR_lights_punctual::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "lights")
        parser.read_array(lights);
    else
        parser.warning();
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// glTF
//
void gltf::glTF::report(vsg::LogOutput& output)
{
    output.enter("glTF {");
    if (asset) asset->report(output);
    accessors.report(output);
    bufferViews.report(output);
    buffers.report(output);
    images.report(output);
    materials.report(output);
    meshes.report(output);
    nodes.report(output);
    samplers.report(output);
    textures.report(output);
    animations.report(output);
    skins.report(output);
    cameras.report(output);
    output("scene = ", scene);
    scenes.report(output);
    output.leave("}");
}

void gltf::glTF::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "extensionsUsed")
        parser.read_array(extensionsUsed);
    else if (property == "extensionsRequired")
        parser.read_array(extensionsRequired);
    else if (property == "accessors")
        parser.read_array(accessors);
    else if (property == "animations")
        parser.read_array(animations);
    else if (property == "buffers")
        parser.read_array(buffers);
    else if (property == "bufferViews")
        parser.read_array(bufferViews);
    else if (property == "cameras")
        parser.read_array(cameras);
    else if (property == "materials")
        parser.read_array(materials);
    else if (property == "meshes")
        parser.read_array(meshes);
    else if (property == "nodes")
        parser.read_array(nodes);
    else if (property == "samplers")
        parser.read_array(samplers);
    else if (property == "scenes")
        parser.read_array(scenes);
    else if (property == "skins")
        parser.read_array(skins);
    else if (property == "images")
        parser.read_array(images);
    else if (property == "textures")
        parser.read_array(textures);
    else
        parser.warning();
}

void gltf::glTF::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "asset")
    {
        asset = gltf::Asset::create();
        parser.read_object(*asset);
    }
    else
        ExtensionsExtras::read_object(parser, property);
}

void gltf::glTF::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "scene")
        input >> scene;
    else
        parser.warning();
}

void gltf::glTF::resolveURIs(vsg::ref_ptr<const vsg::Options> options)
{
    vsg::ref_ptr<vsg::OperationThreads> operationThreads;
    if (options) operationThreads = options->operationThreads;

    auto dataURI = [](const std::string_view& uri, std::string_view& mimeType, std::string_view& encoding, std::string_view& value) -> bool {
        if (uri.size() <= 5) return false;
        if (uri.compare(0, 5, "data:") != 0) return false;

        auto semicolon = uri.find(';', 6);
        auto comma = uri.find(',', semicolon + 1);

        mimeType = std::string_view(&uri[5], semicolon - 5);
        encoding = std::string_view(&uri[semicolon + 1], comma - semicolon - 1);
        value = std::string_view(&uri[comma + 1], uri.size() - comma - 1);

        return true;
    };

    struct OperationWithLatch : public vsg::Inherit<vsg::Operation, OperationWithLatch>
    {
        vsg::ref_ptr<vsg::Latch> latch;

        OperationWithLatch(vsg::ref_ptr<vsg::Latch> l) :
            latch(l) {}
    };

    struct ReadFileOperation : public vsg::Inherit<OperationWithLatch, ReadFileOperation>
    {
        vsg::Path filename;
        vsg::ref_ptr<const vsg::Options> options;
        vsg::ref_ptr<vsg::Data>& data;

        ReadFileOperation(const vsg::Path& f, vsg::ref_ptr<const vsg::Options> o, vsg::ref_ptr<vsg::Data>& d, vsg::ref_ptr<vsg::Latch> l = {}) :
            Inherit(l),
            filename(f),
            options(o),
            data(d) {}

        void run() override
        {
            data = vsg::read_cast<vsg::Data>(filename, options);

            if (latch) latch->count_down();
        }
    };

    struct ReadBufferOperation : public vsg::Inherit<OperationWithLatch, ReadBufferOperation>
    {
        vsg::ref_ptr<Buffer> buffer;
        uint32_t byteOffset = 0;
        uint32_t byteLength = 0;
        vsg::ref_ptr<BufferView> bufferView;
        vsg::ref_ptr<const vsg::Options> options;
        vsg::ref_ptr<vsg::Data>& data;

        ReadBufferOperation(vsg::ref_ptr<Buffer> b, uint32_t offset, uint32_t length, vsg::ref_ptr<const vsg::Options> o, vsg::ref_ptr<vsg::Data>& d, vsg::ref_ptr<vsg::Latch> l = {}) :
            Inherit(l),
            buffer(b),
            byteOffset(offset),
            byteLength(length),
            options(o),
            data(d) {}

        void run() override
        {
            if (!buffer->data)
            {
                vsg::warn("Cannot read for empty buffer.");
                return;
            }

            auto ptr = reinterpret_cast<uint8_t*>(buffer->data->dataPointer()) + byteOffset;

            data = vsg::read_cast<vsg::Data>(ptr, byteLength, options);

            //vsg::info("Read buffer byteLength = ", byteLength, ", data = ", data);
            // if (data) vsg::write(data, vsg::make_string("image_", byteOffset,".png"), options);

            if (latch) latch->count_down();
        }
    };

    struct DecodeOperation : public vsg::Inherit<OperationWithLatch, DecodeOperation>
    {
        std::string_view mimeType;
        std::string_view encoding;
        std::string_view value;
        vsg::ref_ptr<const vsg::Options> options;
        uint32_t byteLength;
        vsg::ref_ptr<vsg::Data>& data;

        DecodeOperation(const std::string_view& m, const std::string_view& e, const std::string_view& v, vsg::ref_ptr<const vsg::Options> o, vsg::ref_ptr<vsg::Data>& d, uint32_t bl, vsg::ref_ptr<vsg::Latch> l = {}) :
            Inherit(l),
            mimeType(m),
            encoding(e),
            value(v),
            options(o),
            byteLength(bl),
            data(d) {}

        void run() override
        {
            if (encoding == "base64")
            {
                auto valid_base64 = [](char c) -> uint8_t {
                    if (c >= 'A' && c <= 'Z') return true;
                    if (c >= 'a' && c <= 'z') return true;
                    if (c >= '0' && c <= '9') return true;
                    if (c == '+') return true;
                    if (c == '/') return true;
                    return false;
                };

                auto decode_base64 = [](char c) -> uint8_t {
                    if (c >= 'A' && c <= 'Z') return c - 'A';
                    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
                    if (c >= '0' && c <= '9') return c - '0' + 52;
                    if (c == '+') return 62;
                    if (c == '/') return 63;
                    return 0;
                };

                while (!value.empty() && !valid_base64(value.back()))
                {
                    value.remove_suffix(1);
                }

                size_t decodedSize = (value.size() * 6) / 8;

                // set up a lookup table to speed up the mapping between encoded to decoded char
                uint8_t lookup[256];
                for (uint32_t c = 0; c < 256; ++c)
                {
                    lookup[c] = decode_base64(c);
                }

                uint32_t lengthToUse = std::min(byteLength, static_cast<uint32_t>(decodedSize));

                // data to stored the decoded data.
                auto decodedData = vsg::ubyteArray::create(lengthToUse);

                // process 4 byte source into 3 byte destination
                auto dest_itr = decodedData->begin();
                auto src_itr = value.begin();

                size_t count = std::min(value.size() / 4, size_t(lengthToUse) / 3);
                size_t srcTailCount = value.size() - count * 4;
                size_t destTailCount = lengthToUse - count * 3;
                for (; count > 0; --count)
                {
                    const uint8_t decodedBytes[4] = {lookup[static_cast<uint8_t>(*(src_itr++))], lookup[static_cast<uint8_t>(*(src_itr++))], lookup[static_cast<uint8_t>(*(src_itr++))], lookup[static_cast<uint8_t>(*(src_itr++))]};

                    (*dest_itr++) = (decodedBytes[0] << 2) + ((decodedBytes[1] & 0x30) >> 4);
                    (*dest_itr++) = ((decodedBytes[1] & 0x0f) << 4) + ((decodedBytes[2] & 0x3c) >> 2);
                    (*dest_itr++) = ((decodedBytes[2] & 0x03) << 6) + decodedBytes[3];
                }

                if (srcTailCount != 0 && destTailCount != 0)
                {
                    const uint8_t decodedBytes[4] = {
                        lookup[static_cast<uint8_t>(*(src_itr++))],
                        srcTailCount >= 2 ? lookup[static_cast<uint8_t>(*(src_itr++))] : uint8_t(0),
                        srcTailCount >= 3 ? lookup[static_cast<uint8_t>(*(src_itr++))] : uint8_t(0),
                        srcTailCount >= 4 ? lookup[static_cast<uint8_t>(*(src_itr++))] : uint8_t(0)};

                    (*dest_itr++) = (decodedBytes[0] << 2) + ((decodedBytes[1] & 0x30) >> 4);
                    if (destTailCount >= 2) (*dest_itr++) = ((decodedBytes[1] & 0x0f) << 4) + ((decodedBytes[2] & 0x3c) >> 2);
                    if (destTailCount >= 3) (*dest_itr++) = ((decodedBytes[2] & 0x03) << 6) + decodedBytes[3];
                }

                // fill in any remaining unassigned bytes to end of decodedData container
                for (; dest_itr != decodedData->end(); ++dest_itr)
                {
                    *dest_itr = 0;
                }

                if (mimeType.compare(0, 6, "image/") == 0)
                {
                    if (auto extensionHint = gltf::mimeTypeToExtension(mimeType); !extensionHint.empty())
                    {
                        auto local_options = vsg::clone(options);
                        local_options->extensionHint = extensionHint;

                        data = vsg::read_cast<vsg::Data>(reinterpret_cast<const uint8_t*>(decodedData->dataPointer()), decodedData->dataSize(), local_options);
                        if (data)
                        {
                            vsg::info("read decoded data [", decodedData->dataSize(), ", ", extensionHint, "] dimensions = {", data->width(), ", ", data->height(), "}");
                        }
                        else
                        {
                            vsg::warn("unable to decoded data [", decodedData->dataSize(), ", ", extensionHint, "]");
                        }
                    }
                    else
                    {
                        vsg::info("Unsupported image data URI : mimeType = ", mimeType, ", encoding = ", encoding, ", value.size() = ", value.size());
                    }
                }
                else
                {
                    // vsg::info("We have a data URI : mimeType = ", mimeType, ", encoding = ", encoding, ", value.size() = ", value.size());

                    data = decodedData;
                }
            }
            else
            {
                vsg::warn("Error: encoding not supported. mimeType = ", mimeType, ", encoding = ", encoding);
            }

            if (latch) latch->count_down();
        }
    };

    std::vector<vsg::ref_ptr<OperationWithLatch>> operations;
    std::vector<vsg::ref_ptr<OperationWithLatch>> secondary_operations;

    for (auto& buffer : buffers.values)
    {
        if (!buffer->data && !buffer->uri.empty())
        {
            std::string_view mimeType;
            std::string_view encoding;
            std::string_view value;
            if (dataURI(buffer->uri, mimeType, encoding, value))
            {
                operations.push_back(DecodeOperation::create(mimeType, encoding, value, options, buffer->data, buffer->byteLength));
            }
            else
            {
                operations.push_back(ReadFileOperation::create(gltf::decodeURI(buffer->uri), options, buffer->data));
            }
        }
    }

    for (auto& image : images.values)
    {
        if (!image->data)
        {
            if (!image->uri.empty())
            {
                std::string_view mimeType;
                std::string_view encoding;
                std::string_view value;
                if (dataURI(image->uri, mimeType, encoding, value))
                {
                    operations.push_back(DecodeOperation::create(mimeType, encoding, value, options, image->data, std::numeric_limits<uint32_t>::max()));
                }
                else
                {
                    operations.push_back(ReadFileOperation::create(gltf::decodeURI(image->uri), options, image->data));
                }
            }
            else if (image->bufferView)
            {
                auto& bufferView = bufferViews.values[image->bufferView.value];
                auto& buffer = buffers.values[bufferView->buffer.value];

                if (auto extensionHint = gltf::mimeTypeToExtension(image->mimeType); !extensionHint.empty())
                {
                    auto local_options = vsg::clone(options);
                    local_options->extensionHint = extensionHint;

                    secondary_operations.push_back(ReadBufferOperation::create(buffer, bufferView->byteOffset, bufferView->byteLength, local_options, image->data));
                }
            }
            else
            {
                vsg::warn("No image uri or bufferView to read image from.");
            }
        }
    }

    if (operations.size() > 1 && operationThreads)
    {
        auto latch = vsg::Latch::create(static_cast<int>(operations.size()));
        for (auto& operation : operations)
        {
            operation->latch = latch;
        }

        operationThreads->add(operations.begin(), operations.end(), vsg::INSERT_FRONT);

        // use this thread to read the files as well
        operationThreads->run();

        // wait till all the read operations have completed
        latch->wait();

        vsg::debug("Completed multi-threaded read/decode");
    }
    else
    {
        for (auto& operation : operations)
        {
            operation->run();
        }
        vsg::debug("Completed single-threaded read/decode");
    }

    // now read any images that were dependent on loading/decoding of shared buffers.
    if (secondary_operations.size() > 1 && operationThreads)
    {
        auto secondary_latch = vsg::Latch::create(static_cast<int>(secondary_operations.size()));
        for (auto& operation : secondary_operations)
        {
            operation->latch = secondary_latch;
        }

        operationThreads->add(secondary_operations.begin(), secondary_operations.end(), vsg::INSERT_FRONT);

        // use this thread to read the files as well
        operationThreads->run();

        // wait till all the read secondary_operations have completed
        secondary_latch->wait();

        vsg::debug("Completed secondary_ multi-threaded read/decode");
    }
    else if (!secondary_operations.empty())
    {
        for (auto& operation : secondary_operations)
        {
            operation->run();
        }
        vsg::debug("Completed secondary single-threaded read/decode");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// gltf
//
gltf::gltf()
{
    //    level = vsg::Logger::LOGGER_FATAL;
}

bool gltf::supportedExtension(const vsg::Path& ext) const
{
    return ext == ".gltf" || ext == ".glb";
}

vsg::ref_ptr<vsg::Object> gltf::read_gltf(std::istream& fin, vsg::ref_ptr<const vsg::Options> options, const vsg::Path& filename) const
{
    fin.seekg(0, fin.end);
    size_t fileSize = fin.tellg();

    if (fileSize == 0) return {};

    vsg::JSONParser parser;
    parser.options = options;

    // set up the supported extensions
    assignExtensions(parser);

    parser.buffer.resize(fileSize);
    fin.seekg(0);
    fin.read(reinterpret_cast<char*>(parser.buffer.data()), fileSize);

    vsg::ref_ptr<vsg::Object> result;

    // skip white space
    parser.pos = parser.buffer.find_first_not_of(" \t\r\n", 0);
    if (parser.pos == std::string::npos) return {};

    if (parser.buffer[parser.pos] == '{')
    {
        auto root = gltf::glTF::create();
        root->filename = filename;

        parser.read_object(*root);

        if (!parser.warnings.empty())
        {
            vsg::warn("glTF parsing failure : ", filename);
            for (auto& warning : parser.warnings) vsg::log(level, warning);
            return {};
        }

        root->resolveURIs(options);

        if (vsg::value<bool>(false, gltf::report, options))
        {
            vsg::LogOutput output;
            root->report(output);
        }

        auto builder = gltf::SceneGraphBuilder::create();
        if (options)
        {
            vsg::Path ext = (options->extensionHint) ? options->extensionHint : vsg::lowerCaseFileExtension(filename);
            if (auto itr = options->formatCoordinateConventions.find(ext); itr != options->formatCoordinateConventions.end())
            {
                builder->source_coordinateConvention = itr->second;
            }
        }

        result = builder->createSceneGraph(root, options);
    }
    else
    {
        vsg::warn("glTF parsing error, could not find opening {");
    }

    return result;
}

vsg::ref_ptr<vsg::Object> gltf::read_glb(std::istream& fin, vsg::ref_ptr<const vsg::Options> options, const vsg::Path& filename) const
{
    fin.seekg(0);

    struct Header
    {
        char magic[4] = {0, 0, 0, 0};
        uint32_t version = 0;
        uint32_t length = 0;
    };

    struct Chunk
    {
        uint32_t chunkLength = 0;
        uint32_t chunkType = 0;
    };

    Header header;
    fin.read(reinterpret_cast<char*>(&header), sizeof(Header));
    if (!fin.good())
    {
        vsg::warn("IO error reading GLB file, &fin=", &fin, ", options = ", options, ", filename = ", filename);
        return {};
    }

    if (strncmp(header.magic, "glTF", 4) != 0)
    {
        vsg::warn("magic number not glTF, header.magic = ", header.magic);
        return {};
    }

    Chunk chunk0;
    fin.read(reinterpret_cast<char*>(&chunk0), sizeof(Chunk));
    if (!fin.good())
    {
        vsg::warn("IO error reading GLB file.");
        return {};
    }

    uint32_t jsonSize = chunk0.chunkLength; // - sizeof(Chunk);

    vsg::JSONParser parser;
    parser.options = options;

    // set up the supported extensions
    assignExtensions(parser);

    parser.buffer.resize(jsonSize);
    fin.read(reinterpret_cast<char*>(parser.buffer.data()), jsonSize);

    Chunk chunk1;
    fin.read(reinterpret_cast<char*>(&chunk1), sizeof(Chunk));
    if (!fin.good())
    {
        vsg::warn("IO error reading GLB file.");
        return {};
    }

    uint32_t binarySize = chunk1.chunkLength; // - sizeof(Chunk);
    auto binaryData = vsg::ubyteArray::create(binarySize);
    fin.read(reinterpret_cast<char*>(binaryData->dataPointer()), binarySize);

    vsg::ref_ptr<vsg::Object> result;

    // skip white space
    parser.pos = parser.buffer.find_first_not_of(" \t\r\n", 0);
    if (parser.pos == std::string::npos) return {};

    if (parser.buffer[parser.pos] == '{')
    {
        auto root = gltf::glTF::create();
        root->filename = filename;

        parser.read_object(*root);

        if (!parser.warnings.empty())
        {
            if (level != vsg::Logger::LOGGER_OFF)
            {
                vsg::warn("glTF parsing failure : ", filename);
                for (auto& warning : parser.warnings) vsg::log(level, warning);
            }
            return {};
        }

        if (root->buffers.values.size() >= 1)
        {
            auto& firstBuffer = root->buffers.values.front();
            if (firstBuffer->uri.empty() && firstBuffer->byteLength == binarySize)
            {
                firstBuffer->data = binaryData;
            }
            else
            {
                vsg::warn("First glTF Buffer not comptible with binary data");
            }
        }
        else
        {
            auto binaryBuffer = Buffer::create();
            binaryBuffer->byteLength = binarySize;
            binaryBuffer->data = binaryData;

            root->buffers.values.push_back(binaryBuffer);
        }

        root->resolveURIs(options);

        if (vsg::value<bool>(false, gltf::report, options))
        {
            vsg::info("gltf::read_glb() filename = ", filename);
            vsg::LogOutput output;
            root->report(output);
        }

        auto builder = gltf::SceneGraphBuilder::create();
        if (options)
        {
            vsg::Path ext = (options->extensionHint) ? options->extensionHint : vsg::lowerCaseFileExtension(filename);
            if (auto itr = options->formatCoordinateConventions.find(ext); itr != options->formatCoordinateConventions.end())
            {
                builder->source_coordinateConvention = itr->second;
            }
        }

        result = builder->createSceneGraph(root, options);

        if (result && filename) result->setValue("gltf", filename);
    }
    else
    {
        vsg::warn("glTF parsing error, could not find opening {");
    }
    return result;
}

vsg::ref_ptr<vsg::Object> gltf::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    vsg::Path ext = vsg::lowerCaseFileExtension(filename);
    if (!supportedExtension(ext)) return {};

    if (vsg::value<bool>(false, gltf::disable_gltf, options)) return {};

    vsg::Path filenameToUse = vsg::findFile(filename, options);
    if (!filenameToUse) return {};

    auto opt = vsg::clone(options);
    opt->paths.insert(opt->paths.begin(), vsg::filePath(filenameToUse));

    std::ifstream fin(filenameToUse, std::ios::ate | std::ios::binary);

    if (ext == ".gltf")
        return read_gltf(fin, opt, filename);
    else
        return read_glb(fin, opt, filename);
}

vsg::ref_ptr<vsg::Object> gltf::read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const
{
    if (vsg::value<bool>(false, gltf::disable_gltf, options)) return {};

    if (!options || !options->extensionHint) return {};
    if (!supportedExtension(options->extensionHint)) return {};

    if (options->extensionHint == ".gltf")
        return read_gltf(fin, options);
    else
        return read_glb(fin, options);
}

vsg::ref_ptr<vsg::Object> gltf::read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options) const
{
    if (vsg::value<bool>(false, gltf::disable_gltf, options)) return {};

    if (!options || !options->extensionHint) return {};
    if (!supportedExtension(options->extensionHint)) return {};

    vsg::mem_stream fin(ptr, size);

    if (options->extensionHint == ".gltf")
        return read_gltf(fin, options);
    else
        return read_glb(fin, options);
}

bool gltf::readOptions(vsg::Options& options, vsg::CommandLine& arguments) const
{
    bool result = arguments.readAndAssign<bool>(gltf::report, &options);
    result = arguments.readAndAssign<bool>(gltf::culling, &options) || result;
    result = arguments.readAndAssign<bool>(gltf::disable_gltf, &options) || result;
    result = arguments.readAndAssign<bool>(gltf::clone_accessors, &options) || result;
    result = arguments.readAndAssign<float>(gltf::maxAnisotropy, &options) || result;
    return result;
}

bool gltf::getFeatures(Features& features) const
{
    vsg::ReaderWriter::FeatureMask supported_features = static_cast<vsg::ReaderWriter::FeatureMask>(vsg::ReaderWriter::READ_FILENAME | vsg::ReaderWriter::READ_ISTREAM | vsg::ReaderWriter::READ_MEMORY);
    features.extensionFeatureMap[".gltf"] = supported_features;
    features.extensionFeatureMap[".glb"] = supported_features;

    features.optionNameTypeMap[gltf::report] = vsg::type_name<bool>();
    features.optionNameTypeMap[gltf::culling] = vsg::type_name<bool>();
    features.optionNameTypeMap[gltf::disable_gltf] = vsg::type_name<bool>();
    features.optionNameTypeMap[gltf::clone_accessors] = vsg::type_name<bool>();
    features.optionNameTypeMap[gltf::maxAnisotropy] = vsg::type_name<float>();

    return true;
}

bool gltf::dataURI(const std::string_view& uri, std::string_view& mimeType, std::string_view& encoding, std::string_view& value)
{
    if (uri.size() <= 5) return false;
    if (uri.compare(0, 5, "data:") != 0) return false;

    auto semicolon = uri.find(';', 6);
    auto comma = uri.find(',', semicolon + 1);

    mimeType = std::string_view(&uri[5], semicolon - 5);
    encoding = std::string_view(&uri[semicolon + 1], comma - semicolon - 1);
    value = std::string_view(&uri[comma + 1], uri.size() - comma - 1);

    return true;
}

vsg::Path gltf::decodeURI(const std::string_view& uri)
{
    if (uri.empty()) return {};

    std::string decoded_uri(uri);

    std::string::size_type percent_space;
    while ((percent_space = decoded_uri.find("%20")) != std::string::npos)
    {
        decoded_uri[percent_space] = ' ';
        decoded_uri.erase(percent_space + 1, 2);
    }

    return decoded_uri;
}

vsg::Path gltf::mimeTypeToExtension(const std::string_view& mimeType)
{
    if (mimeType == "image/png")
        return ".png";
    else if (mimeType == "image/jpeg")
        return ".jpeg";
    else if (mimeType == "image/bmp")
        return ".bmp";
    else if (mimeType == "image/gif")
        return ".gif";
    else if (mimeType == "image/ktx")
        return ".ktx";
    return "";
}

void gltf::assignExtensions(vsg::JSONParser& parser) const
{
    // set up the supported extensions
    parser.setObject("KHR_draco_mesh_compression", KHR_draco_mesh_compression::create());
    parser.setObject("KHR_materials_specular", KHR_materials_specular::create());
    parser.setObject("KHR_materials_ior", KHR_materials_ior::create());
    parser.setObject("EXT_mesh_gpu_instancing", EXT_mesh_gpu_instancing::create());
    parser.setObject("KHR_materials_unlit", KHR_materials_unlit::create());
    parser.setObject("KHR_texture_transform", KHR_texture_transform::create());
    parser.setObject("KHR_lights_punctual", KHR_lights_punctual::create());
    parser.setObject("KHR_materials_pbrSpecularGlossiness", KHR_materials_pbrSpecularGlossiness::create());
    parser.setObject("KHR_materials_emissive_strength", KHR_materials_emissive_strength::create());
}
