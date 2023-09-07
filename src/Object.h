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
#include "CreationObjects.h"

enum ObjectState
{
	STATUS_VISIBLE,   // visible runs the script and renders the object
	STATUS_INVISIBLE, // invisible runs the script, but doesn't render the object
	STATUS_DISABLED   // disabled doesn't run the script and doesn't render the object
};

struct Mesh
{
	Mesh(VkDevice logicalDevice, PhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue, aiMesh* mesh);
	void Destroy();

	VertexBuffer vertexBuffer;
	IndexBuffer indexBuffer;

	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;

	glm::vec3 min, max, center, extents;

	void ProcessIndices(aiMesh* mesh);
	void ProcessVertices(aiMesh* mesh);
};

class Object
{
public:
	Object() = default;
	Object(std::string path, const MeshCreationObjects& creationObjects);

	virtual ~Object() {};
	virtual void Destroy();
	virtual void Start() {};
	virtual void Update(float delta) {};

	bool HasFinishedLoading();
	void AwaitGeneration();

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
	void CreateObject(void* customClassInstancePointer, std::string path, const MeshCreationObjects& creationObjects);

	/// <summary>
	/// The async version of CreateObject. This wont pause the program while its loading, so async loaded objects must be checked with HasFinishedLoading before calling a function.
	/// Be weary of accessing members in the constructor, since they don't have to be loaded in. AwaitGeneration awaits the async thread.
	/// </summary>
	/// <param name="customClassInstancePointer">: A pointer to the custom class, "this" will work most of the time</param>
	/// <param name="path"></param>
	/// <param name="creationObjects">: The objects needed to create the meshes</param>
	void CreateObjectAsync(void* customClassInstancePointer, std::string path, const MeshCreationObjects& creationObjects);

	static void Free(Object* objPtr)
	{
		objPtr->shouldBeDestroyed = true;
	}
};