module;

#include "io/SceneLoader.h"
#include "io/CreationData.h"

#include "core/Object.h"
#include "core/Console.h"
#include "core/MeshObject.h"
#include "core/ScriptObject.h"

#include "system/CriticalSection.h"

module Core.Scene;

import Core.Rigid3DObject;
import Core.LightObject;
import Core.CameraObject;

import System.Window;

CameraObject* Scene::defaultCamera = nullptr;

void Scene::SetActiveCamera(CameraObject* pCamera)
{
	camera = pCamera;
}

bool Scene::HasFinishedLoading()
{
	return true;
}

bool Scene::NameExists(const std::string_view& str, Object* pOwner)
{
	win32::CriticalLockGuard guard(objectCriticalSection);
	return std::find_if(flatObjects.begin(), flatObjects.end(), [&](const Object* pObj) { return pObj->name == str && pObj != pOwner; }) != flatObjects.end();
}

void Scene::EnsureValidName(std::string& name, Object* pObject)
{
	std::string newName = name;

	for (int attemptCount = 0; NameExists(newName, pObject); attemptCount++)
	{
		Console::WriteLine("invalid name \"{}\": it already exists", Console::Severity::Error, newName);
		newName = name + std::to_string(attemptCount);
	}

	name = newName;
}

void Scene::RegisterObjectPointer(Object* pObject, Object* pParent)
{
	win32::CriticalLockGuard guard(objectCriticalSection);

	if (pParent == nullptr) // only accept ownership of the object of noone else has ownership
		allObjects.push_back(pObject);
	else
		pParent->AddChild(pObject);

	flatObjects.push_back(pObject);

	pObject->SetParentScene(this);
}

static void EraseMemberFromVector(std::vector<Object*>& vector, Object* memberToErase)
{
	 auto it = std::find(vector.begin(), vector.end(), memberToErase);
	 if (it != vector.end())
		 vector.erase(it);
}

 Object* Scene::AddObject(const ObjectCreationData& creationData, Object* pParent)
 {
	 Object* pObject = nullptr;

	 switch (creationData.type)
	 {
	 case ObjectCreationData::Type::Base:
		 pObject = Object::Create(creationData);
		 break;
	 case ObjectCreationData::Type::Mesh:
		 pObject = MeshObject::Create(creationData);
		 break;
	 case ObjectCreationData::Type::Rigid3D:
		 pObject = Rigid3DObject::Create(creationData);
		 break;
	 case ObjectCreationData::Type::Light:
		 pObject = LightObject::Create(creationData);
		 break;
	 case ObjectCreationData::Type::Script:
		 pObject = ScriptObject::Create(creationData);
		 break;
	 }
	 assert(pObject != nullptr);

	 EnsureValidName(pObject->name, pObject);

	 RegisterObjectPointer(pObject, pParent);

	 for (const ObjectCreationData& child : creationData.children)
		 AddObject(child, pObject);

	 pObject->Start();

	 return pObject;
 }

 Object* Scene::DuplicateObject(Object* pObject, std::string name)
 {
	 Object* pCopy = pObject->CreateShallowCopy();
	 pCopy->name = name;

	 RegisterObjectPointer(pCopy, pObject->GetParent());
	 return pCopy;
 }

void Scene::Free(Object* pObject)
{
	Object::Free(pObject);
}

void Scene::UpdateCamera(Window* pWindow, float delta)
{
	camera->transform.CalculateModelMatrix();
	camera->UpdateMatrices();
	camera->FullUpdate(delta);
}

void Scene::PrepareObjectsForUpdate()
{
	if (!HasFinishedLoading())
		return;

	for (Object* pObject : allObjects)
	{
		if (pObject->ShouldBeDestroyed() || pObject->state == OBJECT_STATE_DISABLED)
			continue;
		pObject->FullUpdateTransform();
	}
}

void Scene::UpdateScripts(float delta)
{
	if (!HasFinishedLoading())
		return;

	for (int i = 0; i < allObjects.size(); i++)
	{
		if (allObjects[i]->ShouldBeDestroyed() || allObjects[i]->state == OBJECT_STATE_DISABLED)
			continue;
		allObjects[i]->FullUpdate(delta); // can optimize away empty function calls here
	}
}

void Scene::CollectGarbage()
{
	CollectGarbageRecursive(allObjects);
}

void Scene::CollectGarbageRecursive(std::vector<Object*>& base)
{
	win32::CriticalLockGuard guard(objectCriticalSection);
	for (int i = 0; i < base.size(); i++)
	{
		Object* obj = base[i];
		CollectGarbageRecursive(obj->GetChildren());
		if (!obj->ShouldBeDestroyed())
			continue;

		base.erase(base.begin() + i);
		auto it = std::find(flatObjects.begin(), flatObjects.end(), obj);
		if (it == flatObjects.end())
			Console::WriteLine("failed to delete objects as a flat object", Console::Severity::Error);
		else
			flatObjects.erase(it);

		delete obj;
		i--;
	}
}

void Scene::TransferObjectOwnership(Object* pNewOwner, Object* pChild)
{
	std::vector<Object*>::iterator iter = std::find(allObjects.begin(), allObjects.end(), pChild);
	if (iter != allObjects.end())
		allObjects.erase(iter);
	pNewOwner->AddChild(pChild);
}

void Scene::DestroyAllObjects()
{
	for (Object* object : allObjects)
		Object::Free(object);
	CollectGarbage();
}

Scene::~Scene()
{
	Destroy();
	DestroyAllObjects();
}