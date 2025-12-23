/* <editor-fold desc="MIT License">

Copyright(c) 2021 André Normann & Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include "SceneConverter.h"

using namespace vsgXchange;

SubgraphStats SceneConverter::collectSubgraphStats(const aiNode* in_node, unsigned int depth)
{
    SubgraphStats stats;

    stats.depth = depth;
    ++stats.numNodes;

    stats.numMesh += in_node->mNumMeshes;

    for (unsigned int i = 0; i < in_node->mNumChildren; ++i)
    {
        stats += collectSubgraphStats(in_node->mChildren[i], depth + 1);
    }

    subgraphStats[in_node] = stats;

    return stats;
}

SubgraphStats SceneConverter::collectSubgraphStats(const aiScene* in_scene)
{
    SubgraphStats stats;

    bones.clear();

    std::map<const aiNode*, unsigned int> joints;

    for (unsigned int mi = 0; mi < in_scene->mNumMeshes; ++mi)
    {
        auto mesh = in_scene->mMeshes[mi];
        for (unsigned int bi = 0; bi < mesh->mNumBones; ++bi)
        {
            auto bone = mesh->mBones[bi];
            if (bones.find(bone) == bones.end())
            {
                unsigned int jointIndex = 0;
                if (auto itr = joints.find(bone->mNode); itr != joints.end())
                {
                    jointIndex = itr->second;
                }
                else
                {
                    joints[bone->mNode] = jointIndex = static_cast<unsigned int>(joints.size());
                }

                auto& boneStats = bones[bone];
                boneStats.index = jointIndex;
                boneStats.name = bone->mName.C_Str();
                boneStats.node = bone->mNode;

                boneTransforms[boneStats.node] = boneStats.index;

                // vsg::info("bone = ", bone, ", name = ", bone->mName.C_Str(), ", node = ", boneStats.node, ", node->mName = ", boneStats.node->mName.C_Str(), ", joint.index = ", jointIndex);
            }
        }
    }

    stats.numBones = static_cast<unsigned int>(joints.size()); // bones.size();

    if (in_scene->mRootNode)
    {
        stats += collectSubgraphStats(in_scene->mRootNode, 1);
    }

    return stats;
}

SubgraphStats SceneConverter::print(std::ostream& out, const aiAnimation* animation, vsg::indentation indent)
{
    out << indent << "aiAnimation " << animation << " name = " << animation->mName.C_Str() << std::endl;
    out << indent << "{" << std::endl;

    indent += 2;

    out << indent << "animation->mNumChannels " << animation->mNumChannels << std::endl;
    for (unsigned int ci = 0; ci < animation->mNumChannels; ++ci)
    {
        auto nodeAnim = animation->mChannels[ci];
        /** The name of the node affected by this animation. The node
        *  must exist and it must be unique.*/
        out << indent << "aiNodeAnim[" << ci << "] = " << nodeAnim << " mNodeName = " << nodeAnim->mNodeName.C_Str() << std::endl;
        out << indent << "{" << std::endl;

        indent += 2;

        out << indent << "nodeAnim->mNumPositionKeys = " << nodeAnim->mNumPositionKeys << std::endl;

        if (printAssimp >= 3)
        {
            out << indent << "{" << std::endl;
            indent += 2;
            for (unsigned int pi = 0; pi < nodeAnim->mNumPositionKeys; ++pi)
            {
                auto& positionKey = nodeAnim->mPositionKeys[pi];
                out << indent << "positionKey{ mTime = " << positionKey.mTime << ", mValue = (" << positionKey.mValue.x << ", " << positionKey.mValue.y << ", " << positionKey.mValue.z << ") }" << std::endl;
            }
            indent -= 2;
            out << indent << "}" << std::endl;
        }

        out << indent << "nodeAnim->mNumRotationKeys = " << nodeAnim->mNumRotationKeys << std::endl;
        if (printAssimp >= 3)
        {
            out << indent << "{" << std::endl;
            indent += 2;
            for (unsigned int ri = 0; ri < nodeAnim->mNumRotationKeys; ++ri)
            {
                auto& rotationKey = nodeAnim->mRotationKeys[ri];
                out << indent << "rotationKey{ mTime = " << rotationKey.mTime << ", mValue = (" << rotationKey.mValue.x << ", " << rotationKey.mValue.y << ", " << rotationKey.mValue.z << ", " << rotationKey.mValue.w << ") }" << std::endl;
            }
            indent -= 2;
            out << indent << "}" << std::endl;
        }

        out << indent << "nodeAnim->mNumScalingKeys = " << nodeAnim->mNumScalingKeys << std::endl;
        if (printAssimp >= 3)
        {
            out << indent << "{" << std::endl;
            indent += 2;
            for (unsigned int si = 0; si < nodeAnim->mNumScalingKeys; ++si)
            {
                auto& scalingKey = nodeAnim->mScalingKeys[si];
                out << indent << "scalingKey{ mTime = " << scalingKey.mTime << ", mValue = (" << scalingKey.mValue.x << ", " << scalingKey.mValue.y << ", " << scalingKey.mValue.z << ") }" << std::endl;
            }
            indent -= 2;
            out << indent << "}" << std::endl;
        }

        /** Defines how the animation behaves before the first //
        *  key is encountered.
        *
        *  The default value is aiAnimBehaviour_DEFAULT (the original
        *  transformation matrix of the affected node is used).*/
        out << indent << "aiAnimBehaviour mPreState = " << nodeAnim->mPreState << std::endl;

        /** Defines how the animation behaves after the last
        *  key was processed.
        *
        *  The default value is aiAnimBehaviour_DEFAULT (the original
        *  transformation matrix of the affected node is taken).*/
        out << indent << "aiAnimBehaviour mPostState = " << nodeAnim->mPostState << std::endl;

        indent -= 2;
        out << indent << "}" << std::endl;
    }

    out << indent << "mNumMeshChannels = " << animation->mNumMeshChannels << std::endl;
    if (animation->mNumMeshChannels != 0)
    {
        vsg::warn("Assimp::aiMeshAnim not supported, animation->mNumMeshChannels = ", animation->mNumMeshChannels, " ignored.");
    }

    out << indent << "mNumMorphMeshChannels = " << animation->mNumMorphMeshChannels << std::endl;

    if (printAssimp >= 2 && animation->mNumMorphMeshChannels > 0)
    {
        out << indent << "{";
        indent += 2;
        for (unsigned int moi = 0; moi < animation->mNumMorphMeshChannels; ++moi)
        {
            auto meshMorphAnim = animation->mMorphMeshChannels[moi];

            out << indent << "mMorphMeshChannels[" << moi << "] = " << meshMorphAnim << std::endl;

            out << indent << "{" << std::endl;
            indent += 2;

            /** Name of the mesh to be animated. An empty string is not allowed,
            *  animated meshes need to be named (not necessarily uniquely,
            *  the name can basically serve as wildcard to select a group
            *  of meshes with similar animation setup)*/
            out << indent << "mName << " << meshMorphAnim->mName.C_Str() << std::endl;
            out << indent << "mNumKeys<< " << meshMorphAnim->mNumKeys << std::endl;

            if (printAssimp >= 3)
            {
                for (unsigned int mki = 0; mki < meshMorphAnim->mNumKeys; ++mki)
                {
                    auto& morphKey = meshMorphAnim->mKeys[mki];
                    out << indent << "morphKey[" << mki << "]" << std::endl;
                    out << indent << "{";
                    indent += 2;
                    out << indent << "mTime = " << morphKey.mTime << ", mValues = " << morphKey.mValues << ", mWeights =" << morphKey.mWeights << ", mNumValuesAndWeights = " << morphKey.mNumValuesAndWeights;
                    for (unsigned int vwi = 0; vwi < morphKey.mNumValuesAndWeights; ++vwi)
                    {
                        out << indent << "morphKey.mNumValuesAndWeights[" << vwi << "] = { value = " << morphKey.mValues[vwi] << ", weight = " << morphKey.mWeights[vwi] << "}" << std::endl;
                    }
                    indent -= 2;
                    out << indent << "}";
                }
            }

            indent -= 2;
            out << indent << "}" << std::endl;
        }

        indent -= 2;

        out << indent << "}" << std::endl;
    }

    indent -= 2;
    out << indent << "}" << std::endl;

    return {};
}

