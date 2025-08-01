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

// b3dm_FeatureTable
//
void Tiles3D::b3dm_FeatureTable::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "RTC_CENTER")
        parser.read_array(RTC_CENTER);
    else
        parser.warning();
}

void Tiles3D::b3dm_FeatureTable::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "RTC_CENTER")
        parser.read_object(RTC_CENTER);
    else
        parser.warning();
}

void Tiles3D::b3dm_FeatureTable::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "BATCH_LENGTH")
        input >> BATCH_LENGTH;
    else
        parser.warning();
}

void Tiles3D::b3dm_FeatureTable::convert()
{
    if (!binary) return;

    RTC_CENTER.assign(*binary, 3);
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
// read_b3dm
//
vsg::ref_ptr<vsg::Object> Tiles3D::read_b3dm(std::istream& fin, vsg::ref_ptr<const vsg::Options> options, const vsg::Path& filename) const
{

    fin.seekg(0);

    // https://github.com/CesiumGS/3d-tiles/tree/1.0/specification/TileFormats/Batched3DModel
    struct Header
    {
        char magic[4] = {0, 0, 0, 0};
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

        if (header.batchTableBinaryLength > 0)
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

        output("Tiles3D::read_b3dm()");
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

    uint32_t size_of_feature_and_batch_tables = header.featureTableJSONByteLength + header.featureTableBinaryByteLength + header.batchTableJSONByteLength + header.batchTableBinaryLength;
    uint32_t size_of_gltfField = header.byteLength - sizeof(Header) - size_of_feature_and_batch_tables;

    // TODO: need to modify glTF loader to handle batched value assessor.
    // https://github.com/CesiumGS/3d-tiles/tree/1.0/specification/TileFormats/Batched3DModel#batch-table

    std::string binary;
    binary.resize(size_of_gltfField);
    fin.read(binary.data(), size_of_gltfField);

    vsg::mem_stream binary_fin(reinterpret_cast<uint8_t*>(binary.data()), binary.size());

    auto opt = vsg::clone(options);
    opt->extensionHint = ".glb";

    auto model = vsg::read_cast<vsg::Node>(binary_fin, opt);

    if (featureTable && featureTable->RTC_CENTER && featureTable->RTC_CENTER.values.size() == 3)
    {
        vsg::dvec3 rtc_center;
        rtc_center.x = featureTable->RTC_CENTER.values[0];
        rtc_center.y = featureTable->RTC_CENTER.values[1];
        rtc_center.z = featureTable->RTC_CENTER.values[2];

        auto transform = vsg::MatrixTransform::create();
        transform->matrix = vsg::translate(rtc_center);
        transform->addChild(model);

        model = transform;
    }

    if (model && filename) model->setValue("b3dm", filename);

    return model;
}
