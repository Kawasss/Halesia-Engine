#include <algorithm>
#include <execution>
#include "Scene.h"
#include "Console.h"
#include "SceneLoader.h"

MeshCreationObjects (*Scene::GetMeshCreationObjects)() = nullptr;
Camera* Scene::defaultCamera = new Camera();

SceneLoader loader("");
void Scene::LoadScene(std::string path)
{
	loadingProcess = std::async(&Scene::LoadFileIntoScene, this, path);
}

void Scene::LoadFileIntoScene(std::string path)
{
	loader = SceneLoader(path);
	loader.LoadScene();
	
	objectCreationDatas = loader.objects;
	Start();
	LoadUninitializedObjects();
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

void Scene::SubmitStaticModel(const ObjectCreationData& creationData, const MeshCreationObjects& creationObjects)
{
	Object* objPtr = new Object(creationData, creationObjects);

	allObjects.push_back(objPtr);
	staticObjects.push_back(objPtr);
}

template<typename T> void EraseMemberFromVector(std::vector<T>& vector, T memberToErase)
{
	for (auto i = vector.begin(); i != vector.end(); i++)
		if (*i == memberToErase)
			vector.erase(i);
}

void Scene::Free(Object* object)
{
	for (auto i = allObjects.begin(); i != allObjects.end(); i++)
		if (*i == object) // this checks if the object exists
		{
			allObjects.erase(i);

			for (auto i = objectsWithScripts.begin(); i != objectsWithScripts.end(); i++)
				if (*i == object)
				{
					objectsWithScripts.erase(i);
					Console::WriteLine("Freed object with script \"" + object->name + '\"');
					object->Destroy();
					return;
				}
			EraseMemberFromVector(staticObjects, object); // if the object doesnt have a script it must be static
			Console::WriteLine("Freed static object \"" + object->name + '\"');
			object->Destroy();
			return;
		}
	Console::WriteLine("Failed to free an object, because it isn't registered in the scene. Perhaps the object is already freed?", MESSAGE_SEVERITY_ERROR);
}

void Scene::Start()
{
	FindObjectByName("cheese");
	Free(nullptr);
}

void Scene::Update(Win32Window* window, float delta)
{
	if (!HasFinishedLoading())
		return;

	camera->Update(window, delta);
	std::for_each(std::execution::par, objectsWithScripts.begin(), objectsWithScripts.end(), [&](Object* object) // update all of the scripts in parallel
		{
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