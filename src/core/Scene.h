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
class Window;

typedef uint64_t Handle;

class Scene 
{
public:
	static Camera* defaultCamera;

	Camera* camera = defaultCamera;

	template<typename T> 
	Object* AddCustomObject(const ObjectCreationData& creationData);
	Object* AddStaticObject(const ObjectCreationData& creationData);
	
	template<typename T> 
	Object* DuplicateCustomObject(Object* objPtr, std::string name);
	Object* DuplicateStaticObject(Object* objPtr, std::string name);

	template<typename T>
	Camera* AddCustomCamera();

	Object* GetObjectByHandle(Handle handle);
	bool IsObjectHandleValid(Handle handle);
	bool HasFinishedLoading();
	
	void LoadScene(std::string path);
	void LoadUninitializedObjects();

	void UpdateCamera(Window* window, float delta);
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

template<typename T> Object* Scene::DuplicateCustomObject(Object* objPtr, std::string name)
{
	T* tPtr = new T();
	Object* newObjPtr = tPtr;
	Object::Duplicate(objPtr, newObjPtr, name, tPtr);
	RegisterObjectPointer(newObjPtr, true);
	newObjPtr->Start();
	
	return newObjPtr;
}