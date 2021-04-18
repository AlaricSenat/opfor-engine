#include "Model.hpp"

#include "Mesh.hpp"
#include "TextureManager.hpp"
#include "opfor/core/Application.hpp"
#include "opfor/core/ImageLoader.hpp"
#include "tiny_gltf.h"
#include <glm/vec3.hpp>
#include <src/components/AnimationComponent.hpp>

namespace opfor
{
static Optional<Vector<unsigned int>> TinyProcessNode(tinygltf::Node const &node, tinygltf::Model const &model,
                                                      Vector<String> const &materials)
{
    Vector<unsigned int> allMeshes;

    if (node.mesh >= 0)
    {

        auto const &mesh = model.meshes[node.mesh];
        opfor::Mesh current;

        for (auto const &primitive : mesh.primitives)
        {

            // Helper Lambda
            // Returns the count and the attributes
            // TODO: Return the number of components per element (vec{2,3,4}, scalar, ...)
            auto getVertex = [&](String const &attributeName) -> Optional<std::pair<size_t, const float *>> {
                Optional<std::pair<size_t, const float *>> ret;

                auto const &attribute = primitive.attributes.find(attributeName);

                if (attribute == primitive.attributes.end())
                {
                    return ret;
                }

                auto const [name, indice] = *attribute;

                tinygltf::Accessor const &accessor = model.accessors[indice];
                tinygltf::BufferView const &bufferView = model.bufferViews[accessor.bufferView];
                tinygltf::Buffer const &buffer = model.buffers[bufferView.buffer];

                ret = std::pair<size_t, const float *>(
                    accessor.count,
                    reinterpret_cast<const float *>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]));
                return ret;
            };

            // Get data for indices
            tinygltf::Accessor const &indAccessor = model.accessors[primitive.indices];
            tinygltf::BufferView const &indBufferView = model.bufferViews[indAccessor.bufferView];
            tinygltf::Buffer const &indBuffer = model.buffers[indBufferView.buffer];

            auto positions = getVertex("POSITION");
            auto normals = getVertex("NORMAL");
            auto tangents = getVertex("TANGENT");
            auto texcoords = getVertex("TEXCOORD_0");
            auto indices =
                reinterpret_cast<const GLushort *>(&indBuffer.data[indBufferView.byteOffset + indAccessor.byteOffset]);

            // Build the mesh
            // --------------

            if (positions.has_value())
            {
                for (size_t i = 0; i < positions.value().first; i++)
                {
                    current.addPosition({positions.value().second[i * 3 + 0], positions.value().second[i * 3 + 1],
                                         positions.value().second[i * 3 + 2]});
                }
            }

            if (normals.has_value())
            {
                for (size_t i = 0; i < normals.value().first; i++)
                {
                    current.addNormal({normals.value().second[i * 3 + 0], normals.value().second[i * 3 + 1],
                                       normals.value().second[i * 3 + 2]});
                }
            }

            if (tangents.has_value())
            {
                for (size_t i = 0; i < tangents.value().first; i++)
                {
                    current.addTangent({tangents.value().second[i * 4 + 0], tangents.value().second[i * 4 + 1],
                                        tangents.value().second[i * 4 + 2], tangents.value().second[i * 4 + 3]});
                }
            }

            if (texcoords.has_value())
            {
                for (size_t i = 0; i < texcoords.value().first; i++)
                {
                    current.addUv({texcoords.value().second[i * 2 + 0], texcoords.value().second[i * 2 + 1]});
                }
            }

            // Component type for indices is SCALAR, that means the count must be divided by 3 when building
            // triangles so that we do not overrun the buffer
            for (size_t i = 0; i < indAccessor.count / 3; i++)
            {
                current.addTriangle({static_cast<GLuint>(indices[i * 3 + 0]), static_cast<GLuint>(indices[i * 3 + 1]),
                                     static_cast<GLuint>(indices[i * 3 + 2])});
            }

            current.build();

            if (primitive.material != -1)
            {
                current.SetPbrMaterial(materials[primitive.material]);
            }

            // Register this mesh to the engine and save its index
            allMeshes.push_back(opfor::Application::Get().AddMesh(std::move(current)));
        }
    }

    for (auto const &child : node.children)
    {
        auto meshes = TinyProcessNode(model.nodes[child], model, materials);
        if (meshes.has_value())
        {
            allMeshes.insert(allMeshes.end(), meshes.value().begin(), meshes.value().end());
        }
    }

    if (allMeshes.size() > 0)
    {
        return allMeshes;
    }

    return std::nullopt;
}

