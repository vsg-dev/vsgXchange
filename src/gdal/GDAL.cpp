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

#include <vsgXchange/images.h>

#include <vsgGIS/GDAL.h>
#include <vsgGIS/gdal_utils.h>

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
}

vsg::ref_ptr<vsg::Object> GDAL::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    return _implementation->read(filename, options);
}

bool GDAL::getFeatures(Features& features) const
{
    // TODO, need to look up support.
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
    if (ext == "vsgb" || ext == "vsgt" || ext == "osgb" || ext == "osgt" || ext == "osg") return {};

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
        std::cout << "GDAL::read("<<filename<<") multiple input data types not suported." << std::endl;
        for (auto& type : types)
        {
            std::cout << "   GDALDataType " << GDALGetDataTypeName(type) << std::endl;
        }
        return {};
    }

    if (types.empty())
    {
        std::cout<<"GDAL::read("<<filename<<") types set empty." << std::endl;

        return {};
    }

    GDALDataType dataType = *types.begin();

    std::vector<GDALRasterBand*> rasterBands;
    for(int i = 1; i <= dataset->GetRasterCount(); ++i)
    {
        GDALRasterBand* band = dataset->GetRasterBand(i);
        GDALColorInterp classification = band->GetColorInterpretation();

        if (classification != GCI_Undefined)
        {
            rasterBands.push_back(band);
        }
        else
        {
            std::cout<<"GDAL::read("<<filename<<") Undefined classification on raster band "<<i<<std::endl;
        }
    }

    int numComponents = rasterBands.size();
    if (numComponents==0)
    {
        std::cout<<"GDAL::read("<<filename<<") failed numComponents = "<<numComponents<<std::endl;
        return {};
    }

    if (numComponents==3) numComponents = 4;

    if (numComponents>4)
    {
        std::cout<<"GDAL::read("<<filename<<") Too many raster bands to merge into a single output, maximum of 4 raster bands supported."<<std::endl;
        return {};
    }

    int width = dataset->GetRasterXSize();
    int height =  dataset->GetRasterYSize();

    auto image = vsgGIS::createImage2D(width, height, numComponents, dataType, vsg::dvec4(0.0, 0.0, 0.0, 1.0));
    if (!image) return {};

    for(int component = 0; component < static_cast<int>(rasterBands.size()); ++component)
    {
        vsgGIS::copyRasterBandToImage(*rasterBands[component], *image, component);
    }

    vsgGIS::assignMetaData(*dataset, *image);

    if (dataset->GetProjectionRef() && std::strlen(dataset->GetProjectionRef())>0)
    {
        image->setValue("ProjectionRef", std::string(dataset->GetProjectionRef()));
    }

    auto transform = vsg::doubleArray::create(6);
    if (dataset->GetGeoTransform( transform->data() ) == CE_None)
    {
        image->setObject("GeoTransform", transform);
    }

    return image;
}