SubgraphStats SceneConverter::print(std::ostream& out, const aiMaterial* in_material, vsg::indentation indent)
{
    out << indent << "aiMaterial " << in_material << std::endl;
    return {};
}

SubgraphStats SceneConverter::print(std::ostream& out, const aiMesh* in_mesh, vsg::indentation indent)
{
    out << indent << "aiMesh " << in_mesh << std::endl;
    out << indent << "{" << std::endl;
    indent += 2;

    out << indent << "mName = " << in_mesh->mName.C_Str() << std::endl;
    out << indent << "mNumVertices = " << in_mesh->mNumVertices << std::endl;
    out << indent << "mNumFaces = " << in_mesh->mNumFaces << std::endl;
    //out << indent << "mWeight = " <<in_mesh->mWeight<<std::endl;

    out << indent << "mNumBones = " << in_mesh->mNumBones << std::endl;
    if (in_mesh->HasBones())
    {
        out << indent << "mBones[] = " << std::endl;
        out << indent << "{" << std::endl;

        auto nested = indent + 2;

        for (size_t i = 0; i < in_mesh->mNumBones; ++i)
        {
            auto bone = in_mesh->mBones[i];
            out << nested << "mBones[" << i << "] = " << bone << " { mName " << bone->mName.C_Str() << ", mNode = " << bone->mNode << ", mNumWeights = " << bone->mNumWeights << " }" << std::endl;
            // mOffsetMatrix
            // mNumWeights // { mVertexId, mWeight }
        }

        out << indent << "}" << std::endl;
    }

    out << indent << "mNumAnimMeshes " << in_mesh->mNumAnimMeshes << std::endl;
    if (in_mesh->mNumAnimMeshes != 0)
    {
        out << indent << "mAnimMeshes[] = " << std::endl;
        out << indent << "{" << std::endl;

        auto nested = indent + 2;

        for (unsigned int ai = 0; ai < in_mesh->mNumAnimMeshes; ++ai)
        {
            auto* animationMesh = in_mesh->mAnimMeshes[ai];
            out << nested << "mAnimMeshes[" << ai << "] = " << animationMesh << ", mName = " << animationMesh->mName.C_Str() << ", mWeight =  " << animationMesh->mWeight << std::endl;
        }

        out << indent << "}" << std::endl;
    }

    indent -= 2;

    out << indent << "}" << std::endl;

    return {};
}

SubgraphStats SceneConverter::print(std::ostream& out, const aiNode* in_node, vsg::indentation indent)
{
    SubgraphStats stats;
    ++stats.numNodes;

    out << indent << "aiNode " << in_node << " mName = " << in_node->mName.C_Str() << std::endl;

    out << indent << "{" << std::endl;
    indent += 2;

    out << indent << "mTransformation = " << in_node->mTransformation << std::endl;
    out << indent << "mNumMeshes = " << in_node->mNumMeshes << std::endl;
    if (in_node->mNumMeshes > 0)
    {
        out << indent << "mMeshes[] = { ";
        for (unsigned int i = 0; i < in_node->mNumMeshes; ++i)
        {
            if (i > 0) out << ", ";
            out << in_node->mMeshes[i];
        }
        out << " }" << std::endl;

        stats.numMesh += in_node->mNumMeshes;
    }

    out << indent << "mNumChildren = " << in_node->mNumChildren << std::endl;
    if (in_node->mNumChildren > 0)
    {
        out << indent << "mChildren[] = " << std::endl;
        out << indent << "{" << std::endl;
        for (unsigned int i = 0; i < in_node->mNumChildren; ++i)
        {
            stats += print(out, in_node->mChildren[i], indent + 2);
        }
        out << indent << "}" << std::endl;
    }

    indent -= 2;
    out << indent << "} " << stats << std::endl;

    return stats;
}

SubgraphStats SceneConverter::print(std::ostream& out, const aiScene* in_scene, vsg::indentation indent)
{
    SubgraphStats stats;

    out << indent << "aiScene " << in_scene << std::endl;
    out << indent << "{" << std::endl;

    indent += 2;

    out << indent << "scene->mNumMaterials " << scene->mNumMaterials << std::endl;
    out << indent << "{" << std::endl;
    for (unsigned int mi = 0; mi < scene->mNumMaterials; ++mi)
    {
        print(out, scene->mMaterials[mi], indent + 2);
    }
    out << indent << "}" << std::endl;

    out << indent << "scene->mNumMeshes " << scene->mNumMeshes << std::endl;
    out << indent << "{" << std::endl;
    for (unsigned int mi = 0; mi < scene->mNumMeshes; ++mi)
    {
        print(out, scene->mMeshes[mi], indent + 2);
    }
    out << indent << "}" << std::endl;

    out << indent << "scene->mNumAnimations " << scene->mNumAnimations << std::endl;
    out << indent << "{" << std::endl;
    if (scene->mNumAnimations > 0)
    {
        for (unsigned int ai = 0; ai < scene->mNumAnimations; ++ai)
        {
            print(out, scene->mAnimations[ai], indent + 2);
        }
    }
    out << indent << "}" << std::endl;

    out << indent << "scene->mRootNode " << scene->mRootNode << std::endl;
    if (scene->mRootNode != nullptr)
    {
        stats += print(out, scene->mRootNode, indent);
    }

    indent -= 2;

    out << indent << "}" << std::endl;
    out << indent << stats << std::endl;

    return stats;
}