Optional<Vector<unsigned int>> Model::TinyLoader(String const &path)
{
    Optional<Vector<unsigned int>> meshes_ret;

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    String err;
    String warn;

    OP4_CORE_INFO("Parsing model \"{:s}\".", path);
    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, path);

    if (!warn.empty())
    {
        OP4_CORE_WARNING(warn);
    }
    if (!err.empty())
    {
        OP4_CORE_ERROR(err);
    }
    if (ret == false)
    {
        return meshes_ret;
    }

    String modelName;

    auto const basename_idx = path.find_last_of("/");
    auto const extension_idx = path.find_last_of(".");
    if (basename_idx != String::npos)
    {
        if (extension_idx != String::npos)
        {
            modelName = path.substr(basename_idx + 1, extension_idx - (basename_idx + 1));
        }
        else
        {
            modelName = path.substr(basename_idx + 1);
        }
    }
    else if (extension_idx != String::npos)
    {
        modelName = path.substr(0, extension_idx);
    }
    else
    {
        modelName = path;
    }
    OP4_CORE_DEBUG("Model name: \"{:s}\".", modelName);

    // Load Materials
    // --------------
    Vector<String> pbrMaterials; // Save the materials' index to be able to set the material of the mesh later
    String directory = path.substr(0, path.find_last_of('/')) + "/";

    size_t currentMaterialIndex = 0;
    for (auto const &material : model.materials)
    {

        PbrMaterial pbrMaterial;

        pbrMaterial.Name = modelName + "-" + std::to_string(currentMaterialIndex);

        float r = static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[0]);
        float g = static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[1]);
        float b = static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[2]);
        float a = static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[3]);
        pbrMaterial.BaseColor = {r, g, b, a};

        auto const baseColorTexture = material.pbrMetallicRoughness.baseColorTexture.index;
        if (baseColorTexture >= 0)
        {
            pbrMaterial.Albedo = model.images[model.textures[baseColorTexture].source].uri;

            auto texture = TextureManager::Get().Create2D(pbrMaterial.Albedo.value());

            opfor::TextureParameterList texParams = {
                {opfor::TextureParameterType::MignifyFilter, opfor::TextureParameterValue::LinearMipmapLinear},
                {opfor::TextureParameterType::MagnifyFilter, opfor::TextureParameterValue::Linear},
                {opfor::TextureParameterType::WrapS, opfor::TextureParameterValue::Repeat},
                {opfor::TextureParameterType::WrapT, opfor::TextureParameterValue::Repeat},
            };

            opfor::ImageLoader::Image img = opfor::ImageLoader::Load(directory + pbrMaterial.Albedo.value());

            if (img.nchannel == 3)
            {
                texture->SetInputFormat(opfor::DataFormat::RGB);
                texture->SetOutputFormat(opfor::DataFormat::RGB);
            }
            else if (img.nchannel == 4)
            {
                texture->SetInputFormat(opfor::DataFormat::RGBA);
                texture->SetOutputFormat(opfor::DataFormat::RGBA);
                texture->SetHasAlpha(true);
            }
            else
            {
                OP4_CORE_WARNING("img.nchannel = {} -- Not handled!", img.nchannel);
            }
            texture->SetDataType(opfor::DataType::UnsignedByte);
            texture->SetIsSRGB(true);
            texture->SetData(img.data.get());
            texture->SetSize(img.width, img.height);
            texture->SetTextureParameters(texParams);
            texture->SetGenerateMipmap(true);
            texture->Build();
        }

        auto const normalTexture = material.normalTexture.index;
        if (normalTexture >= 0)
        {
            pbrMaterial.Normal = model.images[model.textures[normalTexture].source].uri;
        }

        // Blue channel : metalness
        // Green channel : roughness
        auto const metallicRoughnessTexture = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
        if (metallicRoughnessTexture >= 0)
        {
            pbrMaterial.MetallicRoughness = model.images[model.textures[metallicRoughnessTexture].source].uri;
        }

        auto const metallicFactor = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);
        pbrMaterial.MetallicFactor = metallicFactor;

        auto const roughnessFactor = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);
        pbrMaterial.RoughnessFactor = roughnessFactor;

        // fmt::print("{}\n", pbrMaterial);

        pbrMaterials.push_back(pbrMaterial.Name);
        opfor::Application::Get().AddPbrMaterial(pbrMaterial);

        currentMaterialIndex++;
    }

    // Load texture images
    // -------------------

    for (auto const &t : model.textures)
    {

        auto const &image = model.images[t.source];

        String name = image.uri;
        String path = directory + image.uri;

        opfor::TextureParameterList texParams{};

        if (t.sampler >= 0)
        {
            auto const &sampler = model.samplers[t.sampler];

            texParams = {
                {opfor::TextureParameterType::MignifyFilter, (opfor::TextureParameterValue)sampler.minFilter},
                {opfor::TextureParameterType::MagnifyFilter, (opfor::TextureParameterValue)sampler.magFilter},
                {opfor::TextureParameterType::WrapS, (opfor::TextureParameterValue)sampler.wrapS},
                {opfor::TextureParameterType::WrapT, (opfor::TextureParameterValue)sampler.wrapT},
            };
        }
        else
        {
            texParams = {
                {opfor::TextureParameterType::MignifyFilter, opfor::TextureParameterValue::Linear},
                {opfor::TextureParameterType::MagnifyFilter, opfor::TextureParameterValue::Linear},
                {opfor::TextureParameterType::WrapS, opfor::TextureParameterValue::Repeat},
                {opfor::TextureParameterType::WrapT, opfor::TextureParameterValue::Repeat},
            };
        }

        auto img = opfor::ImageLoader::Load(path);

        auto texture = TextureManager::Get().Create2D(name);

        if (img.nchannel == 3)
        {
            texture->SetInputFormat(opfor::DataFormat::RGB);
            texture->SetOutputFormat(opfor::DataFormat::RGB);
        }
        else if (img.nchannel == 4)
        {
            texture->SetInputFormat(opfor::DataFormat::RGBA);
            texture->SetOutputFormat(opfor::DataFormat::RGBA);
            texture->SetHasAlpha(true);
        }
        else
        {
            OP4_CORE_WARNING("img.nchannel = {} -- Not handled!", img.nchannel);
        }
        texture->SetDataType(opfor::DataType::UnsignedByte);
        texture->SetData(img.data.get());
        texture->SetSize(img.width, img.height);
        texture->SetTextureParameters(texParams);
        texture->SetGenerateMipmap(true);
        texture->Build();
    }

    Vector<unsigned int> meshes;

    for (auto const &scene : model.scenes)
    {
        for (auto const &node : scene.nodes)
        {
            auto const newMeshes = TinyProcessNode(model.nodes[node], model, pbrMaterials);
            if (newMeshes.has_value())
            {
                meshes.insert(meshes.end(), newMeshes.value().begin(), newMeshes.value().end());
            }
        }
    }

    if (meshes.size() > 0)
    {
        meshes_ret = meshes;
    }

    const auto parse_animation = [&model](tinygltf::Animation const &animation) {
        AnimationComponent ret{};

        
        Optional<unsigned> target;
        OP4_CORE_DEBUG("tg");
        for (auto const &channel : animation.channels)
        {
          OP4_CORE_DEBUG("channel: {}", channel.target_path);
            if (!target.has_value())
            {
                target = channel.target_node;
            }
            auto const &sampler = animation.samplers[channel.sampler];
            OP4_CORE_ASSERT(sampler.interpolation == "LINEAR", "Unsupported interpolation");

            {
                auto const &accessor = model.accessors[sampler.input];
                auto const &buffer_view = model.bufferViews[accessor.bufferView];
                auto const &buffer = model.buffers[buffer_view.buffer];
                auto const byte_stride = accessor.ByteStride(buffer_view);
                auto const data = buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;
                OP4_CORE_ASSERT(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "Corrupted gltf");
                OP4_CORE_DEBUG("yep: {} {}", accessor.minValues[0], accessor.maxValues[0]);
                for (auto i = 0u; i < accessor.count; ++i)
                {
                    OP4_CORE_DEBUG("input of {}: {}", buffer_view.name,
                                   *reinterpret_cast<const float *>(data + i * byte_stride));
                }
            }

            {
                auto const &accessor = model.accessors[sampler.output];
                auto const &buffer_view = model.bufferViews[accessor.bufferView];
                auto const &buffer = model.buffers[buffer_view.buffer];
                // OP4_CORE_ASSERT(accessor.type == TINYGLTF_TYPE_VEC3, "Only supports vec3");
                OP4_CORE_ASSERT(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "Corrupted gltf");
                auto const byte_stride = accessor.ByteStride(buffer_view);
                auto const data = buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;
                switch (accessor.type)
                {
                case TINYGLTF_TYPE_VEC3:
                    for (auto i = 0u; i < accessor.count; ++i)
                    {
                        auto const &buffer_view = model.bufferViews[accessor.bufferView];
                        auto const &buffer = model.buffers[buffer_view.buffer];
                        OP4_CORE_DEBUG(
                            "output of {}: vec3({}, {}, {})", buffer.name,
                            *reinterpret_cast<const float *>(buffer.data.data() + 0 * sizeof(float) + i * byte_stride),
                            *reinterpret_cast<const float *>(buffer.data.data() + 1 * sizeof(float) + i * byte_stride),
                            *reinterpret_cast<const float *>(buffer.data.data() + 2 * sizeof(float) + i * byte_stride));
                    }
                    break;
                case TINYGLTF_TYPE_VEC4:
                    for (auto i = 0u; i < accessor.count; ++i)
                    {
                        auto const &buffer_view = model.bufferViews[accessor.bufferView];
                        auto const &buffer = model.buffers[buffer_view.buffer];
                        OP4_CORE_DEBUG(
                            "output of {}: vec4({}, {}, {}, {})", buffer.name,
                            *reinterpret_cast<const float *>(buffer.data.data() + 0 * sizeof(float) + i * byte_stride),
                            *reinterpret_cast<const float *>(buffer.data.data() + 1 * sizeof(float) + i * byte_stride),
                            *reinterpret_cast<const float *>(buffer.data.data() + 2 * sizeof(float) + i * byte_stride),
                            *reinterpret_cast<const float *>(buffer.data.data() + 3 * sizeof(float) + i * byte_stride));
                    }
                    break;
                default:
                    OP4_CORE_UNREACHABLE();
                }
            }
        }
        return ret;
    };

    Vector<AnimationComponent> animation_components;
    std::transform(std::begin(model.animations), std::end(model.animations), std::back_inserter(animation_components),
                   parse_animation);

    OP4_CORE_INFO("Model \"{:s}\" successfully parsed.", path);

    return meshes_ret;
}
//
//template <typename T, size_t N>
//static Array<T, N> gltf_access_data(tinygltf::Accessor const & accessor, tinygltf::Model const & model)
//{
//  Array<T, N> ret;
//    auto const &buffer_view = model.bufferViews[accessor.bufferView];
//    auto const &buffer = model.buffers[buffer_view.buffer];
//    auto const byte_stride = accessor.ByteStride(buffer_view);
//    auto const data = buffer.data.data() + buffer_view.byteOffset + accessor.byteOffset;
//    for (auto i = 0u; i < accessor.count; ++i)
//    {
//
//      for ()
//    }
//}

} // namespace opfor