#include "Object.h"
#include <iostream>
#include <future>
#include "Console.h"
#include "io/SceneLoader.h"
#include "physics/Physics.h"

void Object::AwaitGeneration()
{
	if (generationProcess.valid())
		generationProcess.get();
	for (Mesh& mesh : meshes)
		mesh.AwaitGeneration();
}

void Object::GenerateObjectWithData(const ObjectCreationData& creationData)
{
	/*#ifdef _DEBUG
	Console::WriteLine("Attempting to create new model \"" + name + '\"', MESSAGE_SEVERITY_DEBUG);
	#endif*/

	for (int i = 0; i < creationData.meshes.size(); i++)
		meshes[i].Create(creationData.meshes[i]);

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

void Object::RecreateMeshes()
{
	for (Mesh& mesh : meshes)
		mesh.Recreate();
}

Object* Object::Create(const ObjectCreationData& creationData, void* customClassPointer)
{
	Object* ptr = new Object();
	ptr->scriptClass = customClassPointer;
	ptr->meshes.resize(creationData.meshes.size());

	if (!creationData.meshes.empty())
		ptr->transform = Transform(creationData.position, creationData.rotation, creationData.scale, ptr->meshes[0].extents, ptr->meshes[0].center); // should determine the extents and center (minmax) of all meshes not just the first one
	ptr->handle = ResourceManager::GenerateHandle();
	ptr->name = creationData.name;
	ptr->transform.position = creationData.position;
	ptr->transform.rotation = creationData.rotation;
	ptr->transform.scale = creationData.scale;

	ptr->GenerateObjectWithData(creationData); // maybe async??
	return ptr;
}

void Object::AddMesh(const std::vector<MeshCreationData>& creationData)
{
	for (int i = 0; i < creationData.size(); i++)
	{
		meshes.push_back({});
		meshes.back().Create(creationData[i]);
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

