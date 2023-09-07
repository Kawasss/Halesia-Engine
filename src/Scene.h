#pragma once
#include <map>
#include <string>
#include <vector>
#include "Object.h"
#include "Camera.h"
#include "CreationObjects.h"
#include "Console.h"

class Scene 
{
public:
	Camera* camera;

	template<typename T> Object* AddCustomObject(std::string path)
	{
		Object* objPtr = new T(path, GetMeshCreationObjects());
		allObjects.push_back(objPtr);
		objectsWithScripts.push_back(objPtr);
		return objPtr;
	}
	static MeshCreationObjects(*GetMeshCreationObjects)();
	std::vector<Object*> allObjects;

	void SubmitStaticModel(std::string path, const MeshCreationObjects& creationObjects);
	virtual void Start();
	virtual void Update(Win32Window* window, float delta);
	virtual ~Scene() {};
	void Destroy();

private:
	std::vector<Object*> objectsWithScripts;
	std::vector<Object*> staticObjects;\

protected:
	Object* FindObjectByName(std::string name);
	void Free(Object* object);
};