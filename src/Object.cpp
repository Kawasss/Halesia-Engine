#include <iostream>
#include <future>

#include "core/Object.h"
#include "core/Console.h"

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

Object* Object::Create(const ObjectCreationData& creationData, void* customClassPointer)
{
	Object* ptr = new Object();
	ptr->Initialize(creationData, customClassPointer);
	return ptr;
}

void Object::Initialize(const ObjectCreationData& creationData, void* customClassPointer)
{
	name = creationData.name;
	scriptClass = customClassPointer;
	meshes.resize(creationData.meshes.size());

	if (!creationData.meshes.empty())
		transform = Transform(creationData.position, creationData.rotation, creationData.scale, meshes[0].extents, meshes[0].center); // should determine the extents and center (minmax) of all meshes not just the first one
	handle = ResourceManager::GenerateHandle();
	name = creationData.name;
	transform.position = creationData.position;
	transform.rotation = creationData.rotation;
	transform.scale = creationData.scale;

	GenerateObjectWithData(creationData); // maybe async??
	Start();
}

void Object::AddMesh(const std::vector<MeshCreationData>& creationData)
{
	for (int i = 0; i < creationData.size(); i++)
	{
		meshes.push_back({});
		meshes.back().Create(creationData[i]);
	}
}

void Object::AddChild(Object* object)
{
	children.push_back(object);
	object->parent = this;
	object->transform.parent = &transform;
}

void Object::DeleteChild(Object* child)
{
	std::vector<Object*>::iterator iter = std::find(children.begin(), children.end(), child);
	if (iter == children.end())
		return;

	(*iter)->Destroy();
	children.erase(iter);
	delete child;
}

void Object::RemoveChild(Object* child)
{
	std::vector<Object*>::iterator iter = std::find(children.begin(), children.end(), child);
	if (iter == children.end())
		return;
	children.erase(iter);
}

void Object::TransferChild(Object* child, Object* destination)
{
	std::vector<Object*>::iterator iter = std::find(children.begin(), children.end(), child);
	if (iter == children.end())
		return;

	children.erase(iter);
	destination->children.push_back(child);
	child->parent = destination;
}

void Object::Duplicate(Object* oldObjPtr, Object* newObjPtr, std::string name, void* script)
{
	newObjPtr->meshes = oldObjPtr->meshes;
	newObjPtr->transform = oldObjPtr->transform;
	newObjPtr->name = name;
	newObjPtr->finishedLoading = true;
	newObjPtr->scriptClass = script;
	newObjPtr->handle = ResourceManager::GenerateHandle();

	if (oldObjPtr->rigid.type != RIGID_BODY_NONE)
		newObjPtr->AddRigidBody(oldObjPtr->rigid.type, oldObjPtr->rigid.shape);

	newObjPtr->Start();
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
	for (Object* obj : children)
		obj->Destroy();
	if (parent != nullptr)
		parent->RemoveChild(this);
	delete this;
}