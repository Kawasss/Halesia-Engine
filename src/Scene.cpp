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

SceneLoader loader("");
void Scene::LoadScene(std::string path)
{
	sceneIsLoading = true;
	loadingProcess = std::async(&Scene::LoadFileIntoScene, this, path);
}

void Scene::LoadFileIntoScene(std::string path)
{
	loader = SceneLoader(path);
	loader.LoadScene();
	
	objectCreationDatas = loader.objects;
	Start();
	LoadUninitializedObjects();
	sceneIsLoading = false;
}

void Scene::LoadUninitializedObjects()
{
	camera->position = loader.cameraPos;
	camera->yaw = loader.cameraYaw;
	camera->pitch = loader.cameraPitch;
	for (const ObjectCreationData& creationData : objectCreationDatas)
	{
		Object* objPtr = Object::Create(creationData);
		allObjects.push_back(objPtr);
		staticObjects.push_back(objPtr);
	}
}

Object* Scene::GetObjectByName(std::string name)
{
	for (Object* object : allObjects)
		if (object->name == name)
			return object;
	Console::WriteLine("Failed to find an object matching the name \"" + name + '"', MESSAGE_SEVERITY_ERROR);
	return nullptr; //not really that safe
}

Object* Scene::GetObjectByHandle(Handle handle)
{
	if (objectHandles.count(handle) == 0)
		throw std::runtime_error("Failed to get the object related with handle \"" + std::to_string(handle) + "\"");
	return objectHandles[handle];
}

std::string GetNameFromPath(std::string path)
{
	std::string fileNameWithExtension = path.substr(path.find_last_of("/\\") + 1);
	return fileNameWithExtension.substr(0, fileNameWithExtension.find_last_of('.'));
}

Object* Scene::AddStaticObject(const ObjectCreationData& creationData)
{
	Object* objPtr = Object::Create(creationData);

	objectHandles[objPtr->handle] = objPtr;
	allObjects.push_back(objPtr);
	staticObjects.push_back(objPtr);

	return objPtr;
}

bool Scene::IsObjectHandleValid(Handle handle) 
{ 
	return objectHandles.count(handle) > 0; 
}

bool Scene::HasFinishedLoading()
{
	return loadingProcess._Is_ready() || !sceneIsLoading;
}

bool Scene::GetInternalObjectCreationData(std::string name, ObjectCreationData& creationData)
{
	for (auto i = objectCreationDatas.begin(); i != objectCreationDatas.end(); i++)
	{
		if (i->name != name)
			continue;

		creationData = *i;
		objectCreationDatas.erase(i);
		return true;
	}
	return false;
}

void Scene::RegisterObjectPointer(Object* objPtr, bool isCustom)
{
	objectHandles[objPtr->handle] = objPtr;
	allObjects.push_back(objPtr);
	if (isCustom)
		objectsWithScripts.push_back(objPtr);
}

 void EraseMemberFromVector(std::vector<Object*>& vector, Object* memberToErase)
{
	for (auto i = vector.begin(); i < vector.end(); i++)
		if (*i == memberToErase)
		{
			vector.erase(i);
			return;
		}
}

 Object* Scene::DuplicateStaticObject(Object* objPtr, std::string name)
 {
	 Object* newPtr = new Object();
	 Object::Duplicate(objPtr, newPtr, name, nullptr);
	 RegisterObjectPointer(newPtr, false);

	 return newPtr;
}

void Scene::Free(Object* object)
{
	if (object == nullptr)
	{
		Console::WriteLine("Failed to delete the given object since it is a null pointer", MESSAGE_SEVERITY_ERROR);
		return;
	}

	for (int i = 0; i < allObjects.size(); i++)
	{
		if (allObjects[i] != object)
			continue;

		allObjects.erase(allObjects.begin() + i);
		EraseMemberFromVector(object->HasScript() ? staticObjects : objectsWithScripts, object);
		object->Destroy();

		Console::WriteLine("Freed " + (std::string)(object->HasScript() ? "static" : "scripted") + " object "/* + object->name*/, MESSAGE_SEVERITY_DEBUG);


		break;
	}
	Console::WriteLine("Failed to free an object, because it isn't registered in the scene. Maybe the object is already freed?", MESSAGE_SEVERITY_ERROR);
}

void Scene::UpdateCamera(Win32Window* window, float delta)
{
	camera->Update(window, delta);
}

void Scene::UpdateScripts(float delta)
{
	if (!HasFinishedLoading())
		return;
	
	std::for_each(std::execution::par, objectsWithScripts.begin(), objectsWithScripts.end(), [&](Object* object) // update all of the scripts in parallel
		{
			std::lock_guard<std::mutex> lockGuard(object->mutex);
			if (object->shouldBeDestroyed)
				Free(object);
			else if (object->state != OBJECT_STATE_DISABLED)
				object->Update(delta);
		});
}

void Scene::Destroy()
{
	for (Object* object : allObjects)
		object->Destroy();
	delete this;
}