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

template<typename Class, typename Type>
inline Type* GetPointerToClassMember(const Class* parent, Type Class::* member)
{
	return reinterpret_cast<Type*>((char*)parent + reinterpret_cast<size_t>(&(reinterpret_cast<Class*>(0)->*member))); // calculates the offset of the member relative to the class
}

template<typename Type, typename Class>
inline Class* GetItemByMember(Type Class::* member, Type comp, const std::vector<Class*>& vector)
{
	for (size_t index = 0; index < vector.size(); index++)
		if (*GetPointerToClassMember(vector[index], member) == comp)
			return vector[index];
	return nullptr; // unsafe
}

SceneLoader loader;
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
	}
}

Object* Scene::GetObjectByName(std::string name)
{
	return GetItemByMember(&Object::name, name, allObjects);
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
	objPtr->scene = this;
}

 inline void EraseMemberFromVector(std::vector<Object*>& vector, Object* memberToErase)
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
		object->Destroy();
		
		Console::WriteLine("Freed " + (std::string)(object->HasScript() ? "static" : "scripted") + " object "/* + object->name*/, MESSAGE_SEVERITY_DEBUG);
		return;
	}
	Console::WriteLine("Failed to free an object, because it isn't registered in the scene. Maybe the object is already freed?", MESSAGE_SEVERITY_ERROR);
}

void Scene::UpdateCamera(Window* window, float delta)
{
	camera->Update(window, delta);
}

void Scene::UpdateScripts(float delta)
{
	if (!HasFinishedLoading())
		return;
	
	//std::for_each(std::execution::par, objectsWithScripts.begin(), objectsWithScripts.end(), [&](Object* object) // update all of the scripts in parallel
	//	{
	//		std::lock_guard<std::mutex> lockGuard(object->mutex);
	//		if (object->shouldBeDestroyed)
	//			Free(object);
	//		else if (object->state != OBJECT_STATE_DISABLED)
	//			object->Update(delta);
	//	});
	
	for (int i = 0; i < allObjects.size(); i++)
	{
		if (!allObjects[i]->HasScript() || allObjects[i]->shouldBeDestroyed || allObjects[i]->state == OBJECT_STATE_DISABLED)
			continue;
		allObjects[i]->Update(delta);
	}
}

void Scene::CollectGarbage()
{
	for (auto iter = allObjects.begin(); iter < allObjects.end(); iter++)
	{
		Object* obj = *iter;
		if (!obj->shouldBeDestroyed)
			continue;
		allObjects.erase(iter);
		obj->Destroy();
		iter = allObjects.begin();
	}
}

void Scene::TransferObjectOwnership(Object* newOwner, Object* child)
{
	objectHandles.erase(child->handle);
	std::vector<Object*>::iterator iter = std::find(allObjects.begin(), allObjects.end(), child);
	if (iter != allObjects.end())
		allObjects.erase(iter);
	newOwner->AddChild(child);
}

void Scene::Destroy()
{
	for (Object* object : allObjects)
		object->Destroy();
	delete this;
}