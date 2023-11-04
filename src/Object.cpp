#include "Object.h"
#include <iostream>
#include <future>
#include "Console.h"

void Object::AwaitGeneration()
{
	generationProcess.get();
}

void Object::GenerateObjectWithData(const ObjectCreationObject& creationObject, const ObjectCreationData& creationData)
{
	name = creationData.name;

	#ifdef _DEBUG
	Console::WriteLine("Attempting to create new model \"" + name + '\"', MESSAGE_SEVERITY_DEBUG);
	#endif

	for (int i = 0; i < creationData.meshes.size(); i++)
		meshes[i].Create(creationObject, creationData.meshes[i]);

	transform = Transform(creationData.position, creationData.rotation, creationData.scale, meshes[0].extents, meshes[0].center); // should determine the extents and center (minmax) of all meshes not just the first one
	hObject = ResourceManager::GenerateHandle();

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
	GenerateObjectWithData(creationObject, creationData); // maybe async??
}

Object::Object(const ObjectCreationData& creationData, const ObjectCreationObject& creationObject)
{
	meshes.resize(creationData.meshes.size());
	generationProcess = std::async(&Object::GenerateObjectWithData, this, creationObject, creationData);
}

bool Object::HasFinishedLoading()
{
	return finishedLoading || generationProcess._Is_ready();
}

void Object::Destroy()
{
	for (Mesh& mesh : meshes)
		mesh.Destroy();
	delete this;
}