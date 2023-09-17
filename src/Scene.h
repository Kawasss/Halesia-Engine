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
		if (objectType == OBJECT_IMPORT_EXTERNAL)
		{
			ObjectCreationData creationData = GenericLoader::LoadObjectFile(name);
			Object* objPtr = new T(creationData, GetMeshCreationObjects());
			allObjects.push_back(objPtr);
			objectsWithScripts.push_back(objPtr);
			return objPtr;
		}

		for (auto i = objectCreationDatas.begin(); i != objectCreationDatas.end(); i++)
		{
			int index = i - objectCreationDatas.begin();
			if (objectCreationDatas[index].name == name)
			{
				Object* objPtr = new T(objectCreationDatas[index], GetMeshCreationObjects());
				allObjects.push_back(objPtr);
				objectsWithScripts.push_back(objPtr);
				objectCreationDatas.erase(i);
				return objPtr;
			}
		}
		Console::WriteLine("Failed to find the creation data for the given name \"" + name + '"', MESSAGE_SEVERITY_ERROR);
		return nullptr;
	}
	
	bool HasFinishedLoading()
	{
		return loadingProcess._Is_ready();
	}

	static MeshCreationObject(*GetMeshCreationObjects)();
	std::vector<Object*> allObjects;
	std::vector<ObjectCreationData> objectCreationDatas;

	/// <summary>
	/// Submit an object without a script
	/// </summary>
	/// <param name="creationData"></param>
	/// <param name="creationObjects"></param>
	void SubmitStaticObject(const ObjectCreationData& creationData, const MeshCreationObject& creationObjects);

	/// <summary>
	/// Loads a new scene from a given scene file async
	/// </summary>
	/// <param name="path"></param>
	void LoadScene(std::string path);

	void LoadUninitializedObjects();
	virtual void Start();
	virtual void Update(Win32Window* window, float delta);
	//virtual ~Scene() {};
	void Destroy();

private:
	void LoadFileIntoScene(std::string path);

	std::future<void> loadingProcess;
	std::vector<Object*> objectsWithScripts;
	std::vector<Object*> staticObjects;

protected:
	/// <summary>
	/// Finds an object by name and returns the pointer to it
	/// </summary>
	/// <param name="name"></param>
	/// <returns>Will return a nullptr if the name doesn't match</returns>
	Object* FindObjectByName(std::string name);
	void Free(Object* object);
};