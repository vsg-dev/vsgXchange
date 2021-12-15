/* <editor-fold desc="MIT License">

Copyright(c) 2021 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgXchange/images.h>

#include <vsgGIS/gdal_utils.h>
#include <vsgGIS/TileDatabase.h>

#include <cstring>
#include <iostream>

using namespace vsgXchange;

namespace vsgXchange
{

    class GDAL::Implementation
    {
    public:
        Implementation();

        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const;

    protected:
    };

} // namespace vsgXchange

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// GDAL ReaderWriter fascade
//
GDAL::GDAL() :
    _implementation(new GDAL::Implementation())
{
    vsgGIS::init();
}
GDAL::~GDAL()
{
    delete _implementation;
}
vsg::ref_ptr<vsg::Object> GDAL::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    return _implementation->read(filename, options);
}

bool GDAL::getFeatures(Features& features) const
{
    vsgGIS::initGDAL();

    auto driverManager = GetGDALDriverManager();
    int driverCount = driverManager->GetDriverCount();

    vsg::ReaderWriter::FeatureMask rasterFeatureMask = vsg::ReaderWriter::READ_FILENAME;

    const std::string dotPrefix = ".";

    for (int i = 0; i < driverCount; ++i)
    {
        auto driver = driverManager->GetDriver(i);
        auto raster_meta = driver->GetMetadataItem(GDAL_DCAP_RASTER);
        auto extensions_meta = driver->GetMetadataItem(GDAL_DMD_EXTENSIONS);
        // auto longname_meta = driver->GetMetadataItem( GDAL_DMD_LONGNAME );
        if (raster_meta && extensions_meta)
        {
            std::string extensions = extensions_meta;
            std::string ext;

            std::string::size_type start_pos = 0;
            for (;;)
            {
                start_pos = extensions.find_first_not_of(" .", start_pos);
                if (start_pos == std::string::npos) break;

                std::string::size_type deliminator_pos = extensions.find_first_of(" /", start_pos);
                if (deliminator_pos != std::string::npos)
                {
                    ext = extensions.substr(start_pos, deliminator_pos - start_pos);
                    features.extensionFeatureMap[dotPrefix + ext] = rasterFeatureMask;
                    start_pos = deliminator_pos + 1;
                    if (start_pos == extensions.length()) break;
                }
                else
                {
                    ext = extensions.substr(start_pos, std::string::npos);
                    features.extensionFeatureMap[dotPrefix + ext] = rasterFeatureMask;
                    break;
                }
            }
        }
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// GDAL ReaderWriter implementation
//
GDAL::Implementation::Implementation()
{
}

vsg::ref_ptr<vsg::Object> GDAL::Implementation::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    // GDAL tries to load all datatypes so up front catch VSG and OSG native formats.
    vsg::Path ext = vsg::lowerCaseFileExtension(filename);
    if (ext == ".vsgb" || ext == ".vsgt" || ext == ".osgb" || ext == ".osgt" || ext == ".osg") return {};

    vsg::Path filenameToUse = vsg::findFile(filename, options);
    if (filenameToUse.empty()) return {};

    vsgGIS::initGDAL();

    auto dataset = vsgGIS::openSharedDataSet(filenameToUse.c_str(), GA_ReadOnly);
    if (!dataset)
    {
        return {};
    }

    auto types = vsgGIS::dataTypes(*dataset);
    if (types.size() > 1)
    {
        std::cout << "GDAL::read(" << filename << ") multiple input data types not suported." << std::endl;
        for (auto& type : types)
        {
            std::cout << "   GDALDataType " << GDALGetDataTypeName(type) << std::endl;
        }
        return {};
    }

    if (types.empty())
    {
        std::cout << "GDAL::read(" << filename << ") types set empty." << std::endl;

        return {};
    }

    GDALDataType dataType = *types.begin();

    std::vector<GDALRasterBand*> rasterBands;
    for (int i = 1; i <= dataset->GetRasterCount(); ++i)
    {
        GDALRasterBand* band = dataset->GetRasterBand(i);
        GDALColorInterp classification = band->GetColorInterpretation();

        if (classification != GCI_Undefined)
        {
            rasterBands.push_back(band);
        }
        else
        {
            std::cout << "GDAL::read(" << filename << ") Undefined classification on raster band " << i << std::endl;
        }
    }

    int numComponents = rasterBands.size();
    if (numComponents == 0)
    {
        std::cout << "GDAL::read(" << filename << ") failed numComponents = " << numComponents << std::endl;
        return {};
    }

    bool mapRGBtoRGBAHint = !options || options->mapRGBtoRGBAHint;
    if (mapRGBtoRGBAHint && numComponents == 3)
    {
        //std::cout<<"Reamppping RGB to RGBA "<<filename<<std::endl;
        numComponents = 4;
    }

    if (numComponents > 4)
    {
        std::cout << "GDAL::read(" << filename << ") Too many raster bands to merge into a single output, maximum of 4 raster bands supported." << std::endl;
        return {};
    }

    int width = dataset->GetRasterXSize();
    int height = dataset->GetRasterYSize();

    auto image = vsgGIS::createImage2D(width, height, numComponents, dataType, vsg::dvec4(0.0, 0.0, 0.0, 1.0));
    if (!image) return {};

    for (int component = 0; component < static_cast<int>(rasterBands.size()); ++component)
    {
        vsgGIS::copyRasterBandToImage(*rasterBands[component], *image, component);
    }

    vsgGIS::assignMetaData(*dataset, *image);

    if (dataset->GetProjectionRef() && std::strlen(dataset->GetProjectionRef()) > 0)
    {
        image->setValue("ProjectionRef", std::string(dataset->GetProjectionRef()));
    }

    auto transform = vsg::doubleArray::create(6);
    if (dataset->GetGeoTransform(transform->data()) == CE_None)
    {
        image->setObject("GeoTransform", transform);
    }

    return image;
}
