#include <algorithm>
#include <execution>
#include <optional>

#include "io/SceneLoader.h"
#include "system/Window.h"

#include "core/Camera.h"
#include "core/Object.h"
#include "core/Scene.h"
#include "core/Console.h"

Camera* Scene::defaultCamera = new Camera();

bool Scene::HasFinishedLoading()
{
	return true;
}

void Scene::RegisterObjectPointer(Object* objPtr)
{
	allObjects.push_back(objPtr);
	objPtr->SetParentScene(this);
}

static void EraseMemberFromVector(std::vector<Object*>& vector, Object* memberToErase)
{
	 auto it = std::find(vector.begin(), vector.end(), memberToErase);
	 if (it != vector.end())
		 vector.erase(it);
}

 Object* Scene::AddObject(const ObjectCreationData& creationData)
 {
	 Object* objPtr = Object::Create(creationData);

	 RegisterObjectPointer(objPtr);

	 return objPtr;
 }

 Object* Scene::DuplicateObject(Object* objPtr, std::string name)
 {
	 Object* newPtr = new Object();
	 Object::Duplicate(objPtr, newPtr, name, nullptr);
	 RegisterObjectPointer(newPtr);

	 return newPtr;
 }

void Scene::Free(Object* object)
{
	auto it = std::find(allObjects.begin(), allObjects.end(), object);

	if (it == allObjects.end())
	{
		Console::WriteLine("Failed to free an object, because it isn't registered in the scene. The given object has not been freed.", Console::Severity::Error);
		return;
	}

	allObjects.erase(it);
	object->Destroy();
}

void Scene::UpdateCamera(Window* window, float delta)
{
	camera->Update(window, delta);
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
		obj->Destroy();
		iter = allObjects.begin();
	}
}

void Scene::TransferObjectOwnership(Object* newOwner, Object* child)
{
	std::vector<Object*>::iterator iter = std::find(allObjects.begin(), allObjects.end(), child);
	if (iter != allObjects.end())
		allObjects.erase(iter);
	newOwner->AddChild(child);
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