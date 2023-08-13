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

#include <vsg/core/Value.h>
#include <vsg/io/Logger.h>

#include <vsgXchange/gdal.h>

#include <cstring>

using namespace vsgXchange;

bool vsgXchange::getEXIF_LatitudeLongitudeAlititude(GDALDataset& dataset, double& latitude, double& longitude, double& altitude)
{
    auto metaData = dataset.GetMetadata();
    if (!metaData) return false;

    auto match = [](const char* lhs, const char* rhs) -> const char* {
        auto len = std::strlen(rhs);
        if (strncmp(lhs, rhs, len) == 0)
            return lhs + len;
        else
            return nullptr;
    };

    bool success = false;

    const char* value_str = nullptr;
    std::stringstream str;
    str.imbue(std::locale::classic());

    for (auto ptr = metaData; *ptr != 0; ++ptr)
    {
        if (value_str = match(*ptr, "EXIF_GPSLatitude="); value_str)
        {
            str.clear();
            str.str(value_str);
            str >> vsgXchange::dms_in_brackets(latitude);
            success = true;
        }
        else if (value_str = match(*ptr, "EXIF_GPSLongitude="); value_str)
        {
            str.clear();
            str.str(value_str);
            str >> vsgXchange::dms_in_brackets(longitude);
            success = true;
        }
        else if (value_str = match(*ptr, "EXIF_GPSAltitude="); value_str)
        {
            str.clear();
            str.str(value_str);
            str >> vsgXchange::dms_in_brackets(altitude);
            success = true;
        }
    }

    return success;
}

bool vsgXchange::getEXIF_LatitudeLongitudeAlititude(const vsg::Object& object, double& latitude, double& longitude, double& altitude)
{
    bool success = false;

    std::string value_str;
    std::stringstream str;
    str.imbue(std::locale::classic());

    if (object.getValue("EXIF_GPSLatitude", value_str))
    {
        str.clear();
        str.str(value_str);
        str >> vsgXchange::dms_in_brackets(latitude);
        vsg::info("vsgXchange::getEXIF_..    EXIF_GPSLatitude = ", value_str, " degrees = ", latitude);
        success = true;
    }
    if (object.getValue("EXIF_GPSLongitude", value_str))
    {
        str.clear();
        str.str(value_str);
        str >> vsgXchange::dms_in_brackets(longitude);
        vsg::info("vsgXchange::getEXIF_..    EXIF_GPSLongitude = ", value_str, " degrees = ", longitude);
        success = true;
    }
    if (object.getValue("EXIF_GPSAltitude", value_str))
    {
        str.clear();
        str.str(value_str);
        str >> vsgXchange::in_brackets(altitude);
        vsg::info("vsgXchange::getEXIF_..    EXIF_GPSAltitude = ", value_str, " altitude = ", altitude);
        success = true;
    }
    return success;
}
