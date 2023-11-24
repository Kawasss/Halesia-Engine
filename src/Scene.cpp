#include <algorithm>
#include <execution>
#include <optional>
#include "Scene.h"
#include "Console.h"
#include "SceneLoader.h"

MeshCreationObject (*Scene::GetMeshCreationObjects)() = nullptr;
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
		Object* objPtr = new Object(creationData, GetMeshCreationObjects());
		allObjects.push_back(objPtr);
		staticObjects.push_back(objPtr);
	}
}

Object* Scene::FindObjectByName(std::string name)
{
	for (Object* object : allObjects)
		if (object->name == name)
			return object;
	Console::WriteLine("Failed to find an object matching the name \"" + name + '"', MESSAGE_SEVERITY_ERROR);
	return nullptr; //not really that safe
}

std::string GetNameFromPath(std::string path)
{
	std::string fileNameWithExtension = path.substr(path.find_last_of("/\\") + 1);
	return fileNameWithExtension.substr(0, fileNameWithExtension.find_last_of('.'));
}

Object* Scene::SubmitStaticObject(const ObjectCreationData& creationData)
{
	Object* objPtr = new Object(creationData, GetMeshCreationObjects());

	objectHandles[objPtr->handle] = objPtr;
	allObjects.push_back(objPtr);
	staticObjects.push_back(objPtr);

	return objPtr;
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

void Scene::Free(Object* object)
{
	if (object == nullptr)
	{
		Console::WriteLine("Failed to delete the given object since it is a null pointer", MESSAGE_SEVERITY_ERROR);
		return;
	}
	
	std::optional<std::vector<Object*>::iterator> removeObjectIndex;
	bool objectIsStatic = true;

	for (auto i = allObjects.begin(); i != allObjects.end(); i++)
		if (*i == object) // this checks if the object exists
		{
			removeObjectIndex = i;

			for (auto i = objectsWithScripts.begin(); i != objectsWithScripts.end(); i++)
				if (*i == object)
					objectIsStatic = false;
			break;
		}
	if (!removeObjectIndex.has_value())
	{
		Console::WriteLine("Failed to free an object, because it isn't registered in the scene. Maybe the object is already freed?", MESSAGE_SEVERITY_ERROR);
		return;
	}
	
	allObjects.erase(removeObjectIndex.value());
	EraseMemberFromVector(objectIsStatic ? staticObjects : objectsWithScripts, object);
	object->Destroy();
	Console::WriteLine("Freed " + (std::string)(objectIsStatic ? "static" : "scripted") + " object "/* + object->name*/, MESSAGE_SEVERITY_DEBUG);
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
			else
				object->Update(delta);
		});
}

void Scene::Destroy()
{
	for (Object* object : allObjects)
		object->Destroy();
	delete this;
}