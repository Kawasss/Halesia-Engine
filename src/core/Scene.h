#pragma once
#include <map>
#include <string>
#include <vector>
#include <unordered_map>
#include <future>

#include "Console.h"
#include "io/SceneLoader.h"

class Object;
class Camera;
class Win32Window;

typedef uint64_t Handle;

enum ObjectImportType
{
	/// <summary>
	/// The object is from an external source, i.e. an .obj file.
	/// </summary>
	OBJECT_IMPORT_EXTERNAL,

	/// <summary>
	/// The object is from inside the scene's CRS file.
	/// </summary>
	OBJECT_IMPORT_INTERNAL
};

class Scene 
{
public:
	static Camera* defaultCamera;

	Camera* camera = defaultCamera;

	/// <summary>
	/// Creates an object with a given class as attached script. Loading an external file will not happen async, so load times will be longer
	/// </summary>
	/// <typeparam name="T"></typeparam>
	/// <param name="name">: The name of the object to find and create. If objectType is OBJECT_IMPORT_EXTERNAL this needs to be the path to the file</param>
	/// <param name="objectType">: The type of object: either from inside the scene file or an outside file</param>
	/// <returns>A pointer to the base object of the custom object. A nullptr will be returned if an object matching the name can't be found</returns>
	template<typename T> 
	Object* AddCustomObject(std::string name, ObjectImportType objectType = OBJECT_IMPORT_INTERNAL);
	template<typename T> 
	Object* AddCustomObject(const ObjectCreationData& creationData);
	Object* AddStaticObject(const ObjectCreationData& creationData);
	
	template<typename T> 
	Object* DuplicateCustomObject(Object* objPtr, std::string name);
	Object* DuplicateStaticObject(Object* objPtr, std::string name);

	template<typename T>
	Camera* AddCustomCamera();

	Object* GetObjectByHandle(Handle handle);
	bool HasFinishedLoading();
	bool IsObjectHandleValid(Handle handle);
	void LoadScene(std::string path);
	void LoadUninitializedObjects();
	void UpdateCamera(Win32Window* window, float delta);
	void UpdateScripts(float delta);
	void CollectGarbage();

	/// <summary>
	/// Transfers the ownership of the child from the scene to the object. The scene can no longer access the child after the transfer,
	/// the object will become the sole owner. The scene will no longer be responsible for deletion.
	/// </summary>
	void TransferObjectOwnership(Object* newOwner, Object* child);

	virtual void Start() {};
	virtual void Update(float delta) {};
			void Destroy();

	std::vector<Object*> allObjects; // this vector owns the objects

private:
	void LoadFileIntoScene(std::string path);
	void RegisterObjectPointer(Object* objPtr, bool isCustom);
	bool GetInternalObjectCreationData(std::string name, ObjectCreationData& creationData);

	std::future<void> loadingProcess;
	std::vector<Object*> objectsWithScripts; // these two vectors simply view the objects, "allObjects" owns the objects
	std::vector<Object*> staticObjects;
	std::unordered_map<Handle, Object*> objectHandles;
	std::vector<ObjectCreationData> objectCreationDatas;

	bool sceneIsLoading = false;

protected:
	/// <summary>
	/// Finds an object by name and returns the pointer to it
	/// </summary>
	/// <param name="name"></param>
	/// <returns>Will return a nullptr if the name doesn't match</returns>
	Object* GetObjectByName(std::string name);
	void Free(Object* object);
};

template<typename T> Camera* Scene::AddCustomCamera()
{
	T* custom = new T();
	Camera* ret = custom;
	ret->SetScript(custom);
	ret->Start();
	return ret;
}

template<typename T> Object* Scene::AddCustomObject(const ObjectCreationData& creationData)
{
	T* customPointer = new T();
	Object* objPtr = customPointer;
	RegisterObjectPointer(objPtr, true);
	objPtr->Initialize(creationData, customPointer);

	return objPtr;
}

template<typename T> Object* Scene::AddCustomObject(std::string name, ObjectImportType objectType) // templates must be declared in header
{
	ObjectCreationData creationData;
	if (objectType == OBJECT_IMPORT_EXTERNAL)
	{
		creationData = GenericLoader::LoadObjectFile(name);
	}
	else // if (objectType == OBJECT_IMPORT_INTERNAL)
	{
		if (!GetInternalObjectCreationData(name, creationData))
		{
			Console::WriteLine("Failed to find the creation data for the given name \"" + name + '"', MESSAGE_SEVERITY_ERROR);
			return nullptr;
		}
	}

	T* customPointer = new T();
	Object* objPtr = customPointer;
	RegisterObjectPointer(objPtr, true);
	objPtr->Initialize(creationData, customPointer);
	
	return objPtr;
}

template<typename T> Object* Scene::DuplicateCustomObject(Object* objPtr, std::string name)
{
	T* tPtr = new T();
	Object* newObjPtr = tPtr;
	Object::Duplicate(objPtr, newObjPtr, name, tPtr);
	RegisterObjectPointer(newObjPtr, true);
	newObjPtr->Start();
	
	return newObjPtr;
}