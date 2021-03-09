/* <editor-fold desc="MIT License">

Copyright(c) 2021 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgXchange/curl.h>

#include <curl/curl.h>

#include <iostream>

using namespace vsgXchange;

namespace vsgXchange
{

    bool containsServerAddress(const vsg::Path& filename)
    {
        return filename.compare(0, 7, "http://")==0 || filename.compare(0, 8, "https://")==0;
    }

    std::pair<vsg::Path, vsg::Path> getServerPathAndFilename(const vsg::Path& filename)
    {
        std::string::size_type pos(filename.find("://"));

        if (pos != std::string::npos)
        {
            std::string::size_type pos_slash = filename.find_first_of('/',pos+3);
            if (pos_slash!=std::string::npos)
            {
                return {filename.substr(pos+3,pos_slash-pos-3), filename.substr(pos_slash+1,std::string::npos)};
            }
            else
            {
                return {filename.substr(pos+3,std::string::npos),""};
            }

        }
        return {};
    }

    class curl::Implementation
    {
    public:
        Implementation();

        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const;

    protected:
    };

} // namespace vsgXchange

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CURL ReaderWriter fascade
//
curl::curl() :
    _implementation(new curl::Implementation())
{
}

vsg::ref_ptr<vsg::Object> curl::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    if (containsServerAddress(filename))
    {
        return _implementation->read(filename, options);
    }
    else
    {
        return {};
    }
}

bool curl::getFeatures(Features& features) const
{
    return true;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CURL ReaderWriter implementation
//
curl::Implementation::Implementation()
{
}

vsg::ref_ptr<vsg::Object> curl::Implementation::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    std::cout<<"curl::Implementation::read"<<std::endl;

    auto [server_address, server_filename] = getServerPathAndFilename(filename);

    std::cout<<"   address = "<<server_address<<std::endl;
    std::cout<<"   filename = "<<server_filename<<std::endl;

    return {};
}
