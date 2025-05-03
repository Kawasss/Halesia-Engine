#include <algorithm>
#include <execution>
#include <optional>

#include "io/SceneLoader.h"
#include "system/Window.h"

#include "core/Camera.h"
#include "core/Object.h"
#include "core/Scene.h"
#include "core/Console.h"
#include "core/MeshObject.h"

Camera* Scene::defaultCamera = new Camera();

bool Scene::HasFinishedLoading()
{
	return true;
}

void Scene::RegisterObjectPointer(Object* pObject, Object* pParent)
{
	if (pParent == nullptr) // only accept ownership of the object of noone else has ownership
		allObjects.push_back(pObject);
	else
		pParent->AddChild(pObject);

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
	 }
	 assert(pObject != nullptr);

	 RegisterObjectPointer(pObject, pParent);

	 for (const ObjectCreationData& child : creationData.children)
		 AddObject(child, pObject);

	 return pObject;
 }

 Object* Scene::DuplicateObject(Object* pObject, std::string name) // unsafe !!
 {
	 Object* newPtr = new Object(pObject->GetType());
	 Object::Duplicate(pObject, newPtr, name, nullptr);
	 RegisterObjectPointer(newPtr, pObject->GetParent());

	 return newPtr;
 }

void Scene::Free(Object* pObject)
{
	auto it = std::find(allObjects.begin(), allObjects.end(), pObject);

	if (it == allObjects.end())
	{
		Console::WriteLine("Failed to free an object, because it isn't registered in the scene. The given object has not been freed.", Console::Severity::Error);
		return;
	}

	allObjects.erase(it);
	delete pObject;
}

void Scene::UpdateCamera(Window* pWindow, float delta)
{
	camera->Update(pWindow, delta);
	camera->UpdateVelocityMatrices();
}

void Scene::UpdateScripts(float delta)
{
	if (!HasFinishedLoading())
		return;

	for (int i = 0; i < allObjects.size(); i++)
	{
		if (!allObjects[i]->HasScript() || allObjects[i]->ShouldBeDestroyed() || allObjects[i]->state == OBJECT_STATE_DISABLED)
			continue;
		allObjects[i]->Update(delta);
	}
}

void Scene::CollectGarbage()
{
	for (auto iter = allObjects.begin(); iter < allObjects.end(); iter++)
	{
		Object* obj = *iter;
		if (!obj->ShouldBeDestroyed())
			continue;

		allObjects.erase(iter);
		delete obj;
		iter = allObjects.begin();
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
		delete object;
}

Scene::~Scene()
{
	Destroy();
	DestroyAllObjects();
}