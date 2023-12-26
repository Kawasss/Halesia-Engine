#include "Object.h"
#include <iostream>
#include <future>
#include "Console.h"
#include "SceneLoader.h"
#include "physics/Physics.h"

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

	if (creationData.hitBox.shapeType != SHAPE_TYPE_NONE)
	{
		Shape shape = Shape::GetShapeFromType(creationData.hitBox.shapeType, creationData.hitBox.extents);
		AddRigidBody(creationData.hitBox.rigidType, shape);
		transform.position = creationData.position;
		transform.rotation = creationData.rotation;
		rigid.ForcePosition(transform);
	}

	finishedLoading = true; //maybe use mutex here or just find better solution

	#ifdef _DEBUG
	Console::WriteLine("Created new object \"" + name + "\" with unique id \"" + std::to_string(handle) + '\"', MESSAGE_SEVERITY_DEBUG);
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

	if (!creationData.meshes.empty())
		transform = Transform(creationData.position, creationData.rotation, creationData.scale, meshes[0].extents, meshes[0].center); // should determine the extents and center (minmax) of all meshes not just the first one
	handle = ResourceManager::GenerateHandle();
	name = creationData.name;
	transform.position = creationData.position;
	transform.rotation = creationData.rotation;
	transform.scale = creationData.scale;

	GenerateObjectWithData(creationObject, creationData); // maybe async??
}

Object::Object(const ObjectCreationData& creationData, const ObjectCreationObject& creationObject)
{
	CreateObject(nullptr, creationData, creationObject);
	//meshes.resize(creationData.meshes.size());
	//
	//if (!creationData.meshes.empty())
	//	transform = Transform(creationData.position, creationData.rotation, creationData.scale, creationData.meshes[0].extents, creationData.meshes[0].center); // should determine the extents and center (minmax) of all meshes not just the first one
	//handle = ResourceManager::GenerateHandle();
	//name = creationData.name;

	//generationProcess = std::async(&Object::GenerateObjectWithData, this, creationObject, creationData);
}

void Object::AddMesh(const std::vector<MeshCreationData>& creationData, const MeshCreationObject& creationObject)
{
	for (int i = 0; i < creationData.size(); i++)
	{
		meshes.push_back({});
		meshes.back().Create(creationObject, creationData[i]);
	}
}

void Object::Duplicate(Object* oldObjPtr, Object* newObjPtr, std::string name, void* script)
{
	newObjPtr->meshes = oldObjPtr->meshes;
	newObjPtr->transform = oldObjPtr->transform;
	newObjPtr->name = name;
	newObjPtr->finishedLoading = true;
	newObjPtr->scriptClass = script;
	newObjPtr->handle = ResourceManager::GenerateHandle();

	newObjPtr->AddRigidBody(oldObjPtr->rigid.type, oldObjPtr->rigid.shape);

	Console::WriteLine("Duplicated object \"" + name + "\" from object \"" + oldObjPtr->name + "\" with" + (script == nullptr ? "out a script" : " a script"), MESSAGE_SEVERITY_DEBUG);
}

void Object::Update(float delta)
{
	
}

void Object::AddRigidBody(RigidBodyType type, Shape shape)
{
	rigid = RigidBody(shape, type, transform.position, transform.rotation);
	rigid.SetUserData(this);

	std::cout << "Created new rigid body (" << RigidBodyTypeToString(type) << ") with shape " << ShapeTypeToString(shape.type) << " for object \"" << name << "\"\n";
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

std::string Object::ObjectStateToString(ObjectState state)
{
	switch (state)
	{
	case OBJECT_STATE_DISABLED:
		return "OBJECT_STATE_DISABLED";
	case OBJECT_STATE_INVISIBLE:
		return "OBJECT_STATE_INVISIBLE";
	case OBJECT_STATE_VISIBLE:
		return "OBJECT_STATE_VISIBLE";
	}
	return "";
}