/* <editor-fold desc="MIT License">

Copyright(c) 2021 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Array2D.h>
#include <vsg/core/ConstVisitor.h>
#include <vsg/core/Visitor.h>

#include <vsgXchange/gdal.h>

#include <cstring>
#include <functional>
#include <iostream>

using namespace vsgXchange;

static bool s_GDAL_initialized = false;
bool vsgXchange::initGDAL()
{
    if (!s_GDAL_initialized)
    {
        s_GDAL_initialized = true;
        GDALAllRegister();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        return true;
    }
    else
    {
        return false;
    }
}

bool vsgXchange::compatibleDatasetProjections(const GDALDataset& lhs, const GDALDataset& rhs)
{
    if (&lhs == &rhs) return true;

    const auto lhs_projectionRef = const_cast<GDALDataset&>(lhs).GetProjectionRef();
    const auto rhs_projectionRef = const_cast<GDALDataset&>(rhs).GetProjectionRef();

    // if pointers are the same then they are compatible
    if (lhs_projectionRef == rhs_projectionRef) return true;

    // if one of the pointers is NULL then they are incompatible
    if (!lhs_projectionRef || !rhs_projectionRef) return false;

    // check if the OGRSpatialReference are the same
    return (std::strcmp(lhs_projectionRef, rhs_projectionRef) == 0);
}

bool vsgXchange::compatibleDatasetProjectionsTransformAndSizes(const GDALDataset& lhs, const GDALDataset& rhs)
{
    if (!compatibleDatasetProjections(lhs, rhs)) return false;

    auto& non_const_lhs = const_cast<GDALDataset&>(lhs);
    auto& non_const_rhs = const_cast<GDALDataset&>(rhs);

    if (non_const_lhs.GetRasterXSize() != non_const_rhs.GetRasterXSize() || non_const_lhs.GetRasterYSize() != non_const_rhs.GetRasterYSize())
    {
        return false;
    }

    double lhs_GeoTransform[6];
    double rhs_GeoTransform[6];

    int numberWithValidTransforms = 0;
    if (non_const_lhs.GetGeoTransform(lhs_GeoTransform) == CE_None) ++numberWithValidTransforms;
    if (non_const_rhs.GetGeoTransform(rhs_GeoTransform) == CE_None) ++numberWithValidTransforms;

    // if neither have transforms mark as compatible
    if (numberWithValidTransforms == 0) return true;

    // only one has a transform so must be incompatible
    if (numberWithValidTransforms == 1) return false;

    for (int i = 0; i < 6; ++i)
    {
        if (lhs_GeoTransform[i] != rhs_GeoTransform[i])
        {
            return false;
        }
    }
    return true;
}

template<typename T>
T default_value(double scale)
{
    if (std::numeric_limits<T>::is_integer)
    {
        if (scale < 0.0)
            return std::numeric_limits<T>::min();
        else if (scale > 0.0)
            return std::numeric_limits<T>::max();
        return static_cast<T>(0);
    }
    else
    {
        return static_cast<T>(scale);
    }
}

template<typename T>
vsg::t_vec2<T> default_vec2(const vsg::dvec4& value)
{
    return vsg::t_vec2<T>(default_value<T>(value[0]), default_value<T>(value[1]));
}

template<typename T>
vsg::t_vec3<T> default_vec3(const vsg::dvec4& value)
{
    return vsg::t_vec3<T>(default_value<T>(value[0]), default_value<T>(value[1]), default_value<T>(value[2]));
}

template<typename T>
vsg::t_vec4<T> default_vec4(const vsg::dvec4& value)
{
    return vsg::t_vec4<T>(default_value<T>(value[0]), default_value<T>(value[1]), default_value<T>(value[2]), default_value<T>(value[3]));
}

vsg::ref_ptr<vsg::Data> vsgXchange::createImage2D(int width, int height, int numComponents, GDALDataType dataType, vsg::dvec4 def)
{
    using TypeComponents = std::pair<GDALDataType, int>;
    using CreateFunction = std::function<vsg::ref_ptr<vsg::Data>(uint32_t w, uint32_t h, vsg::dvec4 def)>;

    std::map<TypeComponents, CreateFunction> createMap;
    createMap[TypeComponents(GDT_Byte, 1)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::ubyteArray2D::create(w, h, default_value<uint8_t>(d[0]), vsg::Data::Properties{VK_FORMAT_R8_UNORM}); };
    createMap[TypeComponents(GDT_UInt16, 1)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::ushortArray2D::create(w, h, default_value<uint16_t>(d[0]), vsg::Data::Properties{VK_FORMAT_R16_UNORM}); };
    createMap[TypeComponents(GDT_Int16, 1)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::shortArray2D::create(w, h, default_value<int16_t>(d[0]), vsg::Data::Properties{VK_FORMAT_R16_SNORM}); };
    createMap[TypeComponents(GDT_UInt32, 1)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::uintArray2D::create(w, h, default_value<uint32_t>(d[0]), vsg::Data::Properties{VK_FORMAT_R32_UINT}); };
    createMap[TypeComponents(GDT_Int32, 1)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::intArray2D::create(w, h, default_value<int32_t>(d[0]), vsg::Data::Properties{VK_FORMAT_R32_SINT}); };
    createMap[TypeComponents(GDT_Float32, 1)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::floatArray2D::create(w, h, default_value<float>(d[0]), vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}); };
    createMap[TypeComponents(GDT_Float64, 1)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::doubleArray2D::create(w, h, default_value<double>(d[0]), vsg::Data::Properties{VK_FORMAT_R64_SFLOAT}); };

    createMap[TypeComponents(GDT_Byte, 2)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::ubvec2Array2D::create(w, h, default_vec2<uint8_t>(d), vsg::Data::Properties{VK_FORMAT_R8G8_UNORM}); };
    createMap[TypeComponents(GDT_UInt16, 2)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::usvec2Array2D::create(w, h, default_vec2<uint16_t>(d), vsg::Data::Properties{VK_FORMAT_R16G16_UNORM}); };
    createMap[TypeComponents(GDT_Int16, 2)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::svec2Array2D::create(w, h, default_vec2<int16_t>(d), vsg::Data::Properties{VK_FORMAT_R16G16_SNORM}); };
    createMap[TypeComponents(GDT_UInt32, 2)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::uivec2Array2D::create(w, h, default_vec2<uint32_t>(d), vsg::Data::Properties{VK_FORMAT_R32G32_UINT}); };
    createMap[TypeComponents(GDT_Int32, 2)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::ivec2Array2D::create(w, h, default_vec2<int32_t>(d), vsg::Data::Properties{VK_FORMAT_R32G32_SINT}); };
    createMap[TypeComponents(GDT_Float32, 2)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::vec2Array2D::create(w, h, default_vec2<float>(d), vsg::Data::Properties{VK_FORMAT_R32G32_SFLOAT}); };
    createMap[TypeComponents(GDT_Float64, 2)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::dvec2Array2D::create(w, h, default_vec2<double>(d), vsg::Data::Properties{VK_FORMAT_R64G64_SFLOAT}); };

    createMap[TypeComponents(GDT_Byte, 3)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::ubvec3Array2D::create(w, h, default_vec3<uint8_t>(d), vsg::Data::Properties{VK_FORMAT_R8G8B8_UNORM}); };
    createMap[TypeComponents(GDT_UInt16, 3)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::usvec3Array2D::create(w, h, default_vec3<uint16_t>(d), vsg::Data::Properties{VK_FORMAT_R16G16B16_UNORM}); };
    createMap[TypeComponents(GDT_Int16, 3)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::svec3Array2D::create(w, h, default_vec3<int16_t>(d), vsg::Data::Properties{VK_FORMAT_R16G16B16_SNORM}); };
    createMap[TypeComponents(GDT_UInt32, 3)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::uivec3Array2D::create(w, h, default_vec3<uint32_t>(d), vsg::Data::Properties{VK_FORMAT_R32G32B32_UINT}); };
    createMap[TypeComponents(GDT_Int32, 3)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::ivec3Array2D::create(w, h, default_vec3<int32_t>(d), vsg::Data::Properties{VK_FORMAT_R32G32B32_SINT}); };
    createMap[TypeComponents(GDT_Float32, 3)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::vec3Array2D::create(w, h, default_vec3<float>(d), vsg::Data::Properties{VK_FORMAT_R32G32B32_SFLOAT}); };
    createMap[TypeComponents(GDT_Float64, 3)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::dvec3Array2D::create(w, h, default_vec3<double>(d), vsg::Data::Properties{VK_FORMAT_R64G64B64_SFLOAT}); };

    createMap[TypeComponents(GDT_Byte, 4)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::ubvec4Array2D::create(w, h, default_vec4<uint8_t>(d), vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}); };
    createMap[TypeComponents(GDT_UInt16, 4)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::usvec4Array2D::create(w, h, default_vec4<uint16_t>(d), vsg::Data::Properties{VK_FORMAT_R16G16B16A16_UNORM}); };
    createMap[TypeComponents(GDT_Int16, 4)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::svec4Array2D::create(w, h, default_vec4<int16_t>(d), vsg::Data::Properties{VK_FORMAT_R16G16B16A16_SNORM}); };
    createMap[TypeComponents(GDT_UInt32, 4)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::uivec4Array2D::create(w, h, default_vec4<uint32_t>(d), vsg::Data::Properties{VK_FORMAT_R32G32B32A32_UINT}); };
    createMap[TypeComponents(GDT_Int32, 4)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::ivec4Array2D::create(w, h, default_vec4<int32_t>(d), vsg::Data::Properties{VK_FORMAT_R32G32B32A32_SINT}); };
    createMap[TypeComponents(GDT_Float32, 4)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::vec4Array2D::create(w, h, default_vec4<float>(d), vsg::Data::Properties{VK_FORMAT_R32G32B32A32_SFLOAT}); };
    createMap[TypeComponents(GDT_Float64, 4)] = [](uint32_t w, uint32_t h, vsg::dvec4 d) { return vsg::dvec4Array2D::create(w, h, default_vec4<double>(d), vsg::Data::Properties{VK_FORMAT_R64G64B64A64_SFLOAT}); };

    auto itr = createMap.find(TypeComponents(dataType, numComponents));
    if (itr == createMap.end())
    {
        return {};
    }

    return (itr->second)(width, height, def);
}

bool vsgXchange::copyRasterBandToImage(GDALRasterBand& band, vsg::Data& image, int component)
{
    if (image.width() != static_cast<uint32_t>(band.GetXSize()) || image.height() != static_cast<uint32_t>(band.GetYSize()))
    {
        return false;
    }

    int dataSize = 0;
    switch (band.GetRasterDataType())
    {
    case (GDT_Byte): dataSize = 1; break;
    case (GDT_UInt16):
    case (GDT_Int16): dataSize = 2; break;
    case (GDT_UInt32):
    case (GDT_Int32):
    case (GDT_Float32): dataSize = 4; break;
    case (GDT_Float64): dataSize = 8; break;
    default:
        return false;
    }

    int offset = dataSize * component;
    int stride = image.properties.stride;

    int nBlockXSize, nBlockYSize;
    band.GetBlockSize(&nBlockXSize, &nBlockYSize);

    int nXBlocks = (band.GetXSize() + nBlockXSize - 1) / nBlockXSize;
    int nYBlocks = (band.GetYSize() + nBlockYSize - 1) / nBlockYSize;

    uint8_t* block = new uint8_t[dataSize * nBlockXSize * nBlockYSize];

    for (int iYBlock = 0; iYBlock < nYBlocks; iYBlock++)
    {
        for (int iXBlock = 0; iXBlock < nXBlocks; iXBlock++)
        {
            int nXValid, nYValid;
            CPLErr result = band.ReadBlock(iXBlock, iYBlock, block);
            if (result == 0)
            {
                // Compute the portion of the block that is valid
                // for partial edge blocks.
                band.GetActualBlockSize(iXBlock, iYBlock, &nXValid, &nYValid);

                for (int iY = 0; iY < nYValid; iY++)
                {
                    uint8_t* dest_ptr = reinterpret_cast<uint8_t*>(image.dataPointer(iXBlock * nBlockXSize + (iYBlock * nBlockYSize + iY) * image.width())) + offset;
                    uint8_t* source_ptr = block + iY * nBlockXSize * dataSize;

                    for (int iX = 0; iX < nXValid; iX++)
                    {
                        for (int c = 0; c < dataSize; ++c)
                        {
                            dest_ptr[c] = *source_ptr;
                            ++source_ptr;
                        }

                        dest_ptr += stride;
                    }
                }
            }
        }
    }
    delete[] block;

    return true;
}

bool vsgXchange::assignMetaData(GDALDataset& dataset, vsg::Object& object)
{
    auto metaData = dataset.GetMetadata();
    if (!metaData) return false;

    for (auto ptr = metaData; *ptr != 0; ++ptr)
    {
        std::string line(*ptr);
        auto equal_pos = line.find('=');
        if (equal_pos == std::string::npos)
        {
            object.setValue(line, std::string());
        }
        else
        {
            object.setValue(line.substr(0, equal_pos), line.substr(equal_pos + 1, std::string::npos));
        }
    }
    return true;
}