SamplerData SceneConverter::convertTexture(const aiMaterial& material, aiTextureType type) const
{
    aiString texPath;
    aiTextureMapMode wrapMode[]{aiTextureMapMode_Wrap, aiTextureMapMode_Wrap, aiTextureMapMode_Wrap};

    if (material.GetTexture(type, 0, &texPath, nullptr, nullptr, nullptr, nullptr, wrapMode) == AI_SUCCESS)
    {
        SamplerData samplerImage;
        vsg::Path externalTextureFilename;

        if (auto texture = scene->GetEmbeddedTexture(texPath.C_Str()))
        {
            // embedded texture has no width so must be invalid
            if (texture->mWidth == 0) return {};

            if (texture->mHeight == 0)
            {
                vsg::debug("filename = ", filename, " : Embedded compressed format texture->achFormatHint = ", texture->achFormatHint);

                // texture is a compressed format, defer to the VSG's vsg::read() to convert the block of data to vsg::Data image.
                auto imageOptions = vsg::clone(options);
                imageOptions->extensionHint = vsg::Path(".") + texture->achFormatHint;
                samplerImage.data = vsg::read_cast<vsg::Data>(reinterpret_cast<const uint8_t*>(texture->pcData), texture->mWidth, imageOptions);

                // if no data assigned return null
                if (!samplerImage.data) return {};
            }
            else
            {
                vsg::debug("filename = ", filename, " : Embedded raw format texture->achFormatHint = ", texture->achFormatHint);

                // Vulkan doesn't support this format, we have to reorder it to RGBA
                auto image = vsg::ubvec4Array2D::create(texture->mWidth, texture->mHeight, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM});
                auto src = texture->pcData;
                for (auto& dest_c : *image)
                {
                    auto& src_c = *(src++);
                    dest_c.r = src_c.r;
                    dest_c.g = src_c.g;
                    dest_c.b = src_c.b;
                    dest_c.a = src_c.a;
                }
                samplerImage.data = image;
            }
        }
        else
        {
            externalTextureFilename = vsg::findFile(texPath.C_Str(), options);
            if (samplerImage.data = vsg::read_cast<vsg::Data>(externalTextureFilename, options); !samplerImage.data.valid())
            {
                vsg::warn("Failed to load texture: ", externalTextureFilename, " texPath = ", texPath.C_Str());
                return {};
            }
        }

        samplerImage.sampler = vsg::Sampler::create();
        samplerImage.sampler->addressModeU = getWrapMode(wrapMode[0]);
        samplerImage.sampler->addressModeV = getWrapMode(wrapMode[1]);
        samplerImage.sampler->addressModeW = getWrapMode(wrapMode[2]);
        samplerImage.sampler->anisotropyEnable = VK_TRUE;
        samplerImage.sampler->maxAnisotropy = 16.0f;
        samplerImage.sampler->maxLod = static_cast<float>(samplerImage.data->properties.mipLevels);

        if (samplerImage.sampler->maxLod <= 1.0)
        {
            // Calculate maximum lod level
            auto maxDim = std::max(samplerImage.data->width(), samplerImage.data->height());
            samplerImage.sampler->maxLod = std::floor(std::log2f(static_cast<float>(maxDim)));
        }

        if (sharedObjects)
        {
            sharedObjects->share(samplerImage.data);
            sharedObjects->share(samplerImage.sampler);
        }

        if (externalTextures && externalObjects)
        {
            // calculate the texture filename
            switch (externalTextureFormat)
            {
            case TextureFormat::native:
                break; // nothing to do
            case TextureFormat::vsgt:
                externalTextureFilename = vsg::removeExtension(externalTextureFilename).concat(".vsgt");
                break;
            case TextureFormat::vsgb:
                externalTextureFilename = vsg::removeExtension(externalTextureFilename).concat(".vsgb");
                break;
            }

            // actually write out the texture.. this need only be done once per texture!
            if (externalObjects->entries.count(externalTextureFilename) == 0)
            {
                switch (externalTextureFormat)
                {
                case TextureFormat::native:
                    break; // nothing to do
                case TextureFormat::vsgt:
                    vsg::write(samplerImage.data, externalTextureFilename, options);
                    break;
                case TextureFormat::vsgb:
                    vsg::write(samplerImage.data, externalTextureFilename, options);
                    break;
                }

                externalObjects->add(externalTextureFilename, samplerImage.data);
            }
        }

        return samplerImage;
    }
    else
    {
        return {};
    }
}

void SceneConverter::convert(const aiMaterial* material, vsg::DescriptorConfigurator& convertedMaterial)
{
    auto& defines = convertedMaterial.defines;

    vsg::PbrMaterial pbr;
    bool hasPbrSpecularGlossiness = getColor(material, AI_MATKEY_COLOR_SPECULAR, pbr.specularFactor);

    convertedMaterial.blending = hasAlphaBlend(material);

    if ((options->getValue(assimp::two_sided, convertedMaterial.two_sided) || (material->Get(AI_MATKEY_TWOSIDED, convertedMaterial.two_sided) == AI_SUCCESS)) && convertedMaterial.two_sided)
    {
        defines.insert("VSG_TWO_SIDED_LIGHTING");
    }

    aiShadingMode shadingMode = {};
    material->Get(AI_MATKEY_SHADING_MODEL, shadingMode);

    bool phongShading = (shadingMode == aiShadingMode_Phong) || (shadingMode == aiShadingMode_Blinn);
    bool pbrShading = (getColor(material, AI_MATKEY_BASE_COLOR, pbr.baseColorFactor) || hasPbrSpecularGlossiness);

    if (pbrShading && !phongShading)
    {
        // PBR path
        convertedMaterial.shaderSet = getOrCreatePbrShaderSet();

        auto& materialBinding = convertedMaterial.shaderSet->getDescriptorBinding("material");
        targetMaterialCoordinateSpace = materialBinding.coordinateSpace;

        if (convertedMaterial.blending)
            pbr.alphaMask = 0.0f;

        if (hasPbrSpecularGlossiness)
        {
            defines.insert("VSG_WORKFLOW_SPECGLOSS");
            getColor(material, AI_MATKEY_COLOR_DIFFUSE, pbr.diffuseFactor);

            if (material->Get(AI_MATKEY_GLOSSINESS_FACTOR, pbr.specularFactor.a) != AI_SUCCESS)
            {
                if (float shininess; material->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS)
                    pbr.specularFactor.a = shininess / 1000;
            }
        }
        else
        {
            material->Get(AI_MATKEY_METALLIC_FACTOR, pbr.metallicFactor);
            material->Get(AI_MATKEY_ROUGHNESS_FACTOR, pbr.roughnessFactor);
        }

        getColor(material, AI_MATKEY_COLOR_EMISSIVE, pbr.emissiveFactor);
        material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, pbr.alphaMaskCutoff);

        aiString alphaMode;
        if (material->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS && alphaMode == aiString("OPAQUE"))
            pbr.alphaMaskCutoff = 0.0f;

        // Note: in practice some of these textures may resolve to the same texture. For example,
        // the glTF spec says that ambient occlusion, roughness, and metallic values are mapped
        // respectively to red, green, and blue channels; the Blender glTF exporter can pack them
        // into one texture. It's not clear if anything should be done about that at the VSG level.
        SamplerData samplerImage;
        if (samplerImage = convertTexture(*material, aiTextureType_DIFFUSE); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("diffuseMap", samplerImage.data, samplerImage.sampler);
        }
        if (samplerImage = convertTexture(*material, aiTextureType_EMISSIVE); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("emissiveMap", samplerImage.data, samplerImage.sampler);
        }

        if (samplerImage = convertTexture(*material, aiTextureType_LIGHTMAP); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("aoMap", samplerImage.data, samplerImage.sampler);
        }

        if (samplerImage = convertTexture(*material, aiTextureType_NORMALS); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("normalMap", samplerImage.data, samplerImage.sampler);
        }

        // map either aiTextureType_METALNESS or aiTextureType_UNKNOWN to metal roughness.
        if (samplerImage = convertTexture(*material, aiTextureType_METALNESS); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("mrMap", samplerImage.data, samplerImage.sampler);
        }
        else if (samplerImage = convertTexture(*material, aiTextureType_UNKNOWN); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("mrMap", samplerImage.data, samplerImage.sampler);
        }

        if (samplerImage = convertTexture(*material, aiTextureType_SPECULAR); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("specularMap", samplerImage.data, samplerImage.sampler);
        }

        convertedMaterial.assignDescriptor("material", vsg::PbrMaterialValue::create(pbr));
    }
    else
    {
        // Phong shading
        convertedMaterial.shaderSet = getOrCreatePhongShaderSet();

        auto& materialBinding = convertedMaterial.shaderSet->getDescriptorBinding("material");
        targetMaterialCoordinateSpace = materialBinding.coordinateSpace;

        vsg::PhongMaterial mat;

        if (convertedMaterial.blending)
            mat.alphaMask = 0.0f;

        material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, mat.alphaMaskCutoff);
        getColor(material, AI_MATKEY_COLOR_AMBIENT, mat.ambient);
        const auto diffuseResult = getColor(material, AI_MATKEY_COLOR_DIFFUSE, mat.diffuse);
        const auto emissiveResult = getColor(material, AI_MATKEY_COLOR_EMISSIVE, mat.emissive);
        const auto specularResult = getColor(material, AI_MATKEY_COLOR_SPECULAR, mat.specular);

        unsigned int maxValue = 1;
        float strength = 1.0f;
        // any factors for other material parameters get multiplied in by AssImp before we see them, but the specular factor is different
        // doing it like this means it's happening after conversion to the target colour space, which is sensible, but not necessarily what old data expects
        if (aiGetMaterialFloatArray(material, AI_MATKEY_SHININESS_STRENGTH, &strength, &maxValue) == AI_SUCCESS)
            mat.specular *= strength;

        maxValue = 1;
        if (aiGetMaterialFloatArray(material, AI_MATKEY_SHININESS, &mat.shininess, &maxValue) != AI_SUCCESS)
        {
            mat.shininess = 0.0f;
            mat.specular.set(0.0f, 0.0f, 0.0f, 0.0f);
        }

        if (mat.shininess < 0.01f)
        {
            mat.shininess = 0.0f;
            mat.specular.set(0.0f, 0.0f, 0.0f, 0.0f);
        }

        SamplerData samplerImage;
        if (samplerImage = convertTexture(*material, aiTextureType_DIFFUSE); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("diffuseMap", samplerImage.data, samplerImage.sampler);

            if (diffuseResult != AI_SUCCESS)
                mat.diffuse.set(1.0f, 1.0f, 1.0f, 1.0f);
        }

        if (samplerImage = convertTexture(*material, aiTextureType_EMISSIVE); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("emissiveMap", samplerImage.data, samplerImage.sampler);

            if (emissiveResult != AI_SUCCESS)
                mat.emissive.set(1.0f, 1.0f, 1.0f, 1.0f);
        }

        if (samplerImage = convertTexture(*material, aiTextureType_LIGHTMAP); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("aoMap", samplerImage.data, samplerImage.sampler);
        }
        else if (samplerImage = convertTexture(*material, aiTextureType_AMBIENT); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("aoMap", samplerImage.data, samplerImage.sampler);
        }

        if (samplerImage = convertTexture(*material, aiTextureType_NORMALS); samplerImage.data.valid())
        {
            convertedMaterial.assignTexture("normalMap", samplerImage.data, samplerImage.sampler);
        }

        if (samplerImage = convertTexture(*material, aiTextureType_SPECULAR); samplerImage.data.valid())
        {
            // TODO phong shader doesn't have a specular texture map at present
            convertedMaterial.assignTexture("specularMap", samplerImage.data, samplerImage.sampler);

            if (specularResult != AI_SUCCESS)
                mat.specular.set(1.0f, 1.0f, 1.0f, 1.0f);
        }

        convertedMaterial.assignDescriptor("material", vsg::PhongMaterialValue::create(mat));
    }

    if (jointSampler)
    {
        // useful reference for GLTF animation support
        // https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/figures/gltfOverview-2.0.0d.png
        convertedMaterial.assignDescriptor("jointMatrices", jointSampler->jointMatrices);
    }
}

