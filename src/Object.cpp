#include "Object.h"
#include <iostream>
#include <future>
#include "Console.h"
#include "SceneLoader.h"

void Object::AwaitGeneration()
{
	if (generationProcess.valid())
		generationProcess.get();
	for (Mesh& mesh : meshes)
		mesh.AwaitGeneration();
}

void Object::GenerateObjectWithData(const ObjectCreationObject& creationObject, const ObjectCreationData& creationData)
{
	/*#ifdef _DEBUG
	Console::WriteLine("Attempting to create new model \"" + name + '\"', MESSAGE_SEVERITY_DEBUG);
	#endif*/

	for (int i = 0; i < creationData.meshes.size(); i++)
		meshes[i].Create(creationObject, creationData.meshes[i]);
	finishedLoading = true; //maybe use mutex here or just find better solution

	#ifdef _DEBUG
	Console::WriteLine("Created new object \"" + name + "\" with unique id \"" + std::to_string(hObject) + '\"', MESSAGE_SEVERITY_DEBUG);
	#endif
}

void Object::RecreateMeshes(const MeshCreationObject& creationObject)
{
	for (Mesh& mesh : meshes)
		mesh.Recreate(creationObject);
}
void Object::CreateObject(void* customClassPointer, const ObjectCreationData& creationData, const MeshCreationObject& creationObject)
{
	scriptClass = customClassPointer;
	meshes.resize(creationData.meshes.size());

	transform = Transform(creationData.position, creationData.rotation, creationData.scale, meshes[0].extents, meshes[0].center); // should determine the extents and center (minmax) of all meshes not just the first one
	hObject = ResourceManager::GenerateHandle();
	name = creationData.name;

	GenerateObjectWithData(creationObject, creationData); // maybe async??
}

Object::Object(const ObjectCreationData& creationData, const ObjectCreationObject& creationObject)
{
	meshes.resize(creationData.meshes.size());

	transform = Transform(creationData.position, creationData.rotation, creationData.scale, creationData.meshes[0].extents, creationData.meshes[0].center); // should determine the extents and center (minmax) of all meshes not just the first one
	hObject = ResourceManager::GenerateHandle();
	name = creationData.name;

	generationProcess = std::async(&Object::GenerateObjectWithData, this, creationObject, creationData);
}

bool Object::HasFinishedLoading()
{
	/*for (Mesh& mesh : meshes)
		if (!mesh.HasFinishedLoading())
			return false;*/
	return finishedLoading || generationProcess._Is_ready();
}

void Object::Destroy()
{
	for (Mesh& mesh : meshes)
		mesh.Destroy();
	delete this;
}