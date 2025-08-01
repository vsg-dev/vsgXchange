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
#include <vsg/nodes/InstanceNode.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/threading/OperationThreads.h>
#include <vsg/ui/UIEvent.h>
#include <vsg/utils/CommandLine.h>
#include <vsg/utils/ComputeBounds.h>

#include <fstream>
#include <iostream>
#include <stack>

using namespace vsgXchange;

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// i3dm_FeatureTable
//
void Tiles3D::i3dm_FeatureTable::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "POSITION")
        parser.read_array(POSITION);
    else if (property == "POSITION_QUANTIZED")
        parser.read_array(POSITION_QUANTIZED);
    else if (property == "NORMAL_UP")
        parser.read_array(NORMAL_UP);
    else if (property == "NORMAL_RIGHT")
        parser.read_array(NORMAL_RIGHT);
    else if (property == "NORMAL_UP_OCT32P")
        parser.read_array(NORMAL_UP_OCT32P);
    else if (property == "NORMAL_RIGHT_OCT32P")
        parser.read_array(NORMAL_RIGHT_OCT32P);
    else if (property == "SCALE")
        parser.read_array(SCALE);
    else if (property == "SCALE_NON_UNIFORM")
        parser.read_array(SCALE_NON_UNIFORM);
    else if (property == "RTC_CENTER")
        parser.read_array(RTC_CENTER);
    else if (property == "QUANTIZED_VOLUME_OFFSET")
        parser.read_array(QUANTIZED_VOLUME_OFFSET);
    else if (property == "QUANTIZED_VOLUME_SCALE")
        parser.read_array(QUANTIZED_VOLUME_SCALE);
    else if (property == "BATCH_ID")
        parser.read_array(BATCH_ID);
    else
        parser.warning();
}

void Tiles3D::i3dm_FeatureTable::read_object(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property == "POSITION") { parser.read_object(POSITION); }
    else if (property == "POSITION_QUANTIZED")
        parser.read_object(POSITION_QUANTIZED);
    else if (property == "NORMAL_UP")
        parser.read_object(NORMAL_UP);
    else if (property == "NORMAL_RIGHT")
        parser.read_object(NORMAL_RIGHT);
    else if (property == "NORMAL_UP_OCT32P")
        parser.read_object(NORMAL_UP_OCT32P);
    else if (property == "NORMAL_RIGHT_OCT32P")
        parser.read_object(NORMAL_RIGHT_OCT32P);
    else if (property == "SCALE")
        parser.read_object(SCALE);
    else if (property == "SCALE_NON_UNIFORM")
        parser.read_object(SCALE_NON_UNIFORM);
    else if (property == "RTC_CENTER")
        parser.read_object(RTC_CENTER);
    else if (property == "QUANTIZED_VOLUME_OFFSET")
        parser.read_object(QUANTIZED_VOLUME_OFFSET);
    else if (property == "QUANTIZED_VOLUME_SCALE")
        parser.read_object(QUANTIZED_VOLUME_SCALE);
    else if (property == "BATCH_ID")
        parser.read_object(BATCH_ID);
    else
        parser.warning();
}

void Tiles3D::i3dm_FeatureTable::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property == "INSTANCES_LENGTH")
        input >> INSTANCES_LENGTH;
    else
        parser.warning();
}

void Tiles3D::i3dm_FeatureTable::read_bool(vsg::JSONParser& parser, const std::string_view& property, bool value)
{
    if (property == "EAST_NORTH_UP")
        EAST_NORTH_UP = value;
    else
        parser.warning();
}

void Tiles3D::i3dm_FeatureTable::convert()
{
    if (INSTANCES_LENGTH == 0 || !binary) return;

    POSITION.assign(*binary, 3 * INSTANCES_LENGTH);
    POSITION_QUANTIZED.assign(*binary, 3 * INSTANCES_LENGTH);
    NORMAL_UP.assign(*binary, 3 * INSTANCES_LENGTH);
    NORMAL_RIGHT.assign(*binary, 3 * INSTANCES_LENGTH);
    NORMAL_UP_OCT32P.assign(*binary, 2 * INSTANCES_LENGTH);
    NORMAL_RIGHT_OCT32P.assign(*binary, 2 * INSTANCES_LENGTH);
    SCALE.assign(*binary, INSTANCES_LENGTH);
    SCALE_NON_UNIFORM.assign(*binary, 3 * INSTANCES_LENGTH);
    BATCH_ID.assign(*binary, INSTANCES_LENGTH);
    RTC_CENTER.assign(*binary, 3);
    QUANTIZED_VOLUME_OFFSET.assign(*binary, 3);
    QUANTIZED_VOLUME_SCALE.assign(*binary, 3);
}

