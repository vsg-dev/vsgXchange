#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2021 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shimages be included in images
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/ReaderWriter.h>
#include <vsgXchange/Version.h>

#include <istream>
#include <memory>
#include <unordered_set>

namespace vsgXchange
{
    /// optional GDAL ReaderWriter
    class VSGXCHANGE_DECLSPEC GDAL : public vsg::Inherit<vsg::ReaderWriter, GDAL>
    {
    public:
        GDAL();
        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const override;
        vsg::ref_ptr<vsg::Object> read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options = {}) const override;
        vsg::ref_ptr<vsg::Object> read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options = {}) const override;

        bool getFeatures(Features& features) const override;

    protected:
        ~GDAL();

        class Implementation;
        Implementation* _implementation;
    };

} // namespace vsgXchange

EVSG_type_name(vsgXchange::GDAL);

#ifdef vsgXchange_GDAL

#    include "gdal_priv.h"
#    include "ogr_spatialref.h"

#    include <vsg/core/Data.h>
#    include <vsg/io/Path.h>
#    include <vsg/maths/vec4.h>

#    include <memory>
#    include <set>

namespace vsgXchange
{
    /// call GDALAllRegister() etc. if it hasn't already been called. Return true if this call to initGDAL() invoked GDAL setup, or false if it has previously been done.
    extern VSGXCHANGE_DECLSPEC bool initGDAL();

    /// Call GDALOpen(..) to open specified file returning a std::shared_ptr<GDALDataset> to reboustly manage the lifetime of the GDALDataSet, automatiically call GDALClose.
    inline std::shared_ptr<GDALDataset> openDataSet(const vsg::Path& filename, GDALAccess access)
    {
        // GDAL doesn't support wide string filenames so convert vsg::Path to std::string and then pass to GDALOpen
        return std::shared_ptr<GDALDataset>(static_cast<GDALDataset*>(GDALOpen(filename.string().c_str(), access)), [](GDALDataset* dataset) { GDALClose(dataset); });
    }

    /// Call GDALOpenShared(..) to open specified file returning a std::shared_ptr<GDALDataset> to reboustly manage the lifetime of the GDALDataSet, automatiically call GDALClose.
    inline std::shared_ptr<GDALDataset> openSharedDataSet(const vsg::Path& filename, GDALAccess access)
    {
        // GDAL doesn't support wide string filenames so convert vsg::Path to std::string and then pass to GDALOpenShared
        return std::shared_ptr<GDALDataset>(static_cast<GDALDataset*>(GDALOpenShared(filename.string().c_str(), access)), [](GDALDataset* dataset) { GDALClose(dataset); });
    }

    /// return true if two GDALDataset has the same projection reference string/
    extern VSGXCHANGE_DECLSPEC bool compatibleDatasetProjections(const GDALDataset& lhs, const GDALDataset& rhs);

    /// return true if two GDALDataset has the same projection, geo transform and dimensions indicating they are perfectly pixel aliged and matched in size.
    extern VSGXCHANGE_DECLSPEC bool compatibleDatasetProjectionsTransformAndSizes(const GDALDataset& lhs, const GDALDataset& rhs);

    /// create a vsg::Image2D of the approrpiate type that maps to specified dimensions and GDALDataType
    extern VSGXCHANGE_DECLSPEC vsg::ref_ptr<vsg::Data> createImage2D(int width, int height, int numComponents, GDALDataType dataType, vsg::dvec4 def = {0.0, 0.0, 0.0, 1.0});

    /// copy a RasterBand onto a target RGBA component of a vsg::Data.  Dimensions and datatypes must be compatble between RasterBand and vsg::Data. Return true on success, false on failure to copy.
    extern VSGXCHANGE_DECLSPEC bool copyRasterBandToImage(GDALRasterBand& band, vsg::Data& image, int component);

    /// assign GDAL MetaData mapping the "key=value" entries to vsg::Object as setValue(key, std::string(value)).
    extern VSGXCHANGE_DECLSPEC bool assignMetaData(GDALDataset& dataset, vsg::Object& object);