vsg::ref_ptr<vsg::Data> SceneConverter::createIndices(const aiMesh* mesh, unsigned int numIndicesPerFace, uint32_t numIndices)
{
    if (mesh->mNumVertices > 16384)
    {
        auto indices = vsg::uintArray::create(numIndices);
        auto itr = indices->begin();
        for (unsigned int j = 0; j < mesh->mNumFaces; ++j)
        {
            const auto& face = mesh->mFaces[j];
            if (face.mNumIndices == numIndicesPerFace)
            {
                for (unsigned int i = 0; i < numIndicesPerFace; ++i) (*itr++) = static_cast<uint32_t>(face.mIndices[i]);
            }
        }
        return indices;
    }
    else
    {
        auto indices = vsg::ushortArray::create(numIndices);
        auto itr = indices->begin();
        for (unsigned int j = 0; j < mesh->mNumFaces; ++j)
        {
            const auto& face = mesh->mFaces[j];
            if (face.mNumIndices == numIndicesPerFace)
            {
                for (unsigned int i = 0; i < numIndicesPerFace; ++i) (*itr++) = static_cast<uint16_t>(face.mIndices[i]);
            }
        }
        return indices;
    }
}

void SceneConverter::convert(const aiMesh* mesh, vsg::ref_ptr<vsg::Node>& node)
{
    if (convertedMaterials.size() <= mesh->mMaterialIndex)
    {
        vsg::warn("Warning:  mesh (", mesh, ") mesh->mMaterialIndex = ", mesh->mMaterialIndex, " exceeds available materials.size()= ", convertedMaterials.size());
        return;
    }

    if (mesh->mNumVertices == 0 || mesh->mVertices == nullptr)
    {
        vsg::warn("Warning:  mesh (", mesh, ") no vertices data, mesh->mNumVertices = ", mesh->mNumVertices, " mesh->mVertices = ", mesh->mVertices);
        return;
    }

    if (mesh->mNumFaces == 0 || mesh->mFaces == nullptr)
    {
        vsg::warn("Warning:  mesh (", mesh, ") no mesh data, mesh->mNumFaces = ", mesh->mNumFaces);
        return;
    }

    std::string name = mesh->mName.C_Str();
    auto material = convertedMaterials[mesh->mMaterialIndex];

    // count the number of indices of each type
    uint32_t numTriangleIndices = 0;
    uint32_t numLineIndices = 0;
    uint32_t numPointIndices = 0;
    for (unsigned int j = 0; j < mesh->mNumFaces; ++j)
    {
        const auto& face = mesh->mFaces[j];
        if (face.mNumIndices == 3)
            numTriangleIndices += 3;
        else if (face.mNumIndices == 2)
            numLineIndices += 2;
        else if (face.mNumIndices == 1)
            numPointIndices += 1;
        else
        {
            vsg::warn("Warning: unsupported number of indices on face ", face.mNumIndices);
        }
    }

    int numPrimitiveTypes = 0;
    if (numTriangleIndices > 0) ++numPrimitiveTypes;
    if (numLineIndices > 0) ++numPrimitiveTypes;
    if (numPointIndices > 0) ++numPrimitiveTypes;

    if (numPrimitiveTypes > 1)
    {
        vsg::warn("Warning: more than one primitive type required, numTriangleIndices = ", numTriangleIndices, ", numLineIndices = ", numLineIndices, ", numPointIndices = ", numPointIndices);
    }

    unsigned int numIndicesPerFace = 0;
    uint32_t numIndices = 0;
    VkPrimitiveTopology topology{};
    if (numTriangleIndices > 0)
    {
        topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        numIndicesPerFace = 3;
        numIndices = numTriangleIndices;
    }
    else if (numLineIndices > 0)
    {
        topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        numIndicesPerFace = 2;
        numIndices = numLineIndices;
    }
    else if (numPointIndices > 0)
    {
        topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        numIndicesPerFace = 1;
        numIndices = numPointIndices;
    }
    else
    {
        vsg::warn("Warning: no primitive indices ");
        return;
    }

    auto config = vsg::GraphicsPipelineConfigurator::create(material->shaderSet);
    config->descriptorConfigurator = material;
    if (options) config->assignInheritedState(options->inheritedState);

    auto indices = createIndices(mesh, numIndicesPerFace, numIndices);

    vsg::DataList vertexArrays;
    auto vertices = vsg::vec3Array::create(mesh->mNumVertices);
    std::memcpy(vertices->dataPointer(), mesh->mVertices, mesh->mNumVertices * 12);
    config->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vertices);

    if (mesh->mNormals)
    {
        auto normals = vsg::vec3Array::create(mesh->mNumVertices);
        std::memcpy(normals->dataPointer(), mesh->mNormals, mesh->mNumVertices * 12);
        config->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, normals);
    }
    else
    {
        auto normal = vsg::vec3Value::create(vsg::vec3(0.0f, 0.0f, 1.0f));
        config->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_INSTANCE, normal);
    }

    if (mesh->mTextureCoords[0])
    {
        auto dest_texcoords = vsg::vec2Array::create(mesh->mNumVertices);
        auto src_texcoords = mesh->mTextureCoords[0];
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
        {
            auto& tc = src_texcoords[i];
            dest_texcoords->at(i).set(tc[0], tc[1]);
        }
        config->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, dest_texcoords);
    }
    else
    {
        auto texcoord = vsg::vec2Value::create(vsg::vec2(0.0f, 0.0f));
        config->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_INSTANCE, texcoord);
    }

    vsg::AttributeBinding& colorBinding = config->shaderSet->getAttributeBinding("vsg_Color");
    targetVertexColorSpace = colorBinding.coordinateSpace;

    if (mesh->mColors[0])
    {
        auto colors = vsg::vec4Array::create(mesh->mNumVertices);
        std::memcpy(colors->dataPointer(), mesh->mColors[0], mesh->mNumVertices * 16);
        vsg::convert(colors->size(), &(colors->at(0)), sourceVertexColorSpace, targetVertexColorSpace);
        config->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_VERTEX, colors);

        vsg::debug("vsg::convert(", colors, ", ", sourceVertexColorSpace, ", ", targetVertexColorSpace, ")");
    }
    else
    {
        auto colors = vsg::vec4Value::create(vsg::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        vsg::convert(colors->value(), sourceVertexColorSpace, targetVertexColorSpace);
        config->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, colors);

        vsg::debug("vsg::convert(", colors, ", ", sourceVertexColorSpace, ", ", targetVertexColorSpace, ")");
    }

    if (mesh->HasBones() && jointSampler)
    {
        // useful reference for GLTF animation support
        // https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/figures/gltfOverview-2.0.0d.png

        // now done as part of the aiMaterial/DescriptorConfigurator setup.
        // config->assignDescriptor("jointMatrices", jointSampler->jointMatrices);

        auto jointIndices = vsg::ivec4Array::create(mesh->mNumVertices, vsg::ivec4(0, 0, 0, 0));
        auto jointWeights = vsg::vec4Array::create(mesh->mNumVertices, vsg::vec4(0.0f, 0.0f, 0.0f, 0.0f));

        auto objects = vsg::Objects::create();
        objects->addChild(jointIndices);
        objects->addChild(jointWeights);

        std::vector<uint32_t> weightCounts(mesh->mNumVertices, 0);

        config->assignArray(vertexArrays, "vsg_JointIndices", VK_VERTEX_INPUT_RATE_VERTEX, jointIndices);
        config->assignArray(vertexArrays, "vsg_JointWeights", VK_VERTEX_INPUT_RATE_VERTEX, jointWeights);

        // vsg::info("\nProcessing bones");
        // vsg::info("mesh->mNumBones = ", mesh->mNumBones);
        // vsg::info("mesh->mNumVertices = ", mesh->mNumVertices);

        bool normalizeWeights = false;

        for (size_t i = 0; i < mesh->mNumBones; ++i)
        {
            aiBone* bone = mesh->mBones[i];

            aiMatrix4x4 m = bone->mOffsetMatrix;
            m.Transpose();

            jointSampler->offsetMatrices[i] = vsg::dmat4(vsg::mat4((float*)&m));

            // vsg::info("    bone[", i, "], bone->mName = ", bone->mName.C_Str());

            unsigned int boneIndex = bones[bone].index;

            //! The number of vertices affected by this bone.
            //! The maximum value for this member is #AI_MAX_BONE_WEIGHTS.
            for (unsigned int bwi = 0; bwi < bone->mNumWeights; ++bwi)
            {
                auto& vertexWeight = bone->mWeights[bwi];
                auto& jointIndex = jointIndices->at(vertexWeight.mVertexId);
                auto& jointWeight = jointWeights->at(vertexWeight.mVertexId);
                auto& weightCount = weightCounts[vertexWeight.mVertexId];
                if (weightCount < 4)
                {
                    jointIndex[weightCount] = boneIndex;
                    jointWeight[weightCount] = vertexWeight.mWeight;
                }
                else
                {
                    // too many weights associated with vertex so replace the smallest weight if it's smaller than the vertexWeight.mWeight
                    // and normalize the final result to ensure that weights all add up to 1.0
                    normalizeWeights = true;

                    unsigned int minWeightIndex = 0;
                    float minWeight = jointWeight[minWeightIndex];
                    for (unsigned int wi = 1; wi < 4; ++wi)
                    {
                        if (minWeight > jointWeight[wi])
                        {
                            minWeight = jointWeight[wi];
                            minWeightIndex = wi;
                        }
                    }
                    if (minWeight < vertexWeight.mWeight)
                    {
                        jointIndex[minWeightIndex] = boneIndex;
                        jointWeight[minWeightIndex] = vertexWeight.mWeight;
                    }
                }
                ++weightCount;
            }
        }

        if (normalizeWeights)
        {
            for (auto& weights : *jointWeights)
            {
                float totalWeight = weights[0] + weights[1] + weights[2] + weights[3];
                if (totalWeight != 0.0f)
                {
                    weights /= totalWeight;
                }
            }
        }
    }

    auto vid = vsg::VertexIndexDraw::create();
    vid->assignArrays(vertexArrays);
    vid->assignIndices(indices);
    vid->indexCount = static_cast<uint32_t>(indices->valueCount());
    vid->instanceCount = 1;
    if (!name.empty()) vid->setValue("name", name);

    if (mesh->mNumAnimMeshes != 0)
    {
        vid->setValue("animationMeshes", mesh->mNumAnimMeshes);
    }

    // set the GraphicsPipelineStates to the required values.
    struct SetPipelineStates : public vsg::Visitor
    {
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        bool blending = false;
        bool two_sided = false;

        SetPipelineStates(VkPrimitiveTopology in_topology, bool in_blending, bool in_two_sided) :
            topology(in_topology), blending(in_blending), two_sided(in_two_sided) {}

        void apply(vsg::Object& object) { object.traverse(*this); }
        void apply(vsg::RasterizationState& rs)
        {
            if (two_sided) rs.cullMode = VK_CULL_MODE_NONE;
        }
        void apply(vsg::InputAssemblyState& ias) { ias.topology = topology; }
        void apply(vsg::ColorBlendState& cbs) { cbs.configureAttachments(blending); }
    } sps(topology, material->blending, material->two_sided);
    config->accept(sps);

    if (sharedObjects)
        sharedObjects->share(config, [](auto gpc) { gpc->init(); });
    else
        config->init();

    // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
    auto stateGroup = vsg::StateGroup::create();

    config->copyTo(stateGroup, sharedObjects);

    stateGroup->addChild(vid);

    if (material->blending)
    {
        vsg::ComputeBounds computeBounds;
        vid->accept(computeBounds);
        vsg::dvec3 center = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.5;

        auto depthSorted = vsg::DepthSorted::create();
        depthSorted->binNumber = 10;
        depthSorted->bound.set(center[0], center[1], center[2], radius);
        depthSorted->child = stateGroup;

        node = depthSorted;
    }
    else
    {
        node = stateGroup;
    }
}

