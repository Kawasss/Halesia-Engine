#pragma once
#include <string>
#include <vector>
#include <unordered_map>

class Object;
class Camera;
class Window;

struct ObjectCreationData;

using Handle = uint64_t;

class Scene 
{
public:
	static Camera* defaultCamera;

	Camera* camera = defaultCamera;

	template<typename T> 
	Object* AddObject(const ObjectCreationData& creationData);
	Object* AddObject(const ObjectCreationData& creationData);
	
	template<typename T> 
	Object* DuplicateObject(Object* objPtr, std::string name);
	Object* DuplicateObject(Object* objPtr, std::string name);

	template<typename T>
	Camera* AddCustomCamera();

	Object* GetObjectByHandle(Handle handle);
	bool IsObjectHandleValid(Handle handle);
	bool HasFinishedLoading();

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
	virtual void UpdateGUI(float delta) {};

	virtual void MainThreadUpdate(float delta) {}; // this is only called from the main thread and after the renderer completes a frames in flight cycle

	~Scene() { Destroy(); }
	void Destroy();

	std::vector<Object*> allObjects; // this vector owns the objects

private:
	void RegisterObjectPointer(Object* objPtr);

	std::unordered_map<Handle, Object*> objectHandles;

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

template<typename T> Object* Scene::AddObject(const ObjectCreationData& creationData)
{
	T* customPointer = new T();
	Object* objPtr = customPointer;
	RegisterObjectPointer(objPtr);
	objPtr->Initialize(creationData, customPointer);

	return objPtr;
}

template<typename T> Object* Scene::DuplicateObject(Object* objPtr, std::string name)
{
	T* tPtr = new T();
	Object* newObjPtr = tPtr;
	Object::Duplicate(objPtr, newObjPtr, name, tPtr);
	RegisterObjectPointer(newObjPtr);
	newObjPtr->Start();
	
	return newObjPtr;
}