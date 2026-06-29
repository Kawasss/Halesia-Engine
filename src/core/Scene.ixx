module;

#include <Windows.h>

#include "../system/CriticalSection.h"

export module Core.Scene;

import std;

import System.Window;

import Core.CameraObject;
import Core.Object;

import IO.CreationData;

export class Scene
{
public:
	static CameraObject* defaultCamera;

	CameraObject* camera = defaultCamera;

	Object* AddObject(const ObjectCreationData& creationData, Object* pParent = nullptr);

	void SetActiveCamera(CameraObject* pCamera);

	bool HasFinishedLoading();

	void UpdateCamera(Window* pWindow, float delta);
	void UpdateScripts(float delta);
	void CollectGarbage();

	void PrepareObjectsForUpdate();

	void DestroyAllObjects();

	bool NameExists(const std::string_view& str, Object* pOwner);

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

	void EnsureValidName(std::string& name, Object* pObject);

	std::vector<Object*> flatObjects;
	win32::CriticalSection objectCriticalSection;
};