vsg::ref_ptr<vsg::Node> SceneConverter::visit(const aiScene* in_scene, vsg::ref_ptr<const vsg::Options> in_options, const vsg::Path& ext)
{
    scene = in_scene;
    options = in_options;
    discardEmptyNodes = vsg::value<bool>(true, assimp::discard_empty_nodes, options);
    printAssimp = vsg::value<int>(0, assimp::print_assimp, options);
    externalTextures = vsg::value<bool>(false, assimp::external_textures, options);
    externalTextureFormat = vsg::value<TextureFormat>(TextureFormat::native, assimp::external_texture_format, options);
    culling = vsg::value<bool>(true, assimp::culling, options);
    topEmptyTransform = {};

    if (ext == ".gltf" || ext == ".glb")
    {
        sourceVertexColorSpace = vsg::CoordinateSpace::LINEAR;
        sourceMaterialColorSpace = vsg::CoordinateSpace::LINEAR;
    }
    else
    {
        sourceVertexColorSpace = vsg::CoordinateSpace::sRGB;
        sourceMaterialColorSpace = vsg::CoordinateSpace::sRGB;
    }

    if (options)
    {

        options->getValue(assimp::vertex_color_space, sourceVertexColorSpace);
        options->getValue(assimp::material_color_space, sourceMaterialColorSpace);
    }

    vsg::debug("vsgXchange::assimp sourceVertexColorSpace = ", sourceVertexColorSpace, ", sourceMaterialColorSpace = ", sourceMaterialColorSpace);

    std::string name = scene->mName.C_Str();

    if (options) sharedObjects = options->sharedObjects;
    if (!sharedObjects) sharedObjects = vsg::SharedObjects::create();
    if (externalTextures && !externalObjects) externalObjects = vsg::External::create();

    // collect the subgraph stats to help with decisions on which VSG node to use to represent aiNode.
    auto sceneStats = collectSubgraphStats(scene);
    if (!bones.empty())
    {
        jointSampler = vsg::JointSampler::create();
        jointSampler->jointMatrices = vsg::mat4Array::create(sceneStats.numBones);
        jointSampler->jointMatrices->properties.dataVariance = vsg::DYNAMIC_DATA;
        jointSampler->offsetMatrices.resize(sceneStats.numBones);
    }

    processAnimations();
    processCameras();
    processLights();

    // convert the materials
    convertedMaterials.resize(scene->mNumMaterials);
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
    {
        convertedMaterials[i] = vsg::DescriptorConfigurator::create();
        convert(scene->mMaterials[i], *convertedMaterials[i]);
    }

    // convert the meshes
    convertedMeshes.resize(scene->mNumMeshes);
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
    {
        convert(scene->mMeshes[i], convertedMeshes[i]);
    }

    auto vsg_scene = visit(scene->mRootNode, 0);
    if (!vsg_scene)
    {
        if (scene->mNumMeshes == 1)
        {
            vsg_scene = convertedMeshes[0];
        }
        else if (scene->mNumMeshes > 1)
        {
            auto group = vsg::Group::create();
            for (auto& node : convertedMeshes)
            {
                if (node) group->addChild(node);
            }
        }

        if (!vsg_scene) return {};
    }

    // decorate scene graph with transform if required to standardize the coordinate frame
    if (auto transform = processCoordinateFrame(ext))
    {
        transform->addChild(vsg_scene);
        vsg_scene = transform;

        // TODO check if subgraph requires culling
        // transform->subgraphRequiresLocalFrustum = false;
    }

    if (!animations.empty())
    {
        if (jointSampler)
        {
            for (auto& animation : animations)
            {
                bool assignJointSampler = false;
                for (auto sampler : animation->samplers)
                {
                    if (auto transformSampler = sampler.cast<vsg::TransformSampler>())
                    {
                        assignJointSampler = true;
                        break;
                    }
                }
                if (assignJointSampler)
                {
                    animation->samplers.push_back(jointSampler);
                }
            }

#if 0
            struct FindTopJoint : public vsg::Visitor
            {
                vsg::ref_ptr<vsg::Joint> topJoint;
                void apply(vsg::Object& object) override { object.traverse(*this); }
                void apply(vsg::Joint& joint) override { topJoint = &joint; }
            };

            auto topJoint = vsg::visit<FindTopJoint>(vsg_scene).topJoint;

            jointSampler->subgraph = topJoint;
#endif

            jointSampler->subgraph = topEmptyTransform;
        }

        auto animationGroup = vsg::AnimationGroup::create();
        animationGroup->animations = animations;
        animationGroup->addChild(vsg_scene);
        vsg_scene = animationGroup;
    }
    else
    {
    }

    if (!name.empty()) vsg_scene->setValue("name", name);

    if (printAssimp > 0) print(std::cout, in_scene, vsg::indentation{0});

    if (culling)
    {
        auto bounds = vsg::visit<vsg::ComputeBounds>(vsg_scene).bounds;
        vsg::dsphere bs((bounds.max + bounds.min) * 0.5, vsg::length(bounds.max - bounds.min) * 0.5);
        auto cullNode = vsg::CullNode::create(bs, vsg_scene);
        vsg_scene = cullNode;
    }

    return vsg_scene;
}

