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

	template<typename T> Object* AddCustomObject(std::string name, ObjectImportType objectType = OBJECT_IMPORT_INTERNAL)
	{
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
		Console::WriteLine("Failed to the creation data for the given name \"" + name + '"', MESSAGE_SEVERITY_ERROR);
		/*
		Object* objPtr = new T(path, GetMeshCreationObjects());
		allObjects.push_back(objPtr);
		objectsWithScripts.push_back(objPtr);
		return objPtr;*/
	}
	
	bool HasFinishedLoading()
	{
		return loadingProcess._Is_ready();
	}

	static MeshCreationObjects(*GetMeshCreationObjects)();
	std::vector<Object*> allObjects;
	std::vector<ObjectCreationData> objectCreationDatas;

	void SubmitStaticModel(const ObjectCreationData& creationData, const MeshCreationObjects& creationObjects);
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
	Object* FindObjectByName(std::string name);
	void Free(Object* object);
};