void Tiles3D::i3dm_FeatureTable::report(vsg::LogOutput& output)
{
    output("i3dm_FeatureTable { ");
    if (POSITION) output("    POSITION ", POSITION.values);
    if (POSITION_QUANTIZED) output("    POSITION_QUANTIZED ", POSITION_QUANTIZED.values);
    if (NORMAL_UP) output("    NORMAL_UP ", NORMAL_UP.values);
    if (NORMAL_RIGHT) output("    NORMAL_RIGHT ", NORMAL_RIGHT.values);
    if (NORMAL_UP_OCT32P) output("    NORMAL_UP_OCT32P ", NORMAL_UP_OCT32P.values);
    if (NORMAL_RIGHT_OCT32P) output("    NORMAL_RIGHT_OCT32P ", NORMAL_RIGHT_OCT32P.values);
    if (SCALE) output("    SCALE ", SCALE.values);
    if (SCALE_NON_UNIFORM) output("    SCALE_NON_UNIFORM ", SCALE_NON_UNIFORM.values);
    if (RTC_CENTER) output("    RTC_CENTER ", RTC_CENTER.values);
    if (BATCH_ID) output("    BATCH_ID ", BATCH_ID.values);
    if (QUANTIZED_VOLUME_OFFSET) output("    QUANTIZED_VOLUME_OFFSET ", QUANTIZED_VOLUME_OFFSET.values);
    if (QUANTIZED_VOLUME_SCALE) output("    QUANTIZED_VOLUME_SCALE ", QUANTIZED_VOLUME_SCALE.values);
    output("    INSTANCES_LENGTH ", INSTANCES_LENGTH);
    output("    EAST_NORTH_UP ", EAST_NORTH_UP);
    output("}");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// read_i3dm
//
vsg::ref_ptr<vsg::Object> Tiles3D::read_i3dm(std::istream& fin, vsg::ref_ptr<const vsg::Options> options, const vsg::Path& filename) const
{
    fin.seekg(0);

    // https://github.com/CesiumGS/3d-tiles/blob/main/specification/TileFormats/Instanced3DModel/README.adoc
    struct Header
    {
        char magic[4] = {0, 0, 0, 0};
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
        vsg::warn("magic number not i3dm, magic = ", int(header.magic[0]), ", ", int(header.magic[1]), ", ", int(header.magic[2]), ", ", int(header.magic[3]), ", ");
        return {};
    }

    // Feature table
    // Batch table
    // Binary glTF

    vsg::ref_ptr<i3dm_FeatureTable> featureTable = i3dm_FeatureTable::create();
    if (header.featureTableJSONByteLength > 0)
    {
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
        featureTable->convert();
    }

    vsg::ref_ptr<BatchTable> batchTable;
    if (header.batchTableJSONByteLength > 0)
    {
        batchTable = BatchTable::create();

        vsg::JSONParser parser;
        parser.buffer.resize(header.batchTableJSONByteLength);
        fin.read(parser.buffer.data(), header.batchTableJSONByteLength);

        if (header.batchTableBinaryLength > 0)
        {
            batchTable->binary = vsg::ubyteArray::create(header.batchTableBinaryLength);
            fin.read(reinterpret_cast<char*>(batchTable->binary->dataPointer()), header.batchTableBinaryLength);
        }

        // vsg::info("BatchTable JSON = ", parser.buffer);
        // vsg::info("BatchTable batchTableBinaryLength = ", header.batchTableBinaryLength);

        parser.read_object(*batchTable);

        batchTable->length = featureTable->INSTANCES_LENGTH;
        batchTable->convert();
    }

    if (vsg::value<bool>(false, gltf::report, options))
    {
        vsg::LogOutput output;

        output("Tiles3D::read_i3dm(..)");
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

    vsg::dvec3 quantizeOffset(0.0, 0, 0.0);
    vsg::dvec3 quantizeScale(1.0, 1.0, 1.0);
    if (featureTable->QUANTIZED_VOLUME_OFFSET && featureTable->QUANTIZED_VOLUME_OFFSET.values.size() == 3)
    {
        const auto& values = featureTable->QUANTIZED_VOLUME_OFFSET.values;
        quantizeOffset.set(values[0], values[1], values[2]);
    }

    if (featureTable->QUANTIZED_VOLUME_SCALE && featureTable->QUANTIZED_VOLUME_SCALE.values.size() == 3)
    {
        const auto& values = featureTable->QUANTIZED_VOLUME_SCALE.values;
        quantizeScale.set(values[0], values[1], values[2]);
        quantizeScale /= 65535.0;
    }

    auto convert_oct32 = [](uint16_t x, uint16_t y) -> vsg::dvec3 {
        const double oct32_multiplier = (2.0 / 65535.0);
        vsg::dvec2 e(static_cast<double>(x) * oct32_multiplier - 1.0, static_cast<double>(y) * oct32_multiplier - 1.0);
        vsg::dvec3 v(e.x, e.y, 1.0 - std::abs(e.x) - std::abs(e.y));
        if (v.z < 0.0)
        {
            v.x = (1.0 - std::abs(e.y)) * std::copysignl(1.0, e.x);
            v.y = (1.0 - std::abs(e.x)) * std::copysignl(1.0, e.y);
        }
        return vsg::normalize(v);
    };

    vsg::dvec3 rtc_center;
    if (featureTable->RTC_CENTER && featureTable->RTC_CENTER.values.size() == 3)
    {
        rtc_center.x = featureTable->RTC_CENTER.values[0];
        rtc_center.y = featureTable->RTC_CENTER.values[1];
        rtc_center.z = featureTable->RTC_CENTER.values[2];
    }

    auto getTransformComponents = [&](uint32_t i, vsg::dvec3& translation, vsg::dquat& rotation, vsg::dvec3& scale) -> void {
        if (featureTable->POSITION && i * 3 < featureTable->POSITION.values.size())
        {
            const auto& values = featureTable->POSITION.values;
            translation.set(values[i * 3 + 0], values[i * 3 + 1], values[i * 3 + 2]);
        }
        else if (featureTable->POSITION_QUANTIZED && i * 3 < featureTable->POSITION_QUANTIZED.values.size())
        {
            const auto& values = featureTable->POSITION_QUANTIZED.values;
            vsg::dvec3 quantizedPosition(static_cast<double>(values[i * 3 + 0]), static_cast<double>(values[i * 3 + 1]), static_cast<double>(values[i * 3 + 2]));
            translation = quantizeOffset + quantizedPosition * quantizeScale;
        }

        vsg::dvec3 normal_up(0.0, 0.0, 1.0);
        vsg::dvec3 normal_right(1.0, 0.0, 0.0);

        if (featureTable->EAST_NORTH_UP)
        {
            const double epsilon = 1e-7;

            normal_up = vsg::normalize(translation);
            normal_right.set(-translation.y, translation.x, 0.0);
            double len = vsg::length(normal_right);
            if (len > epsilon)
            {
                normal_right /= len;
            }
            else
            {
                normal_right.set(0.0, 1.0, 0.0);
            }
        }

        if (featureTable->NORMAL_UP && i * 3 < featureTable->NORMAL_UP.values.size())
        {
            const auto& values = featureTable->NORMAL_UP.values;
            normal_up.set(values[i * 3 + 0], values[i * 3 + 1], values[i * 3 + 2]);
        }
        else if (featureTable->NORMAL_UP_OCT32P && i * 2 < featureTable->NORMAL_UP_OCT32P.values.size())
        {
            const auto& values = featureTable->NORMAL_UP_OCT32P.values;
            normal_up = convert_oct32(values[i * 2 + 0], values[i * 2 + 1]);
        }

        if (featureTable->NORMAL_RIGHT && i * 3 < featureTable->NORMAL_RIGHT.values.size())
        {
            const auto& values = featureTable->NORMAL_RIGHT.values;
            normal_right.set(values[i * 3 + 0], values[i * 3 + 1], values[i * 3 + 2]);
        }
        else if (featureTable->NORMAL_RIGHT_OCT32P && i * 2 < featureTable->NORMAL_RIGHT_OCT32P.values.size())
        {
            const auto& values = featureTable->NORMAL_RIGHT_OCT32P.values;
            normal_right = convert_oct32(values[i * 2 + 0], values[i * 2 + 1]);
        }

        if (featureTable->SCALE && i < featureTable->SCALE.values.size())
        {
            double value = featureTable->SCALE.values[i];
            scale.set(value, value, value);
        }
        else if (featureTable->SCALE_NON_UNIFORM && i * 3 < featureTable->SCALE_NON_UNIFORM.values.size())
        {
            const auto& values = featureTable->SCALE_NON_UNIFORM.values;
            scale.set(values[i * 3 + 0], values[i * 3 + 1], values[i * 3 + 2]);
        }
        else
        {
            scale.set(1.0, 1.0, 1.0);
        }

        vsg::dvec3 normal_forward = vsg::cross(normal_up, normal_right);

        vsg::dmat4 rot_matrix(normal_right.x, normal_right.y, normal_right.z, 0.0,
                              normal_forward.x, normal_forward.y, normal_forward.z, 0.0,
                              normal_up.x, normal_up.y, normal_up.z, 0.0,
                              0.0, 0.0, 0.0, 1.0);

        vsg::dvec3 temp_translation, temp_scale;
        vsg::decompose(rot_matrix, temp_translation, rotation, temp_scale);
    };

    bool gpuInstancing = vsg::value<bool>(true, Tiles3D::instancing, options);

    auto opt = vsg::clone(options);
    opt->extensionHint = ".glb";
    opt->formatCoordinateConventions[".glb"] = vsg::CoordinateConvention::Y_UP;
    if (gpuInstancing)
    {
        opt->instanceNodeHint = vsg::Options::INSTANCE_TRANSLATIONS | vsg::Options::INSTANCE_ROTATIONS | vsg::Options::INSTANCE_SCALES;
    }

    vsg::ref_ptr<vsg::Node> model;
    if (header.gltfFormat == 0)
    {
        std::string uri;
        uri.resize(size_of_gltfField);

        fin.read(uri.data(), size_of_gltfField);

        // trim trailing null characters
        while (uri.back() <= 32) uri.pop_back();

        // load model
        model = vsg::read_cast<vsg::Node>(uri, opt);
    }
    else
    {
        std::string binary;
        binary.resize(size_of_gltfField);
        fin.read(binary.data(), size_of_gltfField);

        vsg::mem_stream binary_fin(reinterpret_cast<uint8_t*>(binary.data()), binary.size());

        model = vsg::read_cast<vsg::Node>(binary_fin, opt);
    }

    if (!model) return {};

    // if required decorate the model to provide multiple instances, or provide the global translation.
    if (gpuInstancing)
    {
        // compute the centre of all positions
        vsg::dvec3 instance_center;

        if (featureTable->POSITION)
        {
            for (size_t i = 0; i * 3 < featureTable->POSITION.values.size(); ++i)
            {
                const auto& values = featureTable->POSITION.values;
                instance_center += vsg::dvec3(values[i * 3 + 0], values[i * 3 + 1], values[i * 3 + 2]);
            }
            instance_center /= static_cast<double>(featureTable->POSITION.values.size() / 3);

            rtc_center += instance_center;
        }
        else if (featureTable->POSITION_QUANTIZED)
        {
            for (size_t i = 0; i * 3 < featureTable->POSITION_QUANTIZED.values.size(); ++i)
            {
                const auto& values = featureTable->POSITION_QUANTIZED.values;
                vsg::dvec3 quantizedPosition(static_cast<double>(values[i * 3 + 0]), static_cast<double>(values[i * 3 + 1]), static_cast<double>(values[i * 3 + 2]));
                instance_center += quantizeOffset + quantizedPosition * quantizeScale;
            }

            instance_center /= static_cast<double>(featureTable->POSITION_QUANTIZED.values.size() / 3);

            rtc_center += instance_center;
        }

        auto translations = vsg::vec3Array::create(featureTable->INSTANCES_LENGTH);
        auto rotations = vsg::quatArray::create(featureTable->INSTANCES_LENGTH);
        auto scales = vsg::vec3Array::create(featureTable->INSTANCES_LENGTH);

        auto instanceNode = vsg::InstanceNode::create();
        instanceNode->firstInstance = 0;
        instanceNode->instanceCount = featureTable->INSTANCES_LENGTH;
        instanceNode->setTranslations(translations);
        instanceNode->setRotations(rotations);
        instanceNode->setScales(scales);
        instanceNode->child = model;

        for (uint32_t i = 0; i < featureTable->INSTANCES_LENGTH; ++i)
        {
            vsg::dvec3 translation, scale;
            vsg::dquat rotation;
            getTransformComponents(i, translation, rotation, scale);

            translations->set(i, vsg::vec3(translation - instance_center));
            rotations->set(i, vsg::quat(rotation));
            scales->set(i, vsg::vec3(scale));
        }

        if (rtc_center != vsg::dvec3())
        {
            auto transform = vsg::MatrixTransform::create();
            transform->addChild(instanceNode);
            transform->matrix = vsg::translate(rtc_center);
            model = transform;
        }
        else
        {
            model = instanceNode;
        }
    }
    else
    {
        auto group = vsg::Group::create();
        for (uint32_t i = 0; i < featureTable->INSTANCES_LENGTH; ++i)
        {
            vsg::dvec3 translation, scale;
            vsg::dquat rotation;
            getTransformComponents(i, translation, rotation, scale);

            auto transform = vsg::MatrixTransform::create();
            transform->matrix = vsg::translate(rtc_center + translation) * vsg::rotate(rotation) * vsg::scale(scale);
            transform->addChild(model);

            group->addChild(transform);
        }

        if (group->children.size() == 1)
            model = group->children[0];
        else if (!group->children.empty())
            model = group;
    }

    if (filename) model->setValue("i3dm", filename);

    return model;
}
