/* <editor-fold desc="MIT License">

Copyright(c) 2025 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgXchange/3d-tiles.h>

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
// FeatureTable
//
void Tiles3D::FeatureTable::read_array(vsg::JSONParser& parser, const std::string_view& property)
{
    if (property=="RTC_CENTER") parser.read_array(RTC_CENTER);
    else parser.warning();
}

void Tiles3D::FeatureTable::read_number(vsg::JSONParser& parser, const std::string_view& property, std::istream& input)
{
    if (property=="BATCH_LENGTH") input >> BATCH_LENGTH;
    else parser.warning();
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

    return result;
}

vsg::ref_ptr<vsg::Object> Tiles3D::read_b3dm(std::istream& fin, vsg::ref_ptr<const vsg::Options> options, const vsg::Path& filename) const
{
    vsg::info("Tiles3D::read_b3dm()");

    // https://github.com/CesiumGS/3d-tiles/tree/1.0/specification/TileFormats/Batched3DModel
    fin.seekg(0);

    struct Header
    {
        char magic[4] = {0,0,0,0};
        uint32_t version = 0;
        uint32_t byteLength = 0;
        uint32_t featureTableJSONByteLength = 0;
        uint32_t featureTableBinaryByteLength = 0;
        uint32_t batchTableJSONByteLength = 0;
        uint32_t batchTabelBinaryLength = 0;
    };

    Header header;
    fin.read(reinterpret_cast<char*>(&header), sizeof(Header));

    if (!fin.good())
    {
        vsg::warn("IO error reading bd3m file.");
        return {};
    }

    vsg::info("magic = ", header.magic);
    vsg::info("version = ", header.version);
    vsg::info("byteLength = ", header.byteLength);
    vsg::info("featureTableJSONByteLength = ", header.featureTableJSONByteLength);
    vsg::info("featureTableBinaryByteLength = ", header.featureTableBinaryByteLength);
    vsg::info("batchTableJSONByteLength = ", header.batchTableJSONByteLength);
    vsg::info("batchTabelBinaryLength = ", header.batchTabelBinaryLength);

    if (strncmp(header.magic, "b3dm", 4) != 0)
    {
        vsg::warn("magic number not b3dm");
        return {};
    }

    // Feature table
    // Batch table
    // Binary glTF

    vsg::ref_ptr<FeatureTable> featureTable;
    if (header.featureTableJSONByteLength > 0)
    {
        vsg::JSONParser parser;
        parser.buffer.resize(header.featureTableJSONByteLength);
        fin.read(parser.buffer.data(), header.featureTableJSONByteLength);

        featureTable = FeatureTable::create();
        parser.read_object(*featureTable);

        vsg::info("read featureTableJSON = ", parser.buffer);
        vsg::info("read featureTable = ", featureTable);
        vsg::info("read featureTable->BATCH_LENGTH = ", featureTable->BATCH_LENGTH);
        vsg::info("read featureTable->RTC_CENTER = ", featureTable->RTC_CENTER.values.size());
    }

    if (header.batchTableJSONByteLength > 0)
    {
        std::string batchTableJSON;
        batchTableJSON.resize(header.batchTableJSONByteLength);
        fin.read(batchTableJSON.data(), header.batchTableJSONByteLength);
        vsg::info("read batchTableJSON = ", batchTableJSON);
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

vsg::ref_ptr<vsg::Object> Tiles3D::read_cmpt(std::istream&, vsg::ref_ptr<const vsg::Options>, const vsg::Path& filename) const
{
    vsg::warn("Tiles3D::read_cmpt(..) not implemented yet.");
    return {};
}

vsg::ref_ptr<vsg::Object> Tiles3D::read_i3dm(std::istream&, vsg::ref_ptr<const vsg::Options>, const vsg::Path& filename) const
{
    vsg::warn("Tiles3D::read_i3dm(..) not implemented yet.");
    return {};
}

vsg::ref_ptr<vsg::Object> Tiles3D::read_pnts(std::istream&, vsg::ref_ptr<const vsg::Options>, const vsg::Path& filename) const
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