vsg::ref_ptr<vsg::Node> SceneConverter::visit(const aiNode* node, int depth)
{
    vsg::Group::Children children;

    auto& stats = subgraphStats[node];
    bool subgraphActive = stats.numMesh;

    std::string name = node->mName.C_Str();

    // assign any cameras
    if (auto camera_itr = cameraMap.find(name); camera_itr != cameraMap.end())
    {
        children.push_back(camera_itr->second);
        subgraphActive = true;
    }

    // assign any lights
    if (auto light_itr = lightMap.find(name); light_itr != lightMap.end())
    {
        children.push_back(light_itr->second);
        subgraphActive = true;
    }

    // assign the meshes
    for (unsigned int i = 0; i < node->mNumMeshes; ++i)
    {
        auto mesh_index = node->mMeshes[i];
        if (auto child = convertedMeshes[mesh_index])
        {
            children.push_back(child);
        }
        subgraphActive = true;
    }

    // visit the children
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        if (auto child = visit(node->mChildren[i], depth + 1))
        {
            children.push_back(child);
        }
    }

    bool boneTransform = boneTransforms.find(node) != boneTransforms.end();
    bool animationTransform = !name.empty() && (animationTransforms.find(name) != animationTransforms.end());
    if (boneTransform || animationTransform)
    {
        aiMatrix4x4 m = node->mTransformation;
        m.Transpose();

        auto matrix(vsg::mat4((float*)&m));
        vsg::ref_ptr<vsg::Node> transform;

        if (boneTransform)
        {
            auto joint = vsg::Joint::create();
            joint->index = boneTransforms[node];
            joint->name = name;
            joint->matrix = matrix;

#if 0
            for(auto& child : children)
            {
                if (auto joint_child = child.cast<vsg::Joint>())
                {
                    joint->children.push_back(joint_child);
                }
                else
                {
                    vsg::warn("Child of Joint invalid : ", child, " must be of type vsg::Joint.");
                }
            }
#else
            joint->children = children;
#endif
            transform = joint;
        }
        else
        {
            auto mt = vsg::MatrixTransform::create();
            if (!name.empty()) mt->setValue("name", name);
            mt->matrix = matrix;
            mt->children = children;

            transform = mt;
        }

        if (!subgraphActive || boneTransform) topEmptyTransform = transform;

        subgraphStats[node].vsg_object = transform;

        // wire up transform to animation keyframes
        for (auto& animation : animations)
        {
            for (auto sampler : animation->samplers)
            {
                if (sampler->name == name)
                {
                    if (auto transformSampler = sampler.cast<vsg::TransformSampler>())
                    {
                        transformSampler->object = transform;
                    }
                }
            }
        }

        return transform;
    }
    if (discardEmptyNodes && node->mTransformation.IsIdentity())
    {
        if (children.size() == 1 && name.empty()) return children[0];

        auto group = vsg::Group::create();
        group->children = children;
        if (!name.empty()) group->setValue("name", name);

        subgraphStats[node].vsg_object = group;

        return group;
    }
    else
    {
        aiMatrix4x4 m = node->mTransformation;
        m.Transpose();

        auto transform = vsg::MatrixTransform::create(vsg::dmat4(vsg::mat4((float*)&m)));
        transform->children = children;
        if (!name.empty()) transform->setValue("name", name);

        // TODO check if subgraph requires culling
        //transform->subgraphRequiresLocalFrustum = false;

        subgraphStats[node].vsg_object = transform;

        return transform;
    }
}

