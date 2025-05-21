/* <editor-fold desc="MIT License">

Copyright(c) 2025 Robert Osfield

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

#include <vsgXchange/3DTiles.h>

#include <vsg/nodes/Group.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/nodes/VertexIndexDraw.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/DepthSorted.h>
#include <vsg/nodes/Switch.h>
#include <vsg/nodes/CullNode.h>
#include <vsg/maths/transform.h>
#include <vsg/utils/GraphicsPipelineConfigurator.h>
#include <vsg/utils/ComputeBounds.h>
#include <vsg/state/material.h>

using namespace vsgXchange;

Tiles3D::SceneGraphBuilder::SceneGraphBuilder()
{
}

vsg::ref_ptr<vsg::Object> Tiles3D::SceneGraphBuilder::createSceneGraph(vsg::ref_ptr<Tiles3D::Tileset> tileset, vsg::ref_ptr<const vsg::Options> in_options)
{
    if (!tileset) return {};

    if (in_options) options = in_options;

    if (options) sharedObjects = options->sharedObjects;
    if (!sharedObjects) sharedObjects = vsg::SharedObjects::create();

    if (!shaderSet)
    {
        shaderSet = vsg::createPhysicsBasedRenderingShaderSet(options);
        if (sharedObjects) sharedObjects->share(shaderSet);
    }

    vsg::info("Tiles3D::SceneGraphBuilder::createSceneGraph() not yet implemented.");

    tileset->resolveURIs(options);

    return {};
}

