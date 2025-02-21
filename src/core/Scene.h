#pragma once
#include <string>
#include <vector>

class Object;
class Camera;
class Window;

struct ObjectCreationData;

class Scene 
{
public:
	static Camera* defaultCamera;

	Camera* camera = defaultCamera;

	template<typename T> 
	Object* AddObject(const ObjectCreationData& creationData);
	Object* AddObject(const ObjectCreationData& creationData);
	
	Object* AddObjectAsChild(Object* parent, const ObjectCreationData& creationData);

	template<typename T> 
	Object* DuplicateObject(Object* objPtr, std::string name);
	Object* DuplicateObject(Object* objPtr, std::string name);

	template<typename T>
	Camera* AddCustomCamera();

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

	bool sceneIsLoading = false;

protected:
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