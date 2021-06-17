#pragma once

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

#include <vsg/io/ReaderWriter.h>
#include <vsgXchange/Export.h>

#include <memory>

namespace vsgXchange
{

    /// optional curl ReaderWriter.
    /// depends upon libcurl for reading data from http and https.
    class VSGXCHANGE_DECLSPEC curl : public vsg::Inherit<vsg::ReaderWriter, curl>
    {
    public:
        curl();
        vsg::ref_ptr<vsg::Object> read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options = {}) const override;

        bool getFeatures(Features& features) const override;

        /// specify whether libcurl should be initialized and cleaned up by vsgXchange::curl.
        static bool s_do_curl_global_init_and_cleanup; // defaults to true

    protected:
        ~curl();

        class Implementation;
        mutable std::mutex _mutex;
        mutable Implementation* _implementation;
    };

} // namespace vsgXchange

EVSG_type_name(vsgXchange::curl);
