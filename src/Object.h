#pragma once
#include <string>
#include <vector>
#include <future>
#include <mutex>

#include "renderer/Mesh.h"
#include "Transform.h"
#include "CreationObjects.h"

struct ObjectCreationData;
typedef uint64_t Handle;

enum ObjectState
{
	/// <summary>
	/// The attached script is run and the object is rendered
	/// </summary>
	OBJECT_STATE_VISIBLE,
	/// <summary>
	/// The attached script is run, but the object isn't rendered
	/// </summary>
	OBJECT_STATE_INVISIBLE,
	/// <summary>
	/// The attached script isn't run and the object isn't rendered
	/// </summary>
	OBJECT_STATE_DISABLED
};

class Object
{
public:
	Object() = default;
	Object(const ObjectCreationData& creationData, const ObjectCreationObject& creationObjects);

	virtual		~Object() {};
	virtual void Destroy();
	virtual void Start() {};
	virtual void Update(float delta) {};

	bool HasFinishedLoading();

	/// <summary>
	/// Awaits the async generation process of the object and meshes
	/// </summary>
	void AwaitGeneration();

	void RecreateMeshes(const MeshCreationObject& creationObject);

	/// <summary>
	/// Gets the script attached to the object, if no script is attached it will return an invalid pointer
	/// </summary>
	/// <typeparam name="T">: The name of the script's class as a pointer</typeparam>
	/// <returns>Pointer to the given class</returns>
	template<typename T> T GetScript() { if (scriptClass == nullptr) throw std::runtime_error("Failed to get a script class: the pointer is a nullptr"); return static_cast<T>(scriptClass); };

	/// <summary>
	/// This wont pause the program while its loading, so async loaded objects must be checked with HasFinishedLoading before calling a function.
	/// Be weary of accessing members in the constructor, since they don't have to be loaded in. AwaitGeneration awaits the async thread.
	/// </summary>
	/// <param name="customClassInstancePointer">: A pointer to the custom class, "this" will work most of the time</param>
	/// <param name="creationData"></param>
	/// <param name="creationObject">: The objects needed to create the meshes</param>
	void CreateObject(void* customClassInstancePointer, const ObjectCreationData& creationData, const ObjectCreationObject& creationObject);

	template<typename T> static Object* Duplicate(Object* objPtr, std::string name)
	{
		T* customPtr = new T();
		Object* newObjPtr = customPtr;
		newObjPtr->meshes = objPtr->meshes;
		newObjPtr->transform = objPtr->transform;
		newObjPtr->name = name;
		newObjPtr->finishedLoading = true;
		newObjPtr->scriptClass = customPtr;
		newObjPtr->handle = ResourceManager::GenerateHandle();

		Console::WriteLine("Duplicated object \"" + name + "\" from object \"" + objPtr->name + "\"", MESSAGE_SEVERITY_DEBUG);

		return newObjPtr;
	}

	Transform transform;
	std::vector<Mesh> meshes;
	ObjectState state = OBJECT_STATE_VISIBLE;
	std::string name;
	std::mutex mutex;
	Handle handle;
	bool finishedLoading = false;
	bool shouldBeDestroyed = false;

private:
	void GenerateObjectWithData(const ObjectCreationObject& creationObject, const ObjectCreationData& creationData);

	void* scriptClass = nullptr;
	std::future<void> generationProcess;
	
protected:
	static void Free(Object* objPtr)
	{
		objPtr->shouldBeDestroyed = true;
	}
};