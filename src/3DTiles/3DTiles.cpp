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
#include <vsg/threading/OperationThreads.h>
#include <vsg/utils/CommandLine.h>

#include <fstream>
#include <iostream>

using namespace vsgXchange;

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tileset
//

void Tiles3D::Tileset::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
}

void Tiles3D::Tileset::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
}

void Tiles3D::Tileset::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
}


void Tiles3D::Tileset::report()
{
}

void Tiles3D::Tileset::resolveURIs(vsg::ref_ptr<const vsg::Options> options)
{
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

void Tiles3D::b3dm_FeatureTable::report()
{
    vsg::info("b3dm_FeatureTable { ");
    vsg::info("    RTC_CENTER ", RTC_CENTER.values);
    vsg::info("    BATCH_LENGTH ", BATCH_LENGTH);
    vsg::info("}");
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

void Tiles3D::i3dm_FeatureTable::report()
{
    vsg::info("i3dm_FeatureTable { ");
    vsg::info("    POSITION ", POSITION.values);
    vsg::info("    POSITION_QUANTIZED ", POSITION_QUANTIZED.values);
    vsg::info("    NORMAL_UP ", NORMAL_UP.values);
    vsg::info("    NORMAL_RIGHT ", NORMAL_RIGHT.values);
    vsg::info("    NORMAL_UP_OCT32P ", NORMAL_UP_OCT32P.values);
    vsg::info("    NORMAL_RIGHT_OCT32P ", NORMAL_RIGHT_OCT32P.values);
    vsg::info("    SCALE ", SCALE.values);
    vsg::info("    SCALE_NON_UNIFORM ", SCALE_NON_UNIFORM.values);
    vsg::info("    RTC_CENTER ", RTC_CENTER.values);
    vsg::info("    QUANTIZED_VOLUME_OFFSET ", QUANTIZED_VOLUME_OFFSET.values);
    vsg::info("    QUANTIZED_VOLUME_SCALE ", QUANTIZED_VOLUME_SCALE.values);
    vsg::info("    BATCH_ID ", BATCH_ID);
    vsg::info("    INSTANCES_LENGTH ", INSTANCES_LENGTH);
    vsg::info("}");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Batch
//
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
                        else vsg::info("Unable to convert to stringValue ", child);
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
                        else vsg::info("Unable to convert to floatValue ", child);
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
                        else vsg::info("Unable to convert to doubleValue ", child);
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
        if (componentType=="BYTE") object = vsg::byteArray::create(batchTable.buffer, byteOffset, 1, batchTable.length);
        else if (componentType=="UNSIGNED_BYTE") object = vsg::ubyteArray::create(batchTable.buffer, byteOffset, 1, batchTable.length);
        else if (componentType=="SHORT") object = vsg::shortArray::create(batchTable.buffer, byteOffset, 2, batchTable.length);
        else if (componentType=="UNSIGNED_SHORT") object = vsg::ushortArray::create(batchTable.buffer, byteOffset, 2, batchTable.length);
        else if (componentType=="INT") object = vsg::intArray::create(batchTable.buffer, byteOffset, 4, batchTable.length);
        else if (componentType=="UNSIGNED_INT") object = vsg::uintArray::create(batchTable.buffer, byteOffset, 4, batchTable.length);
        else if (componentType=="FLOAT") object = vsg::floatArray::create(batchTable.buffer, byteOffset, 4, batchTable.length);
        else if (componentType=="DOUBLE") object = vsg::doubleArray::create(batchTable.buffer, byteOffset, 8, batchTable.length);
        else vsg::warn("Unsupported Tiles3D::Batch SCALAR componentType = ", componentType);
    }
    else if (type == "VEC2")
    {
        if (componentType=="BYTE") object = vsg::bvec2Array::create(batchTable.buffer, byteOffset, 2, batchTable.length);
        else if (componentType=="UNSIGNED_BYTE") object = vsg::ubvec2Array::create(batchTable.buffer, byteOffset, 2, batchTable.length);
        else if (componentType=="SHORT") object = vsg::svec2Array::create(batchTable.buffer, byteOffset, 4, batchTable.length);
        else if (componentType=="UNSIGNED_SHORT") object = vsg::usvec2Array::create(batchTable.buffer, byteOffset, 4, batchTable.length);
        else if (componentType=="INT") object = vsg::ivec2Array::create(batchTable.buffer, byteOffset, 8, batchTable.length);
        else if (componentType=="UNSIGNED_INT") object = vsg::uivec2Array::create(batchTable.buffer, byteOffset, 8, batchTable.length);
        else if (componentType=="FLOAT") object = vsg::vec2Array::create(batchTable.buffer, byteOffset, 8, batchTable.length);
        else if (componentType=="DOUBLE") object = vsg::dvec2Array::create(batchTable.buffer, byteOffset, 16, batchTable.length);
        else vsg::warn("Unsupported Tiles3D::Batch VEC2 componentType = ", componentType);
    }
    else if (type == "VEC3")
    {
        if (componentType=="BYTE") object = vsg::bvec3Array::create(batchTable.buffer, byteOffset, 3, batchTable.length);
        else if (componentType=="UNSIGNED_BYTE") object = vsg::ubvec3Array::create(batchTable.buffer, byteOffset, 3, batchTable.length);
        else if (componentType=="SHORT") object = vsg::svec3Array::create(batchTable.buffer, byteOffset, 6, batchTable.length);
        else if (componentType=="UNSIGNED_SHORT") object = vsg::usvec3Array::create(batchTable.buffer, byteOffset, 6, batchTable.length);
        else if (componentType=="INT") object = vsg::ivec3Array::create(batchTable.buffer, byteOffset, 12, batchTable.length);
        else if (componentType=="UNSIGNED_INT") object = vsg::uivec3Array::create(batchTable.buffer, byteOffset, 12, batchTable.length);
        else if (componentType=="FLOAT") object = vsg::vec3Array::create(batchTable.buffer, byteOffset, 12, batchTable.length);
        else if (componentType=="DOUBLE") object = vsg::dvec3Array::create(batchTable.buffer, byteOffset, 16, batchTable.length);
        else vsg::warn("Unsupported Tiles3D::Batch VEC3 componentType = ", componentType);
    }
    else if (type == "VEC4")
    {
        if (componentType=="BYTE") object = vsg::bvec4Array::create(batchTable.buffer, byteOffset, 4, batchTable.length);
        else if (componentType=="UNSIGNED_BYTE") object = vsg::ubvec4Array::create(batchTable.buffer, byteOffset, 4, batchTable.length);
        else if (componentType=="SHORT") object = vsg::svec4Array::create(batchTable.buffer, byteOffset, 8, batchTable.length);
        else if (componentType=="UNSIGNED_SHORT") object = vsg::usvec4Array::create(batchTable.buffer, byteOffset, 8, batchTable.length);
        else if (componentType=="INT") object = vsg::ivec4Array::create(batchTable.buffer, byteOffset, 16, batchTable.length);
        else if (componentType=="UNSIGNED_INT") object = vsg::uivec4Array::create(batchTable.buffer, byteOffset, 16, batchTable.length);
        else if (componentType=="FLOAT") object = vsg::vec4Array::create(batchTable.buffer, byteOffset, 16, batchTable.length);
        else if (componentType=="DOUBLE") object = vsg::dvec4Array::create(batchTable.buffer, byteOffset, 32, batchTable.length);
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

void Tiles3D::BatchTable::report()
{
    struct PrintValues : public vsg::ConstVisitor
    {
        void apply(const vsg::stringValue& v) override
        {
            vsg::info("        ", v.value());
        }

        void apply(const vsg::floatValue& v) override
        {
            vsg::info("        ", v.value());
        }

        void apply(const vsg::doubleValue& v) override
        {
            vsg::info("        ", v.value());
        }

        void apply(const vsg::stringArray& strings) override
        {
            for(auto value : strings) vsg::info("        ", value);
        }

        void apply(const vsg::intArray& ints) override
        {
            for(auto value : ints) vsg::info("        ", value);
        }

        void apply(const vsg::doubleArray& doubles) override
        {
            for(auto value : doubles) vsg::info("        ", value);
        }
    } pv;

    for(auto& [name, batch] : batches)
    {
        vsg::info("batch ", name, " {");

        if (batch->object)
        {
            vsg::info("    object = ", batch->object);
            batch->object->accept(pv);
            vsg::info(" }");
        }
        else if (batch->objects)
        {
            vsg::info("    objects = ", batch->objects);
            for(auto& child : batch->objects->children)
            {
                vsg::info("        child = ", child);
            }
        }
        else
        {
            vsg::info("    byteOffset = ", batch->byteOffset);
            vsg::info("    componentType = ", batch->componentType);
            vsg::info("    type = ", batch->type);
        }

        vsg::info("}");
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

vsg::ref_ptr<vsg::Object> Tiles3D::read_json(std::istream& fin, vsg::ref_ptr<const vsg::Options> options, const vsg::Path&) const
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
    {
        featureTable = b3dm_FeatureTable::create();
        featureTable->binary = vsg::ubyteArray::create(header.featureTableBinaryByteLength);

        vsg::JSONParser parser;
        parser.buffer.resize(header.featureTableJSONByteLength);
        fin.read(parser.buffer.data(), header.featureTableJSONByteLength);
        fin.read(reinterpret_cast<char*>(featureTable->binary->dataPointer()), header.featureTableBinaryByteLength);

        parser.read_object(*featureTable);
    }

    vsg::ref_ptr<BatchTable> batchTable;
    if (header.batchTableJSONByteLength > 0)
    {
        vsg::JSONParser parser;
        parser.buffer.resize(header.batchTableJSONByteLength);
        fin.read(parser.buffer.data(), header.batchTableJSONByteLength);

        batchTable = BatchTable::create();
        parser.read_object(*batchTable);
   }

    vsg::ref_ptr<vsg::ubyteArray> batchTableBinary;
    if (header.batchTableBinaryLength > 0)
    {
        batchTableBinary = vsg::ubyteArray::create(header.batchTableBinaryLength);
        fin.read(reinterpret_cast<char*>(batchTableBinary->dataPointer()), header.batchTableBinaryLength);
    }

    if (featureTable && batchTable)
    {
        batchTable->length = featureTable->BATCH_LENGTH;
        batchTable->buffer = batchTableBinary;
        batchTable->convert();
    }

    if (vsg::value<bool>(false, gltf::report, options))
    {
        vsg::info("magic = ", header.magic);
        vsg::info("version = ", header.version);
        vsg::info("byteLength = ", header.byteLength);
        vsg::info("featureTableJSONByteLength = ", header.featureTableJSONByteLength);
        vsg::info("featureTableBinaryByteLength = ", header.featureTableBinaryByteLength);
        vsg::info("batchTableJSONByteLength = ", header.batchTableJSONByteLength);
        vsg::info("batchTableBinaryLength = ", header.batchTableBinaryLength);

        if (featureTable) featureTable->report();
        if (batchTable) batchTable->report();
    }


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
    vsg::warn("Tiles3D::read_cmpt(..).");

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
            vsg::info("   {", innerHeader.magic, ", ", innerHeader.version, ", ", innerHeader.byteLength, " }");

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
        vsg::info("magic = ", header.magic);
        vsg::info("version = ", header.version);
        vsg::info("byteLength = ", header.byteLength);
        vsg::info("tilesLength = ", header.tilesLength);

        vsg::info("innerHeaders.size() = ", innerHeaders.size());
        for(auto& innerHeader : innerHeaders)
        {
            vsg::info("   {", innerHeader.magic, ", ", innerHeader.version, ", ", innerHeader.byteLength, " }");
        }
    }


    vsg::ref_ptr<vsg::Object> result;

    return result;
}

vsg::ref_ptr<vsg::Object> Tiles3D::read_i3dm(std::istream& fin, vsg::ref_ptr<const vsg::Options> options, const vsg::Path&) const
{
    vsg::warn("Tiles3D::read_i3dm(..) not implemented yet.");
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

    vsg::ref_ptr<i3dm_FeatureTable> featureTable;
    if (header.featureTableJSONByteLength > 0)
    {
        vsg::info("Reading i3dm_FeatureTable");

        featureTable = i3dm_FeatureTable::create();
        featureTable->binary = vsg::ubyteArray::create(header.featureTableBinaryByteLength);

        vsg::JSONParser parser;
        parser.buffer.resize(header.featureTableJSONByteLength);
        fin.read(parser.buffer.data(), header.featureTableJSONByteLength);
        fin.read(reinterpret_cast<char*>(featureTable->binary->dataPointer()), header.featureTableBinaryByteLength);

        parser.read_object(*featureTable);

        vsg::info("finished Reading i3dm_FeatureTable");
    }

    vsg::ref_ptr<vsg::ubyteArray> featureTableBinary;
    if (header.featureTableBinaryByteLength > 0)
    {
        featureTableBinary = vsg::ubyteArray::create(header.featureTableBinaryByteLength);
        fin.read(reinterpret_cast<char*>(featureTableBinary->dataPointer()), header.featureTableBinaryByteLength);
    }
#if 0
    vsg::ref_ptr<BatchTable> batchTable;
    if (header.batchTableJSONByteLength > 0)
    {
        vsg::JSONParser parser;
        parser.buffer.resize(header.batchTableJSONByteLength);
        fin.read(parser.buffer.data(), header.batchTableJSONByteLength);

        batchTable = BatchTable::create();
        parser.read_object(*batchTable);
   }

    vsg::ref_ptr<vsg::ubyteArray> batchTableBinary;
    if (header.batchTableBinaryLength > 0)
    {
        batchTableBinary = vsg::ubyteArray::create(header.batchTableBinaryLength);
        fin.read(reinterpret_cast<char*>(batchTableBinary->dataPointer()), header.batchTableBinaryLength);
    }

    if (featureTable && batchTable)
    {
        batchTable->length = featureTable->BATCH_LENGTH;
        batchTable->buffer = batchTableBinary;
        batchTable->convert();
    }
#endif

    if (vsg::value<bool>(false, gltf::report, options))
    {
        vsg::info("magic = ", header.magic);
        vsg::info("version = ", header.version);
        vsg::info("byteLength = ", header.byteLength);
        vsg::info("featureTableJSONByteLength = ", header.featureTableJSONByteLength);
        vsg::info("featureTableBinaryByteLength = ", header.featureTableBinaryByteLength);
        vsg::info("batchTableJSONByteLength = ", header.batchTableJSONByteLength);
        vsg::info("batchTableBinaryLength = ", header.batchTableBinaryLength);

        if (featureTable) featureTable->report();
//        if (batchTable) batchTable->report();
    }

    vsg::ref_ptr<vsg::Object> result;

    return result;
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

    if (ext==".json") return read_json(fin, options);
    else if (ext==".b3dm") return read_b3dm(fin, options);
    else if (ext==".cmpt") return read_cmpt(fin, options);
    else if (ext==".i3dm") return read_i3dm(fin, options);
    else if (ext==".pnts") return read_pnts(fin, options);
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
