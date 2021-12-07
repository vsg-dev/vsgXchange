/* <editor-fold desc="MIT License">

Copyright(c) 2019 Thomas Hogarth and Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include "BuildOptions.h"

using namespace osg2vsg;

vsg::RegisterWithObjectFactoryProxy<osg2vsg::BuildOptions> s_Register_BuildOptions;

void BuildOptions::read(vsg::Input& input)
{
    input.read("insertCullGroups", insertCullGroups);
    input.read("insertCullNodes", insertCullNodes);
    input.read("useBindDescriptorSet", useBindDescriptorSet);
    input.read("billboardTransform", billboardTransform);
    input.readValue<int32_t>("geometryTarget", geometryTarget);
    input.read("supportedGeometryAttributes", supportedGeometryAttributes);
    input.read("supportedShaderModeMask", supportedShaderModeMask);
    input.read("overrideGeomAttributes", overrideGeomAttributes);
    input.read("overrideShaderModeMask", overrideShaderModeMask);
    input.read("useDepthSorted", useDepthSorted);
    if (input.version_greater_equal(0, 2, 0))
    {
        input.read("mapRGBtoRGBAHint", mapRGBtoRGBAHint);
        input.read("copyNames", copyNames);
    }
    input.read("vertexShaderPath", vertexShaderPath);
    input.read("fragmentShaderPath", fragmentShaderPath);
    input.read("extension", extension);
}

void BuildOptions::write(vsg::Output& output) const
{
    output.write("insertCullGroups", insertCullGroups);
    output.write("insertCullNodes", insertCullNodes);
    output.write("useBindDescriptorSet", useBindDescriptorSet);
    output.write("billboardTransform", billboardTransform);
    output.writeValue<int32_t>("geometryTarget", geometryTarget);
    output.write("supportedGeometryAttributes", supportedGeometryAttributes);
    output.write("supportedShaderModeMask", supportedShaderModeMask);
    output.write("overrideGeomAttributes", overrideGeomAttributes);
    output.write("overrideShaderModeMask", overrideShaderModeMask);
    output.write("useDepthSorted", useDepthSorted);
    if (output.version_greater_equal(0, 2, 0))
    {
        output.write("mapRGBtoRGBAHint", mapRGBtoRGBAHint);
        output.write("copyNames", copyNames);
    }
    output.write("vertexShaderPath", vertexShaderPath);
    output.write("fragmentShaderPath", fragmentShaderPath);
    output.write("extension", extension);
}

vsg::ref_ptr<vsg::BindGraphicsPipeline> PipelineCache::getOrCreateBindGraphicsPipeline(uint32_t shaderModeMask, uint32_t geometryAttributesMask, const std::string& vertShaderPath, const std::string& fragShaderPath)
{
    Key key(shaderModeMask, geometryAttributesMask, vertShaderPath, fragShaderPath);

    // check to see if pipeline has already been created
    {
        std::lock_guard<std::mutex> guard(mutex);
        if (auto itr = pipelineMap.find(key); itr != pipelineMap.end()) return itr->second;
    }

    vsg::ShaderStages shaders{
        vsg::ShaderStage::create(VK_SHADER_STAGE_VERTEX_BIT, "main", vertShaderPath.empty() ? createFbxVertexSource(shaderModeMask, geometryAttributesMask) : readGLSLShader(vertShaderPath, shaderModeMask, geometryAttributesMask)),
        vsg::ShaderStage::create(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderPath.empty() ? createFbxFragmentSource(shaderModeMask, geometryAttributesMask) : readGLSLShader(fragShaderPath, shaderModeMask, geometryAttributesMask))};

    // std::cout<<"createBindGraphicsPipeline("<<shaderModeMask<<", "<<geometryAttributesMask<<")"<<std::endl;

    vsg::DescriptorSetLayoutBindings descriptorBindings;

    // add material first if any (for now material is hardcoded to binding MATERIAL_BINDING)
    if (shaderModeMask & MATERIAL) descriptorBindings.push_back({MATERIAL_BINDING, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}); // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}

    // these need to go in incremental order by texture unit value as that how they will have been added to the desctiptor set
    // VkDescriptorSetLayoutBinding { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
    if (shaderModeMask & DIFFUSE_MAP) descriptorBindings.push_back({DIFFUSE_TEXTURE_UNIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}); // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
    if (shaderModeMask & OPACITY_MAP) descriptorBindings.push_back({OPACITY_TEXTURE_UNIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
    if (shaderModeMask & AMBIENT_MAP) descriptorBindings.push_back({AMBIENT_TEXTURE_UNIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
    if (shaderModeMask & NORMAL_MAP) descriptorBindings.push_back({NORMAL_TEXTURE_UNIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
    if (shaderModeMask & SPECULAR_MAP) descriptorBindings.push_back({SPECULAR_TEXTURE_UNIT, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
    vsg::DescriptorSetLayouts descriptorSetLayouts{descriptorSetLayout};

    vsg::PushConstantRanges pushConstantRanges{
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128} // projection and modelview matrices
    };

    uint32_t vertexBindingIndex = 0;

    vsg::VertexInputState::Bindings vertexBindingsDescriptions;
    vsg::VertexInputState::Attributes vertexAttributeDescriptions;

    // setup vertex array
    {
        vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{vertexBindingIndex, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX});
        vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{VERTEX_CHANNEL, vertexBindingIndex, VK_FORMAT_R32G32B32_SFLOAT, 0});
        vertexBindingIndex++;
    }

    if (geometryAttributesMask & NORMAL)
    {
        VkVertexInputRate nrate = geometryAttributesMask & NORMAL_OVERALL ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
        vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{vertexBindingIndex, sizeof(vsg::vec3), nrate});
        vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{NORMAL_CHANNEL, vertexBindingIndex, VK_FORMAT_R32G32B32_SFLOAT, 0}); // normal as vec3
        vertexBindingIndex++;
    }
    if (geometryAttributesMask & TANGENT)
    {
        VkVertexInputRate trate = geometryAttributesMask & TANGENT_OVERALL ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
        vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{vertexBindingIndex, sizeof(vsg::vec4), trate});
        vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{TANGENT_CHANNEL, vertexBindingIndex, VK_FORMAT_R32G32B32A32_SFLOAT, 0}); // tanget as vec4
        vertexBindingIndex++;
    }
    if (geometryAttributesMask & COLOR)
    {
        VkVertexInputRate crate = geometryAttributesMask & COLOR_OVERALL ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
        vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{vertexBindingIndex, sizeof(vsg::vec4), crate});
        vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{COLOR_CHANNEL, vertexBindingIndex, VK_FORMAT_R32G32B32A32_SFLOAT, 0}); // color as vec4
        vertexBindingIndex++;
    }
    if (geometryAttributesMask & TEXCOORD0)
    {
        vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{vertexBindingIndex, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX});
        vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{TEXCOORD0_CHANNEL, vertexBindingIndex, VK_FORMAT_R32G32_SFLOAT, 0}); // texcoord as vec2
        vertexBindingIndex++;
    }
    if (geometryAttributesMask & TRANSLATE)
    {
        VkVertexInputRate trate = geometryAttributesMask & TRANSLATE_OVERALL ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
        vertexBindingsDescriptions.push_back(VkVertexInputBindingDescription{vertexBindingIndex, sizeof(vsg::vec3), trate});
        vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{TRANSLATE_CHANNEL, vertexBindingIndex, VK_FORMAT_R32G32B32_SFLOAT, 0}); // tanget as vec4
        vertexBindingIndex++;
    }

    auto pipelineLayout = vsg::PipelineLayout::create(descriptorSetLayouts, pushConstantRanges);

    // if blending is requested setup appropriate colorblendstate
    vsg::ColorBlendState::ColorBlendAttachments colorBlendAttachments;
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                          VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT |
                                          VK_COLOR_COMPONENT_A_BIT;

    if (shaderModeMask & BLEND)
    {
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    colorBlendAttachments.push_back(colorBlendAttachment);

    vsg::GraphicsPipelineStates pipelineStates{
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        vsg::InputAssemblyState::create(),
        vsg::RasterizationState::create(),
        vsg::MultisampleState::create(),
        vsg::ColorBlendState::create(colorBlendAttachments),
        vsg::DepthStencilState::create()};

    //
    // set up graphics pipeline
    //
    vsg::ref_ptr<vsg::GraphicsPipeline> graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout, shaders, pipelineStates);
    auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);

    // assigne the pipeline to cache.
    std::lock_guard<std::mutex> guard(mutex);
    pipelineMap[key] = bindGraphicsPipeline;

    return bindGraphicsPipeline;
}
