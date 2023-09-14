#pragma once
#include <string>
#include <assimp/scene.h>
#include <vector>
#include <future>
#include "renderer/Buffers.h"
#include "renderer/PhysicalDevice.h"
#include "Vertex.h"
#include "renderer/PhysicalDevice.h"
#include "Transform.h"
#include "renderer/Texture.h"
#include "CreationObjects.h"
#include "SceneLoader.h"

enum ObjectState
{
	STATUS_VISIBLE,   // visible runs the script and renders the object
	STATUS_INVISIBLE, // invisible runs the script, but doesn't render the object
	STATUS_DISABLED   // disabled doesn't run the script and doesn't render the object
};

struct Material
{
	// dont know if dynamically allocated is necessary since the material will always be used for the lifetime of the mesh, the class is sort of big so not so sure if copying is cheap
	Texture* albedo = Texture::placeholderAlbedo;
	Texture* normal = Texture::placeholderNormal;
	Texture* metallic = Texture::placeholderMetallic;
	Texture* roughness = Texture::placeholderRoughness;
	Texture* ambientOcclusion = Texture::placeholderAmbientOcclusion;

	Texture* At(int i)
	{
		switch (i) 
		{
		case 0:
			return albedo;
		case 1:
			return normal;
		case 2:
			return metallic;
		case 3:
			return roughness;
		case 4:
			return ambientOcclusion;
		default:
			return albedo;
		}
	}

	void Destroy() // only delete the textures if they arent the placeholders
	{
		if (albedo != Texture::placeholderAlbedo) albedo->Destroy();
		if (normal != Texture::placeholderNormal) normal->Destroy();
		if (metallic != Texture::placeholderMetallic) metallic->Destroy();
		if (roughness != Texture::placeholderRoughness) roughness->Destroy();
		if (ambientOcclusion != Texture::placeholderAmbientOcclusion) ambientOcclusion->Destroy();
	}
};

struct Mesh
{
	Mesh(VkDevice logicalDevice, PhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue, aiMesh* mesh, aiMaterial* material);
	Mesh(MeshCreationData creationData, VkDevice logicalDevice, PhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue);
	void Destroy();

	Material material{};
	VertexBuffer vertexBuffer;
	IndexBuffer indexBuffer;

	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;

	glm::vec3 min, max, center, extents;

	void ProcessMaterial(aiMaterial* material, VkDevice logicalDevice, VkQueue queue, VkCommandPool commandPool, PhysicalDevice physicalDevice);
	void ProcessIndices(aiMesh* mesh);
	void ProcessVertices(aiMesh* mesh);

	void Recreate(VkDevice logicalDevice, PhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue);
};

class Object
{
public:
	Object() = default;
	Object(std::string path, const MeshCreationObjects& creationObjects); // maybe seperate the MeshCreationObjects into a CreateMeshes function to allow the meshes to be loaded in later
	Object(const ObjectCreationData& creationData, const MeshCreationObjects& creationObjects);

	virtual ~Object() {};
	virtual void Destroy();
	virtual void Start() {};
	virtual void Update(float delta) {};

	bool HasFinishedLoading();
	void AwaitGeneration();
	void RecreateMeshes(const MeshCreationObjects& creationObjects);

	void* scriptClass = nullptr;
	template<typename T> T GetScript() { return static_cast<T>(scriptClass); };

	Transform transform;
	std::vector<Mesh> meshes;
	ObjectState state = STATUS_VISIBLE;
	std::string name;
	UUID uuid{};
	bool finishedLoading = false;
	bool shouldBeDestroyed = false;

private:
	std::future<void> generationProcess;
	
protected:
	/// <summary>
	/// This function is used for custom classes. This function must be called before any other actions are done.
	/// It creates the meshes and connects the custom class to the base class.
	/// </summary>
	/// <param name="customClassInstancePointer">: A pointer to the custom class, "this" will work most of the time</param>
	/// <param name="path"></param>
	/// <param name="creationObjects">: The objects needed to create the meshes</param>
	void CreateObject(void* customClassInstancePointer, const ObjectCreationData& creationData, const MeshCreationObjects& creationObjects);

	/// <summary>
	/// The async version of CreateObject. This wont pause the program while its loading, so async loaded objects must be checked with HasFinishedLoading before calling a function.
	/// Be weary of accessing members in the constructor, since they don't have to be loaded in. AwaitGeneration awaits the async thread.
	/// </summary>
	/// <param name="customClassInstancePointer">: A pointer to the custom class, "this" will work most of the time</param>
	/// <param name="path"></param>
	/// <param name="creationObjects">: The objects needed to create the meshes</param>
	void CreateObjectAsync(void* customClassInstancePointer, const ObjectCreationData& creationData, const MeshCreationObjects& creationObjects);

	static void Free(Object* objPtr)
	{
		objPtr->shouldBeDestroyed = true;
	}
};