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
    else parser.warning();
}

void Tiles3D::BoundingVolume::report(vsg::LogOutput& output)
{
    output("BoundingVolume {");
    output.in();
    output("box = ", box.values);
    output("region = ", region.values);
    output("sphere = ", sphere.values);
    output.out();
    output("}");
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
    else ExtensionsExtras::read_object(parser, property);
}

void Tiles3D::Content::read_string(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "uri") parser.read_string(uri);
    else parser.warning();
}

void Tiles3D::Content::report(vsg::LogOutput& output)
{
    output("Content {");
    output.in();
    if (boundingVolume) boundingVolume->report(output);
    output("uri = ", uri);
    output.out();
    output("}");
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
    else parser.warning();
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
    else ExtensionsExtras::read_object(parser, property);
}


void Tiles3D::Tile::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "geometricError") input >> geometricError;
    else parser.warning();
}

void Tiles3D::Tile::read_string(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "refine") parser.read_string(refine);
    else parser.warning();
}

void Tiles3D::Tile::report(vsg::LogOutput& output)
{
    output("Tile {");
    output.in();

    output("geometricError = ", geometricError);
    output("refine = ", refine);
    if (content) content->report(output);
    output("transform = ", transform.values);
    if (boundingVolume) boundingVolume->report(output);
    if (viewerRequestVolume) viewerRequestVolume->report(output);

    if (children.values.empty()) output("children {}");
    else
    {
        output("children {");
        output.in();
        for(auto& child : children.values)
        {
            child->report(output);
        }
        output.out();
        output("}");
    }

    output.out();
    output("}");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Properties
//
void Tiles3D::Properties::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "minimum") input >> minimum;
    else if (property == "maximum") input >> maximum;
    else parser.warning();
}

void Tiles3D::Properties::report(vsg::LogOutput& output)
{
    output("Properties {");
    output.in();
    output("minimum = ", minimum);
    output("maximum = ", maximum);
    output.out();
    output("}");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Asset
//
void Tiles3D::Asset::read_string(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "version") parser.read_string(version);
    else if (property == "tilesetVersion") parser.read_string(tilesetVersion);
    else parser.read_string(strings[std::string(property)]);
}

void Tiles3D::Asset::read_number(vsg::JSONParser&, const std::string_view& property, std::istream& input)
{
    input >> numbers[std::string(property)];
}

void Tiles3D::Asset::report(vsg::LogOutput& output)
{
    output("Asset {");
    output.in();
    output("version = ", version);
    output("tilesetVersion = ", tilesetVersion);
    if (!strings.empty())
    {
        for(auto& [name, value] : strings)
        {
            output(name, " = ", value);
        }
    }
    if (!numbers.empty())
    {
        for(auto& [name, value] : numbers)
        {
            output(name, " = ", value);
        }
    }
    output("extras = ", extras);
    output.out();
    output("}");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tileset
//
void Tiles3D::Tileset::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "extensionsUsed") parser.read_array(extensionsUsed);
    else if (property == "extensionsRequired") parser.read_array(extensionsRequired);
    else parser.warning();
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
    else ExtensionsExtras::read_object(parser, property);
}

void Tiles3D::Tileset::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "geometricError") input >> geometricError;
    else parser.warning();
}


void Tiles3D::Tileset::report(vsg::LogOutput& output)
{
    output("Tileset {");
    output.in();

    if (asset) asset->report(output);
    if (properties) properties->report(output);

    output("geometricError = ", geometricError);
    output("extensionsUsed = ", extensionsUsed.values);
    output("extensionsRequired = ", extensionsRequired.values);

    if (root) root->report(output);

    output.out();
    output("}");
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

    while(!tileStack.empty())
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
        for(auto child : tile->children.values)
        {
            tileStack.push(child);
        }
    }

    vsg::info("tileCount = ", tileCount);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// b3dm_FeatureTable
//
void Tiles3D::b3dm_FeatureTable::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property=="RTC_CENTER") parser.read_array(RTC_CENTER);
    else parser.warning();
}

void Tiles3D::b3dm_FeatureTable::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property=="RTC_CENTER") RTC_CENTER.read_and_assign(parser, *binary);
    else parser.warning();
}

