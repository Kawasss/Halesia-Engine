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
#include "Material.h"
#include "CreationObjects.h"
#include "SceneLoader.h"
#include "ResourceManager.h"

enum ObjectState
{
	/// <summary>
	/// The attached script is run and the object is rendered
	/// </summary>
	STATUS_VISIBLE,
	/// <summary>
	/// The attached script is run, but the object isn't rendered
	/// </summary>
	STATUS_INVISIBLE,
	/// <summary>
	/// The attached script isn't run and the object isn't rendered
	/// </summary>
	STATUS_DISABLED
};

struct Mesh
{
	Mesh(const MeshCreationObject& creationObject, const MeshCreationData& creationData);
	void Destroy();

	Material material{};
	VertexBuffer vertexBuffer;
	IndexBuffer indexBuffer;

	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;

	glm::vec3 min, max, center, extents;

	void ProcessMaterial(const TextureCreationObject& creationObjects, const MaterialCreationData& creationData);
	void Recreate(const MeshCreationObject& creationObject);
};

class Object
{
public:
	Object() = default;
	Object(const ObjectCreationData& creationData, const ObjectCreationObject& creationObjects);
	Object(std::string path, const ObjectCreationObject& creationObject);

	virtual ~Object() {};
	virtual void Destroy();
	virtual void Start() {};
	virtual void Update(float delta) {};

	bool HasFinishedLoading();

	/// <summary>
	/// Awaits the async generation process of the object and meshes
	/// </summary>
	void AwaitGeneration();

	void RecreateMeshes(const MeshCreationObject& creationObject);

	void* scriptClass = nullptr;

	/// <summary>
	/// Gets the script attached to the object, if no script is attached it will return an invalid pointer
	/// </summary>
	/// <typeparam name="T">: The name of the script's class</typeparam>
	/// <returns>Pointer to the given class</returns>
	template<typename T> T GetScript() { return static_cast<T>(scriptClass); };

	Transform transform;
	std::vector<Mesh> meshes;
	ObjectState state = STATUS_VISIBLE;
	std::string name;
	Handle hObject{};
	bool finishedLoading = false;
	bool shouldBeDestroyed = false;

private:
	std::future<void> generationProcess;
	
protected:
	/// <summary>
	/// The async version of CreateObject. This wont pause the program while its loading, so async loaded objects must be checked with HasFinishedLoading before calling a function.
	/// Be weary of accessing members in the constructor, since they don't have to be loaded in. AwaitGeneration awaits the async thread.
	/// </summary>
	/// <param name="customClassInstancePointer">: A pointer to the custom class, "this" will work most of the time</param>
	/// <param name="creationData"></param>
	/// <param name="creationObject">: The objects needed to create the meshes</param>
	void CreateObject(void* customClassInstancePointer, const ObjectCreationData& creationData, const ObjectCreationObject& creationObject);

	static void Free(Object* objPtr)
	{
		objPtr->shouldBeDestroyed = true;
	}
};