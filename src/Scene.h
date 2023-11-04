#pragma once
#include <map>
#include <string>
#include <vector>
#include "Object.h"
#include "Camera.h"
#include "CreationObjects.h"
#include "Console.h"

enum ObjectImportType
{
	/// <summary>
	/// The object is from an external source, i.e. an .obj file.
	/// </summary>
	OBJECT_IMPORT_EXTERNAL,

	/// <summary>
	/// The object is from inside the scene's CRS file.
	/// </summary>
	OBJECT_IMPORT_INTERNAL
};

class Scene 
{
public:
	static Camera* defaultCamera;

	Camera* camera = defaultCamera;

	/// <summary>
	/// Creates an object with a given class as attached script. Loading an external file will not happen async, so load times will be longer
	/// </summary>
	/// <typeparam name="T"></typeparam>
	/// <param name="name">: The name of the object to find and create. If objectType is OBJECT_IMPORT_EXTERNAL this needs to be the path to the file</param>
	/// <param name="objectType">: The type of object: either from inside the scene file or an outside file</param>
	/// <returns>A pointer to the base object of the custom object. A nullptr will be returned if an object matching the name can't be found</returns>
	template<typename T> Object* AddCustomObject(std::string name, ObjectImportType objectType = OBJECT_IMPORT_INTERNAL)
	{
		ObjectCreationData creationData;
		Object* objPtr = nullptr;
		T* customPointer = nullptr;
		bool foundCreationData = false;

		if (objectType == OBJECT_IMPORT_EXTERNAL)
		{
			creationData = GenericLoader::LoadObjectFile(name, 1);
		}
		else // if (objectType == OBJECT_IMPORT_INTERNAL)
		{
			for (auto i = objectCreationDatas.begin(); i != objectCreationDatas.end(); i++)
			{
				int index = i - objectCreationDatas.begin();
				if (objectCreationDatas[index].name == name)
				{
					creationData = objectCreationDatas[index];
					objectCreationDatas.erase(i);
					foundCreationData = true;
					break;
				}
			}
		}

		if (!foundCreationData && objectType == OBJECT_IMPORT_INTERNAL)
		{
			Console::WriteLine("Failed to find the creation data for the given name \"" + name + '"', MESSAGE_SEVERITY_ERROR);
			return nullptr;
		}

		customPointer = new T();
		objPtr = customPointer;
		objPtr->CreateObject(customPointer, creationData, GetMeshCreationObjects());

		allObjects.push_back(objPtr);
		objectsWithScripts.push_back(objPtr);
		return objPtr;
	}
	
	bool HasFinishedLoading()
	{
		return loadingProcess._Is_ready() || !sceneIsLoading;
	}

	static MeshCreationObject(*GetMeshCreationObjects)();
	std::vector<Object*> allObjects;
	std::vector<ObjectCreationData> objectCreationDatas;

	/// <summary>
	/// Submit an object without a script
	/// </summary>
	/// <param name="creationData"></param>
	/// <param name="creationObjects"></param>
	Object* SubmitStaticObject(const ObjectCreationData& creationData);

	/// <summary>
	/// Loads a new scene from a given scene file async
	/// </summary>
	/// <param name="path"></param>
	void LoadScene(std::string path);

	void LoadUninitializedObjects();
	virtual void Start();
	void UpdateCamera(Win32Window* window, float delta);
	void UpdateScripts(float delta);
	virtual void Update(float delta) {};
	//virtual ~Scene() {};
	void Destroy();

private:
	void LoadFileIntoScene(std::string path);

	std::future<void> loadingProcess;
	std::vector<Object*> objectsWithScripts;
	std::vector<Object*> staticObjects;

	bool sceneIsLoading = false;

protected:
	/// <summary>
	/// Finds an object by name and returns the pointer to it
	/// </summary>
	/// <param name="name"></param>
	/// <returns>Will return a nullptr if the name doesn't match</returns>
	Object* FindObjectByName(std::string name);
	void Free(Object* object);
};