void Tiles3D::b3dm_FeatureTable::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property=="BATCH_LENGTH") input >> BATCH_LENGTH;
    else parser.warning();
}

void Tiles3D::b3dm_FeatureTable::report(vsg::LogOutput& output)
{
    output("b3dm_FeatureTable { ");
    output("    RTC_CENTER ", RTC_CENTER.values);
    output("    BATCH_LENGTH ", BATCH_LENGTH);
    output("}");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// i3dm_FeatureTable
//
void Tiles3D::i3dm_FeatureTable::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property=="POSITION") parser.read_array(POSITION);
    else if (property=="POSITION_QUANTIZED") parser.read_array(POSITION_QUANTIZED);
    else if (property=="NORMAL_UP") parser.read_array(NORMAL_UP);
    else if (property=="NORMAL_RIGHT") parser.read_array(NORMAL_RIGHT);
    else if (property=="NORMAL_UP_OCT32P") parser.read_array(NORMAL_UP_OCT32P);
    else if (property=="NORMAL_RIGHT_OCT32P") parser.read_array(NORMAL_RIGHT_OCT32P);
    else if (property=="SCALE") parser.read_array(SCALE);
    else if (property=="SCALE_NON_UNIFORM") parser.read_array(SCALE_NON_UNIFORM);
    else if (property=="RTC_CENTER") parser.read_array(RTC_CENTER);
    else if (property=="QUANTIZED_VOLUME_OFFSET") parser.read_array(QUANTIZED_VOLUME_OFFSET);
    else if (property=="QUANTIZED_VOLUME_SCALE") parser.read_array(QUANTIZED_VOLUME_SCALE);
    else parser.warning();
}

void Tiles3D::i3dm_FeatureTable::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property=="POSITION") POSITION.read_and_assign(parser, *binary);
    else if (property=="POSITION_QUANTIZED") POSITION_QUANTIZED.read_and_assign(parser, *binary);
    else if (property=="NORMAL_UP") NORMAL_UP.read_and_assign(parser, *binary);
    else if (property=="NORMAL_RIGHT") NORMAL_RIGHT.read_and_assign(parser, *binary);
    else if (property=="NORMAL_UP_OCT32P") NORMAL_UP_OCT32P.read_and_assign(parser, *binary);
    else if (property=="NORMAL_RIGHT_OCT32P") NORMAL_RIGHT_OCT32P.read_and_assign(parser, *binary);
    else if (property=="SCALE") SCALE.read_and_assign(parser, *binary);
    else if (property=="SCALE_NON_UNIFORM") SCALE_NON_UNIFORM.read_and_assign(parser, *binary);
    else if (property=="RTC_CENTER") RTC_CENTER.read_and_assign(parser, *binary);
    else if (property=="QUANTIZED_VOLUME_OFFSET") QUANTIZED_VOLUME_OFFSET.read_and_assign(parser, *binary);
    else if (property=="QUANTIZED_VOLUME_SCALE") QUANTIZED_VOLUME_SCALE.read_and_assign(parser, *binary);
    else parser.warning();
}

void Tiles3D::i3dm_FeatureTable::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property=="BATCH_ID") input >> BATCH_ID;
    if (property=="INSTANCES_LENGTH") input >> INSTANCES_LENGTH;
    else parser.warning();
}

void Tiles3D::i3dm_FeatureTable::read_bool(vsg::JSONParser& parser, const std::string_view& property, bool value)
{
    if (property=="EAST_NORTH_UP") EAST_NORTH_UP = value;
    else parser.warning();
}

