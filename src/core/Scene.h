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
	Object* AddObject(const ObjectCreationData& creationData, Object* pParent = nullptr);
	Object* AddObject(const ObjectCreationData& creationData, Object* pParent = nullptr);

	template<typename T> 
	Object* DuplicateObject(Object* pObject, std::string name); //!< UNSAFE, currently does not duplicate the appropriate class based on type
	Object* DuplicateObject(Object* pObject, std::string name); //!< UNSAFE

	template<typename T>
	Camera* AddCustomCamera();

	bool HasFinishedLoading();

	void UpdateCamera(Window* pWindow, float delta);
	void UpdateScripts(float delta);
	void CollectGarbage();

	void DestroyAllObjects();

	/// <summary>
	/// Transfers the ownership of the child from the scene to the object. The scene can no longer access the child after the transfer,
	/// the object will become the sole owner. The scene will no longer be responsible for deletion.
	/// </summary>
	void TransferObjectOwnership(Object* pNewOwner, Object* pChild);

	virtual void Start() {};
	virtual void Update(float delta) {};
	virtual void UpdateGUI(float delta) {};

	virtual void MainThreadUpdate(float delta) {}; // this is only called from the main thread and after the renderer completes a frames in flight cycle

	virtual void Destroy() {}

	// order of destruction:
	// 1. call Destroy()
	// 2. destroy all objects
	~Scene();
	
	std::vector<Object*> allObjects; // this vector owns the objects

private:
	void CollectGarbageRecursive(std::vector<Object*>& base);

	void RegisterObjectPointer(Object* pObject, Object* pParent);

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

template<typename T> Object* Scene::AddObject(const ObjectCreationData& creationData, Object* pParent)
{
	T* customPointer = new T();
	Object* objPtr = customPointer;
	RegisterObjectPointer(objPtr, pParent);
	objPtr->Initialize(creationData, customPointer);

	return objPtr;
}

template<typename T> Object* Scene::DuplicateObject(Object* pObject, std::string name)
{
	T* tPtr = new T();
	Object* newObjPtr = tPtr;
	Object::Duplicate(pObject, newObjPtr, name, tPtr);
	RegisterObjectPointer(newObjPtr);
	newObjPtr->Start();
	
	return newObjPtr;
}