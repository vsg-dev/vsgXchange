/* <editor-fold desc="MIT License">

Copyright(c) 2025 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgXchange/3DTiles.h>

#include <vsg/io/Path.h>
#include <vsg/io/mem_stream.h>
#include <vsg/io/read.h>
#include <vsg/io/write.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/threading/OperationThreads.h>
#include <vsg/utils/CommandLine.h>

#include <fstream>
#include <iostream>
#include <stack>

using namespace vsgXchange;

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BoundingVolume
//
void Tiles3D::BoundingVolume::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "box")
    {
        parser.read_array(box);
    }
    else if (property == "region")
    {
        parser.read_array(region);
    }
    else if (property == "sphere")
    {
        parser.read_array(sphere);
    }
    else
        parser.warning();
}

void Tiles3D::BoundingVolume::report(vsg::LogOutput& output)
{
    output.enter("BoundingVolume {");
    output("box = ", box.values);
    output("region = ", region.values);
    output("sphere = ", sphere.values);
    output.leave();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Content
//
void Tiles3D::Content::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "boundingVolume")
    {
        boundingVolume = BoundingVolume::create();
        parser.read_object(*boundingVolume);
    }
    else
        ExtensionsExtras::read_object(parser, property);
}

void Tiles3D::Content::read_string(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "uri")
        parser.read_string(uri);
    else
        parser.warning();
}

void Tiles3D::Content::report(vsg::LogOutput& output)
{
    output.enter("Content {");
    if (boundingVolume) boundingVolume->report(output);
    output("uri = ", uri);
    output.leave();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tile
//
void Tiles3D::Tile::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "transform")
    {
        parser.read_array(transform);
    }
    else if (property == "children")
    {
        parser.read_array(children);
    }
    else
        parser.warning();
}

void Tiles3D::Tile::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "boundingVolume")
    {
        boundingVolume = BoundingVolume::create();
        parser.read_object(*boundingVolume);
    }
    else if (property == "viewerRequestVolume")
    {
        viewerRequestVolume = BoundingVolume::create();
        parser.read_object(*viewerRequestVolume);
    }
    else if (property == "content")
    {
        content = Content::create();
        parser.read_object(*content);
    }
    else
        ExtensionsExtras::read_object(parser, property);
}

void Tiles3D::Tile::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "geometricError")
        input >> geometricError;
    else
        parser.warning();
}

void Tiles3D::Tile::read_string(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "refine")
        parser.read_string(refine);
    else
        parser.warning();
}

void Tiles3D::Tile::report(vsg::LogOutput& output)
{
    output.enter("Tile {");

    output("geometricError = ", geometricError);
    output("refine = ", refine);
    if (content) content->report(output);
    output("transform = ", transform.values);
    if (boundingVolume) boundingVolume->report(output);
    if (viewerRequestVolume) viewerRequestVolume->report(output);

    if (children.values.empty())
        output("children {}");
    else
    {
        output.enter("children {");
        for (auto& child : children.values)
        {
            child->report(output);
        }
        output.leave();
    }

    output.leave();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Properties
//
void Tiles3D::PropertyRange::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "minimum")
        input >> minimum;
    else if (property == "maximum")
        input >> maximum;
    else
        parser.warning();
}

void Tiles3D::Properties::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    parser.read_object(properties[std::string(property)]);
}

void Tiles3D::Properties::report(vsg::LogOutput& output)
{
    output.enter("Properties {");
    for (auto& [name, property] : properties)
    {
        output(name, " { minimum = ", property.minimum, ", maximum = ", property.maximum, " }");
    }
    output.leave();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Asset
//
void Tiles3D::Asset::read_string(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "version")
        parser.read_string(version);
    else if (property == "tilesetVersion")
        parser.read_string(tilesetVersion);
    else
        parser.read_string(strings[std::string(property)]);
}

void Tiles3D::Asset::read_number(vsg::JSONParser&, const std::string_view& property, std::istream& input)
{
    input >> numbers[std::string(property)];
}

void Tiles3D::Asset::report(vsg::LogOutput& output)
{
    output.enter("Asset {");
    output("version = ", version);
    output("tilesetVersion = ", tilesetVersion);
    if (!strings.empty())
    {
        for (auto& [name, value] : strings)
        {
            output(name, " = ", value);
        }
    }
    if (!numbers.empty())
    {
        for (auto& [name, value] : numbers)
        {
            output(name, " = ", value);
        }
    }
    output("extras = ", extras);
    output.leave();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tileset
//
void Tiles3D::Tileset::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "extensionsUsed")
        parser.read_array(extensionsUsed);
    else if (property == "extensionsRequired")
        parser.read_array(extensionsRequired);
    else
        parser.warning();
}

void Tiles3D::Tileset::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "asset")
    {
        asset = Asset::create();
        parser.read_object(*asset);
    }
    else if (property == "properties")
    {
        properties = Properties::create();
        parser.read_object(*properties);
    }
    else if (property == "root")
    {
        root = Tile::create();
        parser.read_object(*root);
    }
    else
        ExtensionsExtras::read_object(parser, property);
}

void Tiles3D::Tileset::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "geometricError")
        input >> geometricError;
    else
        parser.warning();
}

void Tiles3D::Tileset::report(vsg::LogOutput& output)
{
    output.enter("Tileset {");

    if (asset) asset->report(output);
    if (properties) properties->report(output);

    output("geometricError = ", geometricError);
    output("extensionsUsed = ", extensionsUsed.values);
    output("extensionsRequired = ", extensionsRequired.values);

    if (root) root->report(output);

    output.leave();
}

void Tiles3D::Tileset::resolveURIs(vsg::ref_ptr<const vsg::Options> options)
{
    vsg::info("Tiles3D::Tileset::resolveURIs()");

    std::stack<Tile*> tileStack;

    size_t tileCount = 0;

    if (root)
    {
        tileStack.push(root);
    }

    while (!tileStack.empty())
    {
        ++tileCount;
        auto tile = tileStack.top();
        tileStack.pop();
        if (tile->content && !(tile->content->uri.empty()))
        {
            tile->content->object = vsg::read(tile->content->uri, options);
            if (tile->content->object)
            {
                vsg::info("Loaded uri = ", tile->content->uri, ", object = ", tile->content->object);
            }
            else
            {
                vsg::info("uri = ", tile->content->uri, ", unable to load.");
            }
        }
        for (auto child : tile->children.values)
        {
            tileStack.push(child);
        }
    }

    vsg::info("tileCount = ", tileCount);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Batch
//
void Tiles3D::Batch::read_number(vsg::JSONParser&, std::istream& input)
{
    if (componentType == "UNSIGNED_INT")
    {
        uint32_t value;
        input >> value;

        addToArray(vsg::uintValue::create(value));
    }
    else
    {
        double value;
        input >> value;

        addToArray(vsg::doubleValue::create(value));
    }
}

void Tiles3D::Batch::read_string(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "componentType")
        parser.read_string(componentType);
    else if (property == "type")
        parser.read_string(type);
    else
        parser.warning();
}

void Tiles3D::Batch::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "byteOffset")
        input >> byteOffset;
    else
        parser.warning();
}

void Tiles3D::Batch::convert(BatchTable& batchTable)
{
    if (object) return;

    if (objects)
    {
        if (objects->children.empty())
        {
            vsg::warn("Tiles3D::Batch::convert() failed ", objects, " empty.");
            return;
        }

        if (objects->children.size() == 1)
        {
            object = objects->children.front();
            return;
        }

        auto first_child = objects->children.front();
        size_t numDifferent = 0;
        size_t numSame = 0;
        for (auto& child : objects->children)
        {
            if (first_child->type_info() != child->type_info())
                ++numDifferent;
            else
                ++numSame;
        }

        if (numDifferent == 0)
        {

            struct ValuesToArray : public vsg::ConstVisitor
            {
                ValuesToArray(vsg::Objects::Children& in_children) :
                    children(in_children) {}

                vsg::Objects::Children children;
                vsg::ref_ptr<vsg::Data> array;

                void apply(const vsg::Object& value) override
                {
                    vsg::warn("Tiles3D::Batch::convert() unhandled type ", value.className());
                }

                void apply(const vsg::stringValue&) override
                {
                    auto strings = vsg::stringArray::create(children.size());
                    auto itr = strings->begin();
                    for (auto& child : children)
                    {
                        if (auto sv = child.cast<vsg::stringValue>())
                            *itr = sv->value();
                        else
                            vsg::warn("Unable to convert to stringValue ", child);
                        ++itr;
                    }

                    array = strings;
                }

                void apply(const vsg::floatValue&) override
                {
                    auto floats = vsg::floatArray::create(children.size());
                    auto itr = floats->begin();
                    for (auto& child : children)
                    {
                        if (auto sv = child.cast<vsg::floatValue>())
                            *itr = sv->value();
                        else
                            vsg::warn("Unable to convert to floatValue ", child);
                        ++itr;
                    }

                    array = floats;
                }

                void apply(const vsg::doubleValue&) override
                {
                    auto doubles = vsg::doubleArray::create(children.size());
                    auto itr = doubles->begin();
                    for (auto& child : children)
                    {
                        if (auto sv = child.cast<vsg::doubleValue>())
                            *itr = sv->value();
                        else
                            vsg::warn("Unable to convert to doubleValue ", child);
                        ++itr;
                    }

                    array = doubles;
                }
            };

            ValuesToArray vta(objects->children);
            first_child->accept(vta);

            if (vta.array)
            {
                object = vta.array;
            }
            else
            {
                vsg::warn("Tiles3D::Batch::convert() enable to convert ", objects);
                object = objects;
            }
        }
        else
        {
            // mixed data so the best we can do it just use the vsg::Objects object for access
            object = objects;
        }
    }
    else if (type == "SCALAR")
    {
        if (componentType == "BYTE")
            object = vsg::byteArray::create(batchTable.binary, byteOffset, 1, batchTable.length);
        else if (componentType == "UNSIGNED_BYTE")
            object = vsg::ubyteArray::create(batchTable.binary, byteOffset, 1, batchTable.length);
        else if (componentType == "SHORT")
            object = vsg::shortArray::create(batchTable.binary, byteOffset, 2, batchTable.length);
        else if (componentType == "UNSIGNED_SHORT")
            object = vsg::ushortArray::create(batchTable.binary, byteOffset, 2, batchTable.length);
        else if (componentType == "INT")
            object = vsg::intArray::create(batchTable.binary, byteOffset, 4, batchTable.length);
        else if (componentType == "UNSIGNED_INT")
            object = vsg::uintArray::create(batchTable.binary, byteOffset, 4, batchTable.length);
        else if (componentType == "FLOAT")
            object = vsg::floatArray::create(batchTable.binary, byteOffset, 4, batchTable.length);
        else if (componentType == "DOUBLE")
            object = vsg::doubleArray::create(batchTable.binary, byteOffset, 8, batchTable.length);
        else
            vsg::warn("Unsupported Tiles3D::Batch SCALAR componentType = ", componentType);
    }
    else if (type == "VEC2")
    {
        if (componentType == "BYTE")
            object = vsg::bvec2Array::create(batchTable.binary, byteOffset, 2, batchTable.length);
        else if (componentType == "UNSIGNED_BYTE")
            object = vsg::ubvec2Array::create(batchTable.binary, byteOffset, 2, batchTable.length);
        else if (componentType == "SHORT")
            object = vsg::svec2Array::create(batchTable.binary, byteOffset, 4, batchTable.length);
        else if (componentType == "UNSIGNED_SHORT")
            object = vsg::usvec2Array::create(batchTable.binary, byteOffset, 4, batchTable.length);
        else if (componentType == "INT")
            object = vsg::ivec2Array::create(batchTable.binary, byteOffset, 8, batchTable.length);
        else if (componentType == "UNSIGNED_INT")
            object = vsg::uivec2Array::create(batchTable.binary, byteOffset, 8, batchTable.length);
        else if (componentType == "FLOAT")
            object = vsg::vec2Array::create(batchTable.binary, byteOffset, 8, batchTable.length);
        else if (componentType == "DOUBLE")
            object = vsg::dvec2Array::create(batchTable.binary, byteOffset, 16, batchTable.length);
        else
            vsg::warn("Unsupported Tiles3D::Batch VEC2 componentType = ", componentType);
    }
    else if (type == "VEC3")
    {
        if (componentType == "BYTE")
            object = vsg::bvec3Array::create(batchTable.binary, byteOffset, 3, batchTable.length);
        else if (componentType == "UNSIGNED_BYTE")
            object = vsg::ubvec3Array::create(batchTable.binary, byteOffset, 3, batchTable.length);
        else if (componentType == "SHORT")
            object = vsg::svec3Array::create(batchTable.binary, byteOffset, 6, batchTable.length);
        else if (componentType == "UNSIGNED_SHORT")
            object = vsg::usvec3Array::create(batchTable.binary, byteOffset, 6, batchTable.length);
        else if (componentType == "INT")
            object = vsg::ivec3Array::create(batchTable.binary, byteOffset, 12, batchTable.length);
        else if (componentType == "UNSIGNED_INT")
            object = vsg::uivec3Array::create(batchTable.binary, byteOffset, 12, batchTable.length);
        else if (componentType == "FLOAT")
            object = vsg::vec3Array::create(batchTable.binary, byteOffset, 12, batchTable.length);
        else if (componentType == "DOUBLE")
            object = vsg::dvec3Array::create(batchTable.binary, byteOffset, 16, batchTable.length);
        else
            vsg::warn("Unsupported Tiles3D::Batch VEC3 componentType = ", componentType);
    }
    else if (type == "VEC4")
    {
        if (componentType == "BYTE")
            object = vsg::bvec4Array::create(batchTable.binary, byteOffset, 4, batchTable.length);
        else if (componentType == "UNSIGNED_BYTE")
            object = vsg::ubvec4Array::create(batchTable.binary, byteOffset, 4, batchTable.length);
        else if (componentType == "SHORT")
            object = vsg::svec4Array::create(batchTable.binary, byteOffset, 8, batchTable.length);
        else if (componentType == "UNSIGNED_SHORT")
            object = vsg::usvec4Array::create(batchTable.binary, byteOffset, 8, batchTable.length);
        else if (componentType == "INT")
            object = vsg::ivec4Array::create(batchTable.binary, byteOffset, 16, batchTable.length);
        else if (componentType == "UNSIGNED_INT")
            object = vsg::uivec4Array::create(batchTable.binary, byteOffset, 16, batchTable.length);
        else if (componentType == "FLOAT")
            object = vsg::vec4Array::create(batchTable.binary, byteOffset, 16, batchTable.length);
        else if (componentType == "DOUBLE")
            object = vsg::dvec4Array::create(batchTable.binary, byteOffset, 32, batchTable.length);
        else
            vsg::warn("Unsupported Tiles3D::Batch VEC4 componentType = ", componentType);
    }
    else
    {
        vsg::warn("Unsupported Tiles3D::Batch type = ", type);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BatchTable
//
void Tiles3D::BatchTable::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    auto batch = Batch::create();

    // for batchID hint that the type should be uint.
    if (property == "batchId") batch->componentType = "UNSIGNED_INT";

    parser.read_array(*batch);
    batches[std::string(property)] = batch;
}

void Tiles3D::BatchTable::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    auto batch = Batch::create();
    parser.read_object(*batch);
    batches[std::string(property)] = batch;
}

void Tiles3D::BatchTable::convert()
{
    for (auto itr = batches.begin(); itr != batches.end(); ++itr) itr->second->convert(*this);
}

void Tiles3D::BatchTable::report(vsg::LogOutput& output)
{
    struct PrintValues : public vsg::ConstVisitor
    {
        vsg::LogOutput& out;
        PrintValues(vsg::LogOutput& o) :
            out(o) {}

        void apply(const vsg::stringValue& v) override
        {
            out(v.value());
        }

        void apply(const vsg::floatValue& v) override
        {
            out(v.value());
        }

        void apply(const vsg::doubleValue& v) override
        {
            out(v.value());
        }

        void apply(const vsg::stringArray& strings) override
        {
            for (auto value : strings) out(value);
        }

        void apply(const vsg::intArray& ints) override
        {
            for (auto value : ints) out(value);
        }

        void apply(const vsg::doubleArray& doubles) override
        {
            for (auto value : doubles) out(value);
        }
    } pv(output);

    for (auto& [name, batch] : batches)
    {
        output.enter("batch ", name, " {");

        if (batch->object)
        {
            output.enter("object = ", batch->object);

            batch->object->accept(pv);

            output.leave();
        }
        else if (batch->objects)
        {
            output.enter("objects = ", batch->objects);

            for (auto& child : batch->objects->children)
            {
                output("child = ", child);
            }

            output.leave();
        }
        else
        {
            output("byteOffset = ", batch->byteOffset);
            output("componentType = ", batch->componentType);
            output("type = ", batch->type);
        }

        output.leave();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tiles3D
//
Tiles3D::Tiles3D()
{
}

bool Tiles3D::supportedExtension(const vsg::Path& ext) const
{
    return ext == ".tiles" || ext == ".json" || ext == ".b3dm" || ext == ".cmpt" || ext == ".i3dm" || ext == ".pnts";
}

vsg::ref_ptr<vsg::Object> Tiles3D::read_tiles(const vsg::Path&, vsg::ref_ptr<const vsg::Options> options) const
{
    auto non_const_options = const_cast<vsg::Options*>(options.get());

    auto tile = non_const_options->getRefObject<vsgXchange::Tiles3D::Tile>("tile");
    auto builder = non_const_options->getRefObject<vsgXchange::Tiles3D::SceneGraphBuilder>("builder");

    uint32_t lod_level = 0;
    non_const_options->getValue("level", lod_level);

    std::string inherited_refine;
    non_const_options->getValue("refine", inherited_refine);

    if (tile && builder)
        return builder->readTileChildren(tile, lod_level, inherited_refine);
    else
        return {};
}

vsg::ref_ptr<vsg::Object> Tiles3D::read_json(std::istream& fin, vsg::ref_ptr<const vsg::Options> options, const vsg::Path& filename) const
{
    vsg::info("Tiles3D::read_json(", filename, ")");

    fin.seekg(0, fin.end);
    size_t fileSize = fin.tellg();

    if (fileSize == 0) return {};

    vsg::JSONParser parser;
    parser.options = options;

    parser.buffer.resize(fileSize);
    fin.seekg(0);
    fin.read(reinterpret_cast<char*>(parser.buffer.data()), fileSize);

    vsg::ref_ptr<vsg::Object> result;

    // skip white space
    parser.pos = parser.buffer.find_first_not_of(" \t\r\n", 0);
    if (parser.pos == std::string::npos) return {};

    if (parser.buffer[parser.pos] == '{')
    {
        auto tileset = Tiles3D::Tileset::create();

        parser.read_object(*tileset);

        if (!parser.warnings.empty())
        {
            if (level != vsg::Logger::LOGGER_OFF)
            {
                vsg::warn("3DTiles parsing failure : ", filename);
                for (auto& warning : parser.warnings) vsg::log(level, warning);
            }
            return {};
        }

        auto builder = Tiles3D::SceneGraphBuilder::create();

        auto opt = vsg::clone(options);

        if (tileset->asset)
        {
            const auto& strings = tileset->asset->strings;
            if (auto itr = strings.find("gltfUpAxis"); itr != strings.end())
            {
                std::string gltfUpAxis = itr->second;
                vsg::CoordinateConvention upAxis = vsg::CoordinateConvention::Y_UP;
                if (gltfUpAxis == "X")
                    upAxis = vsg::CoordinateConvention::X_UP;
                else if (gltfUpAxis == "Y")
                    upAxis = vsg::CoordinateConvention::Y_UP;
                else if (gltfUpAxis == "Z")
                    upAxis = vsg::CoordinateConvention::Z_UP;

                opt->formatCoordinateConventions[".gltf"] = upAxis;
                opt->formatCoordinateConventions[".glb"] = upAxis;
                builder->source_coordinateConvention = upAxis;
            }
        }

        if (vsg::value<bool>(false, Tiles3D::report, options))
        {
            vsg::LogOutput output;
            output("Tiles3D::read_json() filename = ", filename);
            tileset->report(output);
        }

        builder->preLoadLevel = vsg::value<uint32_t>(builder->preLoadLevel, Tiles3D::pre_load_level, options);
        builder->pixelErrorToScreenHeightRatio = vsg::value<double>(builder->pixelErrorToScreenHeightRatio, Tiles3D::pixel_ratio, options);

        return builder->createSceneGraph(tileset, opt);
    }
    else
    {
        return {};
    }
}

vsg::ref_ptr<vsg::Object> Tiles3D::read_pnts(std::istream&, vsg::ref_ptr<const vsg::Options>, const vsg::Path&) const
{
    vsg::warn("Tiles3D::read_pnts(..) not implemented yet.");
    return {};
}

vsg::ref_ptr<vsg::Object> Tiles3D::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    vsg::Path ext = vsg::lowerCaseFileExtension(filename);
    if (!supportedExtension(ext)) return {};

    if (ext == ".tiles") return read_tiles(filename, options);

    vsg::Path filenameToUse = vsg::findFile(filename, options);
    if (!filenameToUse) return {};

    auto opt = vsg::clone(options);
    opt->paths.insert(opt->paths.begin(), vsg::filePath(filenameToUse));

    std::ifstream fin(filenameToUse, std::ios::ate | std::ios::binary);

    // vsg::info("Tiles3D::read(", filename, ", ", options, ") &fin = ", &fin);

    if (ext == ".json")
        return read_json(fin, opt, filename);
    else if (ext == ".b3dm")
        return read_b3dm(fin, opt, filename);
    else if (ext == ".cmpt")
        return read_cmpt(fin, opt, filename);
    else if (ext == ".i3dm")
        return read_i3dm(fin, opt, filename);
    else if (ext == ".pnts")
        return read_pnts(fin, opt, filename);
    else
    {
        vsg::warn("Tiles3D::read() unhandled file type ", options->extensionHint);
        return {};
    }
}

vsg::ref_ptr<vsg::Object> Tiles3D::read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!options || !options->extensionHint) return {};
    if (!supportedExtension(options->extensionHint)) return {};

    if (options->extensionHint == ".json")
        return read_json(fin, options);
    else if (options->extensionHint == ".b3dm")
        return read_b3dm(fin, options);
    else if (options->extensionHint == ".cmpt")
        return read_cmpt(fin, options);
    else if (options->extensionHint == ".i3dm")
        return read_i3dm(fin, options);
    else if (options->extensionHint == ".pnts")
        return read_pnts(fin, options);
    else
    {
        vsg::warn("Tiles3D::read() unhandled file type ", options->extensionHint);
        return {};
    }
}

vsg::ref_ptr<vsg::Object> Tiles3D::read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!options || !options->extensionHint) return {};
    if (!supportedExtension(options->extensionHint)) return {};

    vsg::mem_stream fin(ptr, size);

    if (options->extensionHint == ".json")
        return read_json(fin, options);
    else if (options->extensionHint == ".b3dm")
        return read_b3dm(fin, options);
    else if (options->extensionHint == ".cmpt")
        return read_cmpt(fin, options);
    else if (options->extensionHint == ".i3dm")
        return read_i3dm(fin, options);
    else if (options->extensionHint == ".pnts")
        return read_pnts(fin, options);
    else
    {
        vsg::warn("Tiles3D::read() unhandled file type ", options->extensionHint);
        return {};
    }
}

bool Tiles3D::readOptions(vsg::Options& options, vsg::CommandLine& arguments) const
{
    bool result = arguments.readAndAssign<bool>(Tiles3D::report, &options);
    result = arguments.readAndAssign<bool>(Tiles3D::instancing, &options) | result;
    result = arguments.readAndAssign<double>(Tiles3D::pixel_ratio, &options) | result;
    result = arguments.readAndAssign<uint32_t>(Tiles3D::pre_load_level, &options) | result;
    return result;
}

bool Tiles3D::getFeatures(Features& features) const
{
    vsg::ReaderWriter::FeatureMask supported_features = static_cast<vsg::ReaderWriter::FeatureMask>(vsg::ReaderWriter::READ_FILENAME | vsg::ReaderWriter::READ_ISTREAM | vsg::ReaderWriter::READ_MEMORY);
    features.extensionFeatureMap[".json"] = supported_features;
    features.extensionFeatureMap[".tiles"] = supported_features;
    features.extensionFeatureMap[".b3dm"] = supported_features;
    features.extensionFeatureMap[".cmpt"] = supported_features;
    features.extensionFeatureMap[".i3dm"] = supported_features;
    features.extensionFeatureMap[".pnts"] = supported_features;

    features.optionNameTypeMap[Tiles3D::report] = vsg::type_name<bool>();
    features.optionNameTypeMap[Tiles3D::instancing] = vsg::type_name<bool>();
    features.optionNameTypeMap[Tiles3D::pixel_ratio] = vsg::type_name<double>();
    features.optionNameTypeMap[Tiles3D::pre_load_level] = vsg::type_name<uint32_t>();

    return true;
}
