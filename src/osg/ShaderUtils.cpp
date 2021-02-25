/* <editor-fold desc="MIT License">

Copyright(c) 2019 Thomas Hogarth and Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include "ShaderUtils.h"
#include "GeometryUtils.h"

#include <algorithm>
#include <iomanip>

using namespace osg2vsg;

#if 1
#    define DEBUG_OUTPUT std::cout
#else
#    define DEBUG_OUTPUT \
        if (false) std::cout
#endif
#define INFO_OUTPUT std::cout

uint32_t osg2vsg::calculateShaderModeMask(const osg::StateSet* stateSet)
{
    uint32_t stateMask = 0;
    if (stateSet)
    {
        if (stateSet->getMode(GL_BLEND) & osg::StateAttribute::ON) stateMask |= BLEND;
        if (stateSet->getMode(GL_LIGHTING) & osg::StateAttribute::ON) stateMask |= LIGHTING;

        auto asMaterial = dynamic_cast<const osg::Material*>(stateSet->getAttribute(osg::StateAttribute::Type::MATERIAL));
        if (asMaterial && stateSet->getMode(GL_COLOR_MATERIAL) == osg::StateAttribute::Values::ON) stateMask |= MATERIAL;

        auto hasTextureWithImageInChannel = [stateSet](unsigned int channel) {
            auto asTex = dynamic_cast<const osg::Texture*>(stateSet->getTextureAttribute(channel, osg::StateAttribute::TEXTURE));
            if (asTex && asTex->getImage(0)) return true;
            return false;
        };

        if (hasTextureWithImageInChannel(DIFFUSE_TEXTURE_UNIT)) stateMask |= DIFFUSE_MAP;
        if (hasTextureWithImageInChannel(OPACITY_TEXTURE_UNIT)) stateMask |= OPACITY_MAP;
        if (hasTextureWithImageInChannel(AMBIENT_TEXTURE_UNIT)) stateMask |= AMBIENT_MAP;
        if (hasTextureWithImageInChannel(NORMAL_TEXTURE_UNIT)) stateMask |= NORMAL_MAP;
        if (hasTextureWithImageInChannel(SPECULAR_TEXTURE_UNIT)) stateMask |= SPECULAR_MAP;
    }
    return stateMask;
}

// create defines string based of shader mask

static std::vector<std::string> createPSCDefineStrings(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    bool hasnormal = geometryAttrbutes & NORMAL;
    bool hastanget = geometryAttrbutes & TANGENT;
    bool hascolor = geometryAttrbutes & COLOR;
    bool hastex0 = geometryAttrbutes & TEXCOORD0;

    std::vector<std::string> defines;

    // vertx inputs
    if (hasnormal) defines.push_back("VSG_NORMAL");
    if (hascolor) defines.push_back("VSG_COLOR");
    if (hastex0) defines.push_back("VSG_TEXCOORD0");
    if (hastanget) defines.push_back("VSG_TANGENT");

    // shading modes/maps
    if (hasnormal && (shaderModeMask & LIGHTING)) defines.push_back("VSG_LIGHTING");

    if (shaderModeMask & MATERIAL) defines.push_back("VSG_MATERIAL");

    if (hastex0 && (shaderModeMask & DIFFUSE_MAP)) defines.push_back("VSG_DIFFUSE_MAP");
    if (hastex0 && (shaderModeMask & OPACITY_MAP)) defines.push_back("VSG_OPACITY_MAP");
    if (hastex0 && (shaderModeMask & AMBIENT_MAP)) defines.push_back("VSG_AMBIENT_MAP");
    if (hastex0 && (shaderModeMask & NORMAL_MAP)) defines.push_back("VSG_NORMAL_MAP");
    if (hastex0 && (shaderModeMask & SPECULAR_MAP)) defines.push_back("VSG_SPECULAR_MAP");

    if (shaderModeMask & BILLBOARD) defines.push_back("VSG_BILLBOARD");

    if (shaderModeMask & SHADER_TRANSLATE) defines.push_back("VSG_TRANSLATE");

    return defines;
}

// insert defines string after the version in source

static std::string processGLSLShaderSource(const std::string& source, const std::vector<std::string>& defines)
{
    // trim leading spaces/tabs
    auto trimLeading = [](std::string& str) {
        size_t startpos = str.find_first_not_of(" \t");
        if (std::string::npos != startpos)
        {
            str = str.substr(startpos);
        }
    };

    // trim trailing spaces/tabs/newlines
    auto trimTrailing = [](std::string& str) {
        size_t endpos = str.find_last_not_of(" \t\n");
        if (endpos != std::string::npos)
        {
            str = str.substr(0, endpos + 1);
        }
    };

    // sanitise line by triming leading and trailing characters
    auto sanitise = [&trimLeading, &trimTrailing](std::string& str) {
        trimLeading(str);
        trimTrailing(str);
    };

    // return true if str starts with match string
    auto startsWith = [](const std::string& str, const std::string& match) {
        return str.compare(0, match.length(), match) == 0;
    };

    // returns the string between the start and end character
    auto stringBetween = [](const std::string& str, const char& startChar, const char& endChar) {
        auto start = str.find_first_of(startChar);
        if (start == std::string::npos) return std::string();

        auto end = str.find_first_of(endChar, start);
        if (end == std::string::npos) return std::string();

        if ((end - start) - 1 == 0) return std::string();

        return str.substr(start + 1, (end - start) - 1);
    };

    auto split = [](const std::string& str, const char& seperator) {
        std::vector<std::string> elements;

        std::string::size_type prev_pos = 0, pos = 0;

        while ((pos = str.find(seperator, pos)) != std::string::npos)
        {
            auto substring = str.substr(prev_pos, pos - prev_pos);
            elements.push_back(substring);
            prev_pos = ++pos;
        }

        elements.push_back(str.substr(prev_pos, pos - prev_pos));

        return elements;
    };

    auto addLine = [](std::ostringstream& ss, const std::string& line) {
        ss << line << "\n";
    };

    std::istringstream iss(source);
    std::ostringstream headerstream;
    std::ostringstream sourcestream;

    const std::string versionmatch = "#version";
    const std::string importdefinesmatch = "#pragma import_defines";

    std::vector<std::string> finaldefines;

    for (std::string line; std::getline(iss, line);)
    {
        std::string sanitisedline = line;
        sanitise(sanitisedline);

        // is it the version
        if (startsWith(sanitisedline, versionmatch))
        {
            addLine(headerstream, line);
        }
        // is it the defines import
        else if (startsWith(sanitisedline, importdefinesmatch))
        {
            // get the import defines between ()
            auto csv = stringBetween(sanitisedline, '(', ')');
            auto importedDefines = split(csv, ',');

            addLine(headerstream, line);

            // loop the imported defines and see if it's also requested in defines, if so insert a define line
            for (auto importedDef : importedDefines)
            {
                auto sanitiesedImportDef = importedDef;
                sanitise(sanitiesedImportDef);

                auto finditr = std::find(defines.begin(), defines.end(), sanitiesedImportDef);
                if (finditr != defines.end())
                {
                    addLine(headerstream, "#define " + sanitiesedImportDef);
                }
            }
        }
        else
        {
            // standard source line
            addLine(sourcestream, line);
        }
    }

    return headerstream.str() + sourcestream.str();
}

// read a glsl file and inject defines based on shadermodemask and geometryatts
std::string osg2vsg::readGLSLShader(const std::string& filename, const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    std::string sourceBuffer;
    if (!vsg::readFile(sourceBuffer, filename))
    {
        DEBUG_OUTPUT << "readGLSLShader: Failed to read file '" << filename << std::endl;
        return std::string();
    }

    auto defines = createPSCDefineStrings(shaderModeMask, geometryAttrbutes);
    std::string formatedSource = processGLSLShaderSource(sourceBuffer, defines);
    return formatedSource;
}

// create an fbx vertex shader
#include "shaders/fbxshader_vert.cpp"

std::string osg2vsg::createFbxVertexSource(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    auto defines = createPSCDefineStrings(shaderModeMask, geometryAttrbutes);
    std::string formatedSource = processGLSLShaderSource(fbxshader_vert, defines);

    return formatedSource;
}

// create an fbx fragment shader
#include "shaders/fbxshader_frag.cpp"

std::string osg2vsg::createFbxFragmentSource(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    auto defines = createPSCDefineStrings(shaderModeMask, geometryAttrbutes);
    std::string formatedSource = processGLSLShaderSource(fbxshader_frag, defines);

    return formatedSource;
}

// create a default vertex shader
#include "shaders/defaultshader_vert.cpp"

std::string osg2vsg::createDefaultVertexSource(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    auto defines = createPSCDefineStrings(shaderModeMask, geometryAttrbutes);
    std::string formatedSource = processGLSLShaderSource(defaultshader_vert, defines);

    return formatedSource;
}

// create a default fragment shader
#include "shaders/defaultshader_frag.cpp"

std::string osg2vsg::createDefaultFragmentSource(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes)
{
    auto defines = createPSCDefineStrings(shaderModeMask, geometryAttrbutes);
    std::string formatedSource = processGLSLShaderSource(defaultshader_frag, defines);

    return formatedSource;
}