void SceneConverter::processAnimations()
{

    for (unsigned int ai = 0; ai < scene->mNumAnimations; ++ai)
    {
        auto animation = scene->mAnimations[ai];

        // convert the time values the VSG uses to seconds.
        const double timeScale = 1.0 / animation->mTicksPerSecond;

        auto vsg_animation = vsg::Animation::create();
        vsg_animation->name = animation->mName.C_Str();
        animations.push_back(vsg_animation);

        double epsilion = 1e-6;
        for (unsigned int ci = 0; ci < animation->mNumChannels; ++ci)
        {
            auto nodeAnim = animation->mChannels[ci];

            auto vsg_transformKeyframes = vsg::TransformKeyframes::create();
            vsg_transformKeyframes->name = nodeAnim->mNodeName.C_Str();

            auto transformSampler = vsg::TransformSampler::create();
            transformSampler->name = nodeAnim->mNodeName.C_Str();
            transformSampler->keyframes = vsg_transformKeyframes;

            vsg_animation->samplers.push_back(transformSampler);

            // record the node name that will be animated.
            animationTransforms.insert(vsg_transformKeyframes->name);

            if (nodeAnim->mNumPositionKeys > 0)
            {
                unsigned numUniquePositions = 1;
                for (unsigned int si = 1; si < nodeAnim->mNumPositionKeys; ++si)
                {
                    auto& prev = nodeAnim->mPositionKeys[si - 1];
                    auto& curr = nodeAnim->mPositionKeys[si];
                    if ((std::fabs(prev.mValue.x - curr.mValue.x) > epsilion) ||
                        (std::fabs(prev.mValue.y - curr.mValue.y) > epsilion) ||
                        (std::fabs(prev.mValue.z - curr.mValue.z) > epsilion))
                    {
                        ++numUniquePositions;
                    }
                }

                auto& positions = vsg_transformKeyframes->positions;
                if (numUniquePositions <= 1)
                {
                    positions.resize(1);
                    auto& positionKey = nodeAnim->mPositionKeys[0];
                    positions[0].time = positionKey.mTime * timeScale;
                    positions[0].value.set(positionKey.mValue.x, positionKey.mValue.y, positionKey.mValue.z);
                }
                else
                {
                    positions.resize(nodeAnim->mNumPositionKeys);
                    for (unsigned int pi = 0; pi < nodeAnim->mNumPositionKeys; ++pi)
                    {
                        auto& positionKey = nodeAnim->mPositionKeys[pi];
                        positions[pi].time = positionKey.mTime * timeScale;
                        positions[pi].value.set(positionKey.mValue.x, positionKey.mValue.y, positionKey.mValue.z);
                    }
                }
            }

            if (nodeAnim->mNumRotationKeys > 0)
            {
                unsigned numUniqueRotations = 1;
                for (unsigned int si = 1; si < nodeAnim->mNumRotationKeys; ++si)
                {
                    auto& prev = nodeAnim->mRotationKeys[si - 1];
                    auto& curr = nodeAnim->mRotationKeys[si];
                    if ((std::fabs(prev.mValue.x - curr.mValue.x) > epsilion) ||
                        (std::fabs(prev.mValue.y - curr.mValue.y) > epsilion) ||
                        (std::fabs(prev.mValue.z - curr.mValue.z) > epsilion) ||
                        (std::fabs(prev.mValue.w - curr.mValue.w) > epsilion))
                    {
                        ++numUniqueRotations;
                    }
                }

                auto& rotations = vsg_transformKeyframes->rotations;
                if (numUniqueRotations <= 1)
                {
                    rotations.resize(1);
                    auto& rotationKey = nodeAnim->mRotationKeys[0];
                    rotations[0].time = rotationKey.mTime * timeScale;
                    rotations[0].value.set(rotationKey.mValue.x, rotationKey.mValue.y, rotationKey.mValue.z, rotationKey.mValue.w);
                }
                else
                {
                    rotations.resize(nodeAnim->mNumRotationKeys);
                    for (unsigned int ri = 0; ri < nodeAnim->mNumRotationKeys; ++ri)
                    {
                        auto& rotationKey = nodeAnim->mRotationKeys[ri];
                        rotations[ri].time = rotationKey.mTime * timeScale;
                        rotations[ri].value.set(rotationKey.mValue.x, rotationKey.mValue.y, rotationKey.mValue.z, rotationKey.mValue.w);
                    }
                }
            }

            if (nodeAnim->mNumScalingKeys > 0)
            {
                unsigned numUniqueScales = 1;
                for (unsigned int si = 1; si < nodeAnim->mNumScalingKeys; ++si)
                {
                    auto& prev = nodeAnim->mScalingKeys[si - 1];
                    auto& curr = nodeAnim->mScalingKeys[si];
                    if ((std::fabs(prev.mValue.x - curr.mValue.x) > epsilion) ||
                        (std::fabs(prev.mValue.y - curr.mValue.y) > epsilion) ||
                        (std::fabs(prev.mValue.z - curr.mValue.z) > epsilion))
                    {
                        ++numUniqueScales;
                    }
                }
                auto& scales = vsg_transformKeyframes->scales;
                if (numUniqueScales <= 1)
                {
                    scales.resize(1);
                    auto& scalingKey = nodeAnim->mScalingKeys[0];
                    scales[0].time = scalingKey.mTime * timeScale;
                    scales[0].value.set(scalingKey.mValue.x, scalingKey.mValue.y, scalingKey.mValue.z);
                }
                else
                {
                    scales.resize(nodeAnim->mNumScalingKeys);
                    for (unsigned int si = 0; si < nodeAnim->mNumScalingKeys; ++si)
                    {
                        auto& scalingKey = nodeAnim->mScalingKeys[si];
                        scales[si].time = scalingKey.mTime * timeScale;
                        scales[si].value.set(scalingKey.mValue.x, scalingKey.mValue.y, scalingKey.mValue.z);
                    }
                }
            }
        }

        for (unsigned int moi = 0; moi < animation->mNumMorphMeshChannels; ++moi)
        {
            auto meshMorphAnim = animation->mMorphMeshChannels[moi];

            auto vsg_morphKeyframes = vsg::MorphKeyframes::create();
            vsg_morphKeyframes->name = meshMorphAnim->mName.C_Str();

            auto morphSampler = vsg::MorphSampler::create();
            morphSampler->name = meshMorphAnim->mName.C_Str();
            morphSampler->keyframes = vsg_morphKeyframes;

            vsg_animation->samplers.push_back(morphSampler);

            auto& keyFrames = vsg_morphKeyframes->keyframes;
            keyFrames.resize(meshMorphAnim->mNumKeys);

            for (unsigned int mki = 0; mki < meshMorphAnim->mNumKeys; ++mki)
            {
                auto& morphKey = meshMorphAnim->mKeys[mki];
                keyFrames[mki].time = morphKey.mTime * timeScale;

                auto& values = keyFrames[mki].values;
                auto& weights = keyFrames[mki].weights;
                values.resize(morphKey.mNumValuesAndWeights);
                weights.resize(morphKey.mNumValuesAndWeights);

                for (unsigned int vi = 0; vi < morphKey.mNumValuesAndWeights; ++vi)
                {
                    values[vi] = morphKey.mValues[vi];
                    weights[vi] = morphKey.mWeights[vi];
                }
            }
        }
    }
}