void Tiles3D::i3dm_FeatureTable::report(vsg::LogOutput& output)
{
    output("i3dm_FeatureTable { ");
    output("    POSITION ", POSITION.values);
    output("    POSITION_QUANTIZED ", POSITION_QUANTIZED.values);
    output("    NORMAL_UP ", NORMAL_UP.values);
    output("    NORMAL_RIGHT ", NORMAL_RIGHT.values);
    output("    NORMAL_UP_OCT32P ", NORMAL_UP_OCT32P.values);
    output("    NORMAL_RIGHT_OCT32P ", NORMAL_RIGHT_OCT32P.values);
    output("    SCALE ", SCALE.values);
    output("    SCALE_NON_UNIFORM ", SCALE_NON_UNIFORM.values);
    output("    RTC_CENTER ", RTC_CENTER.values);
    output("    QUANTIZED_VOLUME_OFFSET ", QUANTIZED_VOLUME_OFFSET.values);
    output("    QUANTIZED_VOLUME_SCALE ", QUANTIZED_VOLUME_SCALE.values);
    output("    BATCH_ID ", BATCH_ID);
    output("    INSTANCES_LENGTH ", INSTANCES_LENGTH);
    output("}");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Batch
//
void Tiles3D::Batch::read_number(vsg::JSONParser&, std::istream& input)
{
    if (componentType=="UNSIGNED_INT")
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
    if (property == "componentType") parser.read_string(componentType);
    else if (property == "type") parser.read_string(type);
    else parser.warning();
}

void Tiles3D::Batch::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "byteOffset") input >> byteOffset;
    else parser.warning();
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
        for(auto& child : objects->children)
        {
            if (first_child->type_info() != child->type_info()) ++numDifferent;
            else ++numSame;
        }

        if (numDifferent == 0)
        {

            struct ValuesToArray : public vsg::ConstVisitor
            {
                ValuesToArray(vsg::Objects::Children& in_children) : children(in_children) {}

                vsg::Objects::Children children;
                vsg::ref_ptr<vsg::Data> array;

                void apply(const vsg::Object& value) override
                {
                    vsg::warn("Tiles3D::Batch::convert() unhandled type ",  value.className());
                }

                void apply(const vsg::stringValue&) override
                {
                    auto strings = vsg::stringArray::create(children.size());
                    auto itr = strings->begin();
                    for(auto& child : children)
                    {
                        if (auto sv = child.cast<vsg::stringValue>()) *itr = sv->value();
                        else vsg::warn("Unable to convert to stringValue ", child);
                        ++itr;
                    }

                    array = strings;
                }

                void apply(const vsg::floatValue&) override
                {
                    auto floats = vsg::floatArray::create(children.size());
                    auto itr = floats->begin();
                    for(auto& child : children)
                    {
                        if (auto sv = child.cast<vsg::floatValue>()) *itr = sv->value();
                        else vsg::warn("Unable to convert to floatValue ", child);
                        ++itr;
                    }

                    array = floats;
                }

                void apply(const vsg::doubleValue&) override
                {
                    auto doubles = vsg::doubleArray::create(children.size());
                    auto itr = doubles->begin();
                    for(auto& child : children)
                    {
                        if (auto sv = child.cast<vsg::doubleValue>()) *itr = sv->value();
                        else vsg::warn("Unable to convert to doubleValue ", child);
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
        if (componentType=="BYTE") object = vsg::byteArray::create(batchTable.binary, byteOffset, 1, batchTable.length);
        else if (componentType=="UNSIGNED_BYTE") object = vsg::ubyteArray::create(batchTable.binary, byteOffset, 1, batchTable.length);
        else if (componentType=="SHORT") object = vsg::shortArray::create(batchTable.binary, byteOffset, 2, batchTable.length);
        else if (componentType=="UNSIGNED_SHORT") object = vsg::ushortArray::create(batchTable.binary, byteOffset, 2, batchTable.length);
        else if (componentType=="INT") object = vsg::intArray::create(batchTable.binary, byteOffset, 4, batchTable.length);
        else if (componentType=="UNSIGNED_INT") object = vsg::uintArray::create(batchTable.binary, byteOffset, 4, batchTable.length);
        else if (componentType=="FLOAT") object = vsg::floatArray::create(batchTable.binary, byteOffset, 4, batchTable.length);
        else if (componentType=="DOUBLE") object = vsg::doubleArray::create(batchTable.binary, byteOffset, 8, batchTable.length);
        else vsg::warn("Unsupported Tiles3D::Batch SCALAR componentType = ", componentType);
    }
    else if (type == "VEC2")
    {
        if (componentType=="BYTE") object = vsg::bvec2Array::create(batchTable.binary, byteOffset, 2, batchTable.length);
        else if (componentType=="UNSIGNED_BYTE") object = vsg::ubvec2Array::create(batchTable.binary, byteOffset, 2, batchTable.length);
        else if (componentType=="SHORT") object = vsg::svec2Array::create(batchTable.binary, byteOffset, 4, batchTable.length);
        else if (componentType=="UNSIGNED_SHORT") object = vsg::usvec2Array::create(batchTable.binary, byteOffset, 4, batchTable.length);
        else if (componentType=="INT") object = vsg::ivec2Array::create(batchTable.binary, byteOffset, 8, batchTable.length);
        else if (componentType=="UNSIGNED_INT") object = vsg::uivec2Array::create(batchTable.binary, byteOffset, 8, batchTable.length);
        else if (componentType=="FLOAT") object = vsg::vec2Array::create(batchTable.binary, byteOffset, 8, batchTable.length);
        else if (componentType=="DOUBLE") object = vsg::dvec2Array::create(batchTable.binary, byteOffset, 16, batchTable.length);
        else vsg::warn("Unsupported Tiles3D::Batch VEC2 componentType = ", componentType);
    }
    else if (type == "VEC3")
    {
        if (componentType=="BYTE") object = vsg::bvec3Array::create(batchTable.binary, byteOffset, 3, batchTable.length);
        else if (componentType=="UNSIGNED_BYTE") object = vsg::ubvec3Array::create(batchTable.binary, byteOffset, 3, batchTable.length);
        else if (componentType=="SHORT") object = vsg::svec3Array::create(batchTable.binary, byteOffset, 6, batchTable.length);
        else if (componentType=="UNSIGNED_SHORT") object = vsg::usvec3Array::create(batchTable.binary, byteOffset, 6, batchTable.length);
        else if (componentType=="INT") object = vsg::ivec3Array::create(batchTable.binary, byteOffset, 12, batchTable.length);
        else if (componentType=="UNSIGNED_INT") object = vsg::uivec3Array::create(batchTable.binary, byteOffset, 12, batchTable.length);
        else if (componentType=="FLOAT") object = vsg::vec3Array::create(batchTable.binary, byteOffset, 12, batchTable.length);
        else if (componentType=="DOUBLE") object = vsg::dvec3Array::create(batchTable.binary, byteOffset, 16, batchTable.length);
        else vsg::warn("Unsupported Tiles3D::Batch VEC3 componentType = ", componentType);
    }
    else if (type == "VEC4")
    {
        if (componentType=="BYTE") object = vsg::bvec4Array::create(batchTable.binary, byteOffset, 4, batchTable.length);
        else if (componentType=="UNSIGNED_BYTE") object = vsg::ubvec4Array::create(batchTable.binary, byteOffset, 4, batchTable.length);
        else if (componentType=="SHORT") object = vsg::svec4Array::create(batchTable.binary, byteOffset, 8, batchTable.length);
        else if (componentType=="UNSIGNED_SHORT") object = vsg::usvec4Array::create(batchTable.binary, byteOffset, 8, batchTable.length);
        else if (componentType=="INT") object = vsg::ivec4Array::create(batchTable.binary, byteOffset, 16, batchTable.length);
        else if (componentType=="UNSIGNED_INT") object = vsg::uivec4Array::create(batchTable.binary, byteOffset, 16, batchTable.length);
        else if (componentType=="FLOAT") object = vsg::vec4Array::create(batchTable.binary, byteOffset, 16, batchTable.length);
        else if (componentType=="DOUBLE") object = vsg::dvec4Array::create(batchTable.binary, byteOffset, 32, batchTable.length);
        else vsg::warn("Unsupported Tiles3D::Batch VEC4 componentType = ", componentType);
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
    if (property=="batchId") batch->componentType = "UNSIGNED_INT";

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
    for(auto itr = batches.begin(); itr != batches.end(); ++itr) itr->second->convert(*this);
}

void Tiles3D::BatchTable::report(vsg::LogOutput& output)
{
    struct PrintValues : public vsg::ConstVisitor
    {
        vsg::LogOutput& out;
        PrintValues(vsg::LogOutput& o) : out(o) {}

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
            for(auto value : strings) out(value);
        }

        void apply(const vsg::intArray& ints) override
        {
            for(auto value : ints) out(value);
        }

        void apply(const vsg::doubleArray& doubles) override
        {
            for(auto value : doubles) out(value);
        }
    } pv(output);

    for(auto& [name, batch] : batches)
    {
        output("batch ", name, " {");

        output.in();

        if (batch->object)
        {
            output("object = ", batch->object);
            output.in();

            batch->object->accept(pv);

            output.out();
        }
        else if (batch->objects)
        {
            output("objects = ", batch->objects);

            output.in();

            for(auto& child : batch->objects->children)
            {
                output("child = ", child);
            }

            output.out();
        }
        else
        {
            output("byteOffset = ", batch->byteOffset);
            output("componentType = ", batch->componentType);
            output("type = ", batch->type);
        }

        output.out();

        output("}");
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
    return ext == ".json" || ext == ".b3dm" || ext == ".cmpt" || ext == ".i3dm" || ext == ".pnts";
}

vsg::ref_ptr<vsg::Object> Tiles3D::read_json(std::istream& fin, vsg::ref_ptr<const vsg::Options> options, const vsg::Path& filename) const
{
    vsg::info("Tiles3D::read_json()");

    fin.seekg(0, fin.end);
    size_t fileSize = fin.tellg();

    if (fileSize==0) return {};

    vsg::JSONParser parser;
    parser.level =  level;
    parser.options = options;

    parser.buffer.resize(fileSize);
    fin.seekg(0);
    fin.read(reinterpret_cast<char*>(parser.buffer.data()), fileSize);

    vsg::ref_ptr<vsg::Object> result;

    // skip white space
    parser.pos = parser.buffer.find_first_not_of(" \t\r\n", 0);
    if (parser.pos == std::string::npos) return {};

    if (parser.buffer[parser.pos]=='{')
    {
        auto root = Tiles3D::Tileset::create();

        parser.warningCount = 0;
        parser.read_object(*root);

        root->resolveURIs(options);

        if (parser.warningCount != 0) vsg::warn("3DTiles parsing failure : ", filename);
        else vsg::debug("3DTiles parsing success : ", filename);

        if (vsg::value<bool>(false, gltf::report, options))
        {
            vsg::LogOutput output;
            output("Tiles3D::read_json() filename = ", filename);
            root->report(output);
        }

        auto builder = Tiles3D::SceneGraphBuilder::create();
        result = builder->createSceneGraph(root, options);
    }
    else
    {
        vsg::warn("glTF parsing error, could not find opening {");
    }

    return result;
}

vsg::ref_ptr<vsg::Object> Tiles3D::read_b3dm(std::istream& fin, vsg::ref_ptr<const vsg::Options> options, const vsg::Path&) const
{
    vsg::info("Tiles3D::read_b3dm()");

    fin.seekg(0);

    // https://github.com/CesiumGS/3d-tiles/tree/1.0/specification/TileFormats/Batched3DModel
    struct Header
    {
        char magic[4] = {0,0,0,0};
        uint32_t version = 0;
        uint32_t byteLength = 0;
        uint32_t featureTableJSONByteLength = 0;
        uint32_t featureTableBinaryByteLength = 0;
        uint32_t batchTableJSONByteLength = 0;
        uint32_t batchTableBinaryLength = 0;
    };

    Header header;
    fin.read(reinterpret_cast<char*>(&header), sizeof(Header));

    if (!fin.good())
    {
        vsg::warn("IO error reading bd3m file.");
        return {};
    }

    if (strncmp(header.magic, "b3dm", 4) != 0)
    {
        vsg::warn("magic number not b3dm");
        return {};
    }

    // Feature table
    // Batch table
    // Binary glTF

    vsg::ref_ptr<b3dm_FeatureTable> featureTable = b3dm_FeatureTable::create();
    if (header.featureTableJSONByteLength > 0)
    {
        featureTable = b3dm_FeatureTable::create();

        vsg::JSONParser parser;
        parser.buffer.resize(header.featureTableJSONByteLength);
        fin.read(parser.buffer.data(), header.featureTableJSONByteLength);

        if (header.featureTableBinaryByteLength > 0)
        {
            featureTable->binary = vsg::ubyteArray::create(header.featureTableBinaryByteLength);
            fin.read(reinterpret_cast<char*>(featureTable->binary->dataPointer()), header.featureTableBinaryByteLength);
        }

        parser.read_object(*featureTable);
    }

    vsg::ref_ptr<BatchTable> batchTable = BatchTable::create();

    if (header.batchTableJSONByteLength > 0)
    {
        vsg::JSONParser parser;
        parser.buffer.resize(header.batchTableJSONByteLength);
        fin.read(parser.buffer.data(), header.batchTableJSONByteLength);

        if (header.batchTableBinaryLength > 0 )
        {
            batchTable->binary = vsg::ubyteArray::create(header.batchTableBinaryLength);
            fin.read(reinterpret_cast<char*>(batchTable->binary->dataPointer()), header.batchTableBinaryLength);
        }

        parser.read_object(*batchTable);

        batchTable->length = featureTable->BATCH_LENGTH;
        batchTable->convert();
   }

    if (vsg::value<bool>(false, gltf::report, options))
    {
        vsg::LogOutput output;

        output("magic = ", header.magic);
        output("version = ", header.version);
        output("byteLength = ", header.byteLength);
        output("featureTableJSONByteLength = ", header.featureTableJSONByteLength);
        output("featureTableBinaryByteLength = ", header.featureTableBinaryByteLength);
        output("batchTableJSONByteLength = ", header.batchTableJSONByteLength);
        output("batchTableBinaryLength = ", header.batchTableBinaryLength);

        vsg::LogOutput log;

        if (featureTable) featureTable->report(output);
        if (batchTable) batchTable->report(output);
    }

    // uint32_t size_of_feature_and_batch_tables = header.featureTableJSONByteLength + header.featureTableBinaryByteLength + header.batchTableJSONByteLength + header.batchTableBinaryLength;
    // uint32_t size_of_gltfField = header.byteLength - sizeof(Header) - size_of_feature_and_batch_tables;

#if 0
    fin.seekg(0, fin.end);
    size_t fileSize = fin.tellg();

    std::string buffer;
    buffer.resize(fileSize);
    fin.read(reinterpret_cast<char*>(buffer.data()), fileSize);
#endif

    vsg::ref_ptr<vsg::Object> result;

    return result;
}

vsg::ref_ptr<vsg::Object> Tiles3D::read_cmpt(std::istream& fin, vsg::ref_ptr<const vsg::Options> options, const vsg::Path&) const
{
    vsg::info("Tiles3D::read_cmpt(..)");

    fin.seekg(0);

    // https://github.com/CesiumGS/3d-tiles/blob/main/specification/TileFormats/Composite/README.adoc
    struct Header
    {
        char magic[4] = {0,0,0,0};
        uint32_t version = 0;
        uint32_t byteLength = 0;
        uint32_t tilesLength = 0;
    };

    struct InnerHeader
    {
        char magic[4] = {0,0,0,0};
        uint32_t version = 0;
        uint32_t byteLength = 0;
    };

    Header header;
    fin.read(reinterpret_cast<char*>(&header), sizeof(Header));

    if (!fin.good())
    {
        vsg::warn("IO error reading cmpt file.");
        return {};
    }

    if (strncmp(header.magic, "cmpt", 4) != 0)
    {
        vsg::warn("magic number not cmpt");
        return {};
    }

    std::list<InnerHeader> innerHeaders;
    for(uint32_t i=0; i < header.tilesLength; ++i)
    {
        InnerHeader innerHeader;
        fin.read(reinterpret_cast<char*>(&innerHeader), sizeof(InnerHeader));

        if (!fin.good())
        {
            vsg::warn("IO error reading cmpt file.");
            return {};
        }

        // skip over rest of tile to next one.
        fin.seekg(innerHeader.byteLength - sizeof(InnerHeader), fin.cur);

        innerHeaders.push_back(innerHeader);
    }

    if (vsg::value<bool>(false, gltf::report, options))
    {
        vsg::LogOutput output;

        output("magic = ", header.magic);
        output("version = ", header.version);
        output("byteLength = ", header.byteLength);
        output("tilesLength = ", header.tilesLength);

        output("innerHeaders.size() = ", innerHeaders.size());
        for(auto& innerHeader : innerHeaders)
        {
            output("   {", innerHeader.magic, ", ", innerHeader.version, ", ", innerHeader.byteLength, " }");
        }
    }


    vsg::ref_ptr<vsg::Object> result;

    return result;
}

vsg::ref_ptr<vsg::Object> Tiles3D::read_i3dm(std::istream& fin, vsg::ref_ptr<const vsg::Options> options, const vsg::Path&) const
{
    vsg::info("Tiles3D::read_i3dm(..)");
    fin.seekg(0);

    // https://github.com/CesiumGS/3d-tiles/blob/main/specification/TileFormats/Instanced3DModel/README.adoc
    struct Header
    {
        char magic[4] = {0,0,0,0};
        uint32_t version = 0;
        uint32_t byteLength = 0;
        uint32_t featureTableJSONByteLength = 0;
        uint32_t featureTableBinaryByteLength = 0;
        uint32_t batchTableJSONByteLength = 0;
        uint32_t batchTableBinaryLength = 0;
        uint32_t gltfFormat = 0;
    };

    Header header;
    fin.read(reinterpret_cast<char*>(&header), sizeof(Header));

    if (!fin.good())
    {
        vsg::warn("IO error reading i3dm file.");
        return {};
    }

    if (strncmp(header.magic, "i3dm", 4) != 0)
    {
        vsg::warn("magic number not i3dm");
        return {};
    }

    // Feature table
    // Batch table
    // Binary glTF

    vsg::ref_ptr<i3dm_FeatureTable> featureTable = i3dm_FeatureTable::create();
    if (header.featureTableJSONByteLength > 0)
    {
        vsg::info("Reading i3dm_FeatureTable");

        featureTable = i3dm_FeatureTable::create();

        vsg::JSONParser parser;
        parser.buffer.resize(header.featureTableJSONByteLength);
        fin.read(parser.buffer.data(), header.featureTableJSONByteLength);

        if (header.featureTableBinaryByteLength > 0)
        {
            featureTable->binary = vsg::ubyteArray::create(header.featureTableBinaryByteLength);
            fin.read(reinterpret_cast<char*>(featureTable->binary->dataPointer()), header.featureTableBinaryByteLength);
        }

        parser.read_object(*featureTable);

        vsg::info("finished Reading i3dm_FeatureTable");
    }

    vsg::ref_ptr<BatchTable> batchTable;
    if (header.batchTableJSONByteLength > 0)
    {
        batchTable = BatchTable::create();

        vsg::JSONParser parser;
        parser.buffer.resize(header.batchTableJSONByteLength);
        fin.read(parser.buffer.data(), header.batchTableJSONByteLength);

        if (header.batchTableBinaryLength > 0 )
        {
            batchTable->binary = vsg::ubyteArray::create(header.batchTableBinaryLength);
            fin.read(reinterpret_cast<char*>(batchTable->binary->dataPointer()), header.batchTableBinaryLength);
        }

        parser.read_object(*batchTable);

        batchTable->length = featureTable->INSTANCES_LENGTH;
        batchTable->convert();
   }

    if (vsg::value<bool>(false, gltf::report, options))
    {
        vsg::LogOutput output;

        output("magic = ", header.magic);
        output("version = ", header.version);
        output("byteLength = ", header.byteLength);
        output("featureTableJSONByteLength = ", header.featureTableJSONByteLength);
        output("featureTableBinaryByteLength = ", header.featureTableBinaryByteLength);
        output("batchTableJSONByteLength = ", header.batchTableJSONByteLength);
        output("batchTableBinaryLength = ", header.batchTableBinaryLength);
        output("gltfFormat = ", header.gltfFormat);

        if (featureTable) featureTable->report(output);
        if (batchTable) batchTable->report(output);
    }

    uint32_t size_of_feature_and_batch_tables = header.featureTableJSONByteLength + header.featureTableBinaryByteLength + header.batchTableJSONByteLength + header.batchTableBinaryLength;
    uint32_t size_of_gltfField = header.byteLength - sizeof(Header) - size_of_feature_and_batch_tables;

    vsg::ref_ptr<vsg::Node> model;
    if (header.gltfFormat==0)
    {
        std::string uri;
        uri.resize(size_of_gltfField);

        fin.read(uri.data(), size_of_gltfField);

        // trim trailing null characters
        while(uri.back()==0) uri.pop_back();

        // load model
        model = vsg::read_cast<vsg::Node>(uri, options);
    }
    else
    {
        std::string binary;
        binary.resize(size_of_gltfField);
        fin.read(binary.data(), size_of_gltfField);

        vsg::mem_stream binary_fin(reinterpret_cast<uint8_t*>(binary.data()), binary.size());

        auto opt = vsg::clone(options);
        opt->extensionHint = ".glb";

        model = vsg::read_cast<vsg::Node>(binary_fin, opt);
    }

    if (model)
    {
        // TODO: place transform above model based on POSITION, SCALE, NORMAL_UP etc.

        vsg::dvec3 position;
        vsg::dquat rotation;
        vsg::dvec3 scale(1.0, 1.0, 1.0);

        auto transform = vsg::MatrixTransform::create();
        transform->matrix = vsg::translate(position) * vsg::rotate(rotation) * vsg::scale(scale);
        transform->addChild(model);

        model = transform;
    }

    return model;
}

vsg::ref_ptr<vsg::Object> Tiles3D::read_pnts(std::istream&, vsg::ref_ptr<const vsg::Options>, const vsg::Path&) const
{
    vsg::warn("Tiles3D::read_pnts(..) not implemented yet.");
    return {};
}


vsg::ref_ptr<vsg::Object> Tiles3D::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    vsg::Path ext  = (options && options->extensionHint) ? options->extensionHint : vsg::lowerCaseFileExtension(filename);
    if (!supportedExtension(ext)) return {};

    vsg::Path filenameToUse = vsg::findFile(filename, options);
    if (!filenameToUse) return {};

    auto opt = vsg::clone(options);
    opt->paths.insert(opt->paths.begin(), vsg::filePath(filenameToUse));

    std::ifstream fin(filenameToUse, std::ios::ate | std::ios::binary);

    if (ext==".json") return read_json(fin, opt, filename);
    else if (ext==".b3dm") return read_b3dm(fin, opt, filename);
    else if (ext==".cmpt") return read_cmpt(fin, opt, filename);
    else if (ext==".i3dm") return read_i3dm(fin, opt, filename);
    else if (ext==".pnts") return read_pnts(fin, opt, filename);
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

    if (options->extensionHint==".json") return read_json(fin, options);
    else if (options->extensionHint==".b3dm") return read_b3dm(fin, options);
    else if (options->extensionHint==".cmpt") return read_cmpt(fin, options);
    else if (options->extensionHint==".i3dm") return read_i3dm(fin, options);
    else if (options->extensionHint==".pnts") return read_pnts(fin, options);
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

    if (options->extensionHint==".json") return read_json(fin, options);
    else if (options->extensionHint==".b3dm") return read_b3dm(fin, options);
    else if (options->extensionHint==".cmpt") return read_cmpt(fin, options);
    else if (options->extensionHint==".i3dm") return read_i3dm(fin, options);
    else if (options->extensionHint==".pnts") return read_pnts(fin, options);
    else
    {
        vsg::warn("Tiles3D::read() unhandled file type ", options->extensionHint);
        return {};
    }
}


bool Tiles3D::readOptions(vsg::Options& options, vsg::CommandLine& arguments) const
{
    bool result = arguments.readAndAssign<bool>(Tiles3D::report, &options);
    return result;
}

bool Tiles3D::getFeatures(Features& features) const
{
    vsg::ReaderWriter::FeatureMask supported_features = static_cast<vsg::ReaderWriter::FeatureMask>(vsg::ReaderWriter::READ_FILENAME | vsg::ReaderWriter::READ_ISTREAM | vsg::ReaderWriter::READ_MEMORY);
    features.extensionFeatureMap[".json"] = supported_features;
    features.extensionFeatureMap[".b3dm"] = supported_features;
    features.extensionFeatureMap[".cmpt"] = supported_features;
    features.extensionFeatureMap[".i3dm"] = supported_features;
    features.extensionFeatureMap[".pnts"] = supported_features;

    return true;
}