    /// call binary comparison operators on dereferenced items in specified range.
    template<class Iterator, class BinaryPredicate>
    bool all_equal(Iterator first, Iterator last, BinaryPredicate compare)
    {
        if (first == last) return true;
        Iterator itr = first;
        ++itr;

        for (; itr != last; ++itr)
        {
            if (!compare(**first, **itr)) return false;
        }

        return true;
    }

    /// collect the set of GDALDataType of all the RansterBand is the specified GDALDataset
    inline std::set<GDALDataType> dataTypes(GDALDataset& dataset)
    {
        std::set<GDALDataType> types;
        for (int i = 1; i <= dataset.GetRasterCount(); ++i)
        {
            GDALRasterBand* band = dataset.GetRasterBand(i);
            types.insert(band->GetRasterDataType());
        }

        return types;
    }

    /// collect the set of GDALDataType of all the RansterBand is the specified range of GDALDataset
    template<class Iterator>
    std::set<GDALDataType> dataTypes(Iterator first, Iterator last)
    {
        std::set<GDALDataType> types;
        for (Iterator itr = first; itr != last; ++itr)
        {
            GDALDataset& dataset = **itr;
            for (int i = 1; i <= dataset.GetRasterCount(); ++i)
            {
                GDALRasterBand* band = dataset.GetRasterBand(i);
                types.insert(band->GetRasterDataType());
            }
        }
        return types;
    }

    /// get the latitude, longitude and altitude values from the GDALDataSet's EXIF_GPSLatitude, EXIF_GPSLongitude and EXIF_GPSAltitude meta data fields, return true on success,
    extern VSGXCHANGE_DECLSPEC bool getEXIF_LatitudeLongitudeAlititude(GDALDataset& dataset, double& latitude, double& longitude, double& altitude);

    /// get the latitude, longitude and altitude values from the vsg::Object's EXIF_GPSLatitude, EXIF_GPSLongitude and EXIF_GPSAltitude meta data fields, return true on success,
    extern VSGXCHANGE_DECLSPEC bool getEXIF_LatitudeLongitudeAlititude(const vsg::Object& object, double& latitude, double& longitude, double& altitude);

    template<typename T>
    struct in_brackets
    {
        in_brackets(T& v) :
            value(v) {}
        T& value;
    };

    template<typename T>
    std::istream& operator>>(std::istream& input, in_brackets<T> field)
    {
        while (input.peek() == ' ') input.get();

        std::string str;
        if (input.peek() == '(')
        {
            input.ignore();

            input >> field.value;

            if constexpr (std::is_same_v<T, std::string>)
            {
                if (!field.value.empty() && field.value[field.value.size() - 1] == ')')
                {
                    field.value.erase(field.value.size() - 1);
                    return input;
                }
                else
                {
                    while (input.peek() != ')')
                    {
                        int c = input.get();
                        if (input.eof()) return input;

                        field.value.push_back(c);
                    }
                }
            }

            if (input.peek() == ')')
            {
                input.ignore();
            }
        }
        else
        {
            input >> field.value;
        }

        return input;
    }

    /// helper class for reading decimal degree in the form of (degrees) (minutes) (seconds) as used with EXIF_GPSLatitude and EXIF_GPSLongitude tags.
    /// usage:
    ///    std::stringstream str(EXIF_GPSLatitude_String);
    ///    double latitude;
    ///    str >> dms_in_brackets(latitude);
    struct dms_in_brackets
    {
        dms_in_brackets(double& angle) :
            value(angle) {}
        double& value;
    };

    inline std::istream& operator>>(std::istream& input, dms_in_brackets field)
    {
        double degrees = 0.0, minutes = 0.0, seconds = 0.0;
        input >> in_brackets(degrees) >> in_brackets(minutes) >> in_brackets(seconds);
        field.value = degrees + (minutes + seconds / 60.0) / 60.0;
        return input;
    }

} // namespace vsgXchange

#endif
