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
#include "core/Rigid3DObject.h"
#include "core/LightObject.h"

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
	 case ObjectCreationData::Type::Rigid3D:
		 pObject = Rigid3DObject::Create(creationData);
		 break;
	 case ObjectCreationData::Type::Light:
		 pObject = LightObject::Create(creationData);
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
	Object::Free(pObject);
}

void Scene::UpdateCamera(Window* pWindow, float delta)
{
	camera->UpdateVelocityMatrices();
	camera->Update(pWindow, delta);
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
	CollectGarbageRecursive(allObjects);
}

void Scene::CollectGarbageRecursive(std::vector<Object*>& base)
{
	for (int i = 0; i < base.size(); i++)
	{
		Object* obj = base[i];
		if (!obj->ShouldBeDestroyed())
		{
			CollectGarbageRecursive(obj->GetChildren());
			continue;
		}

		base.erase(base.begin() + i);
		i--;

		delete obj;
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