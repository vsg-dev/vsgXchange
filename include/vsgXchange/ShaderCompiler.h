#pragma once

#include <vsgXchange/Export.h>

#include <vsg/vk/ShaderStage.h>

namespace vsgXchange
{
    class VSGXCHANGE_DECLSPEC ShaderCompiler : public vsg::Object
    {
    public:
        ShaderCompiler(vsg::Allocator* allocator=nullptr);
        virtual ~ShaderCompiler();

        bool compile(vsg::ShaderStages& shaders);
    };
}