void SceneConverter::processCameras()
{
    if (scene->mNumCameras > 0)
    {
        for (unsigned int li = 0; li < scene->mNumCameras; ++li)
        {
            auto* camera = scene->mCameras[li];
            auto vsg_camera = vsg::Camera::create();
            vsg_camera->name = camera->mName.C_Str();

            vsg_camera->viewMatrix = vsg::LookAt::create(
                vsg::dvec3(camera->mPosition[0], camera->mPosition[1], camera->mPosition[2]), // eye
                vsg::dvec3(camera->mLookAt[0], camera->mLookAt[1], camera->mLookAt[2]),       // center
                vsg::dvec3(camera->mUp[0], camera->mUp[1], camera->mUp[2])                    // up
            );

            double verticalFOV = vsg::degrees(atan(tan(static_cast<double>(camera->mHorizontalFOV) * 0.5) / camera->mAspect) * 2.0);
            vsg_camera->projectionMatrix = vsg::Perspective::create(verticalFOV, camera->mAspect, camera->mClipPlaneNear, camera->mClipPlaneFar);

            // the aiNodes in the scene with the same name as the camera will provide a place to add the camera, this is added in the node handling in the for loop below.
            cameraMap[vsg_camera->name] = vsg_camera;
        }
    }
}

void SceneConverter::processLights()
{
    if (scene->mNumLights > 0)
    {
        auto setColorAndIntensity = [&](const aiLight& light, vsg::Light& vsg_light) -> void {
            vsg_light.color = convert(light.mColorDiffuse);
            float maxValue = std::max(std::max(vsg_light.color.r, vsg_light.color.g), vsg_light.color.b);
            if (maxValue > 0.0)
            {
                vsg_light.color /= maxValue;
                vsg_light.intensity = maxValue;
            }
            else
            {
                vsg_light.intensity = 0.0;
            }
        };

        for (unsigned int li = 0; li < scene->mNumLights; ++li)
        {
            auto* light = scene->mLights[li];
            switch (light->mType)
            {
            case (aiLightSource_UNDEFINED): {
                auto vsg_light = vsg::Light::create();
                setColorAndIntensity(*light, *vsg_light);
                vsg_light->name = light->mName.C_Str();
                vsg_light->setValue("light_type", "UNDEFINED");
                lightMap[vsg_light->name] = vsg_light;
                break;
            }
            case (aiLightSource_DIRECTIONAL): {
                auto vsg_light = vsg::DirectionalLight::create();
                setColorAndIntensity(*light, *vsg_light);
                vsg_light->name = light->mName.C_Str();
                vsg_light->direction = dconvert(light->mDirection);
                lightMap[vsg_light->name] = vsg_light;
                break;
            }
            case (aiLightSource_POINT): {
                auto vsg_light = vsg::PointLight::create();
                setColorAndIntensity(*light, *vsg_light);
                vsg_light->name = light->mName.C_Str();
                vsg_light->position = dconvert(light->mDirection);
                lightMap[vsg_light->name] = vsg_light;
                break;
            }
            case (aiLightSource_SPOT): {
                auto vsg_light = vsg::SpotLight::create();
                setColorAndIntensity(*light, *vsg_light);
                vsg_light->name = light->mName.C_Str();
                vsg_light->position = dconvert(light->mDirection);
                vsg_light->direction = dconvert(light->mDirection);
                vsg_light->innerAngle = light->mAngleInnerCone;
                vsg_light->outerAngle = light->mAngleOuterCone;
                lightMap[vsg_light->name] = vsg_light;
                break;
            }
            case (aiLightSource_AMBIENT): {
                auto vsg_light = vsg::AmbientLight::create();
                setColorAndIntensity(*light, *vsg_light);
                vsg_light->name = light->mName.C_Str();
                lightMap[vsg_light->name] = vsg_light;
                break;
            }
            case (aiLightSource_AREA): {
                auto vsg_light = vsg::Light::create();
                setColorAndIntensity(*light, *vsg_light);
                vsg_light->name = light->mName.C_Str();
                vsg_light->setValue("light_type", "AREA");
                lightMap[vsg_light->name] = vsg_light;
                break;
            }
            default:
                break;
            }
        }
    }
}

vsg::ref_ptr<vsg::MatrixTransform> SceneConverter::processCoordinateFrame(const vsg::Path& ext)
{
    vsg::CoordinateConvention source_coordinateConvention = vsg::CoordinateConvention::Y_UP;

    if (auto itr = options->formatCoordinateConventions.find(ext); itr != options->formatCoordinateConventions.end())
    {
        source_coordinateConvention = itr->second;
    }

    if (scene->mMetaData)
    {
        int upAxis = 1;
        if (scene->mMetaData->Get("UpAxis", upAxis))
        {
            if (upAxis == 0)
                source_coordinateConvention = vsg::CoordinateConvention::X_UP;
            else if (upAxis == 1)
                source_coordinateConvention = vsg::CoordinateConvention::Y_UP;
            else
                source_coordinateConvention = vsg::CoordinateConvention::Z_UP;

            // unclear on how to interpret the UpAxisSign so will leave it unused for now.
            // int upAxisSign = 1;
            // scene->mMetaData->Get("UpAxisSign", upAxisSign);
        }
    }

    vsg::dmat4 matrix;
    if (vsg::transform(source_coordinateConvention, options->sceneCoordinateConvention, matrix))
    {
        return vsg::MatrixTransform::create(matrix);
    }
    else
    {
        return {};
    }
}
