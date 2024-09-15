#include <iostream>
#include <future>

#include "core/Object.h"
#include "core/Console.h"

#include "io/SceneLoader.h"
#include "physics/Physics.h"

void Object::AwaitGeneration()
{
	if (generation.valid())
		generation.get();
	mesh.AwaitGeneration();
}

void Object::GenerateObjectWithData(const ObjectCreationData& creationData)
{
	if (creationData.hasMesh)
		mesh.Create(creationData.mesh);

	if (creationData.hitBox.shapeType != Shape::Type::None)
	{
		Shape shape = Shape::GetShapeFromType(creationData.hitBox.shapeType, creationData.hitBox.extents);
		SetRigidBody(creationData.hitBox.rigidType, shape);
		transform.position = creationData.position;
		transform.rotation = creationData.rotation;
		rigid.ForcePosition(transform);
	}
	finishedLoading = true; //maybe use mutex here or just find better solution

	#ifdef _DEBUG
	Console::WriteLine("Created new object \"" + name + "\" with unique id \"" + ToHexadecimalString(handle) + '\"', Console::Severity::Debug);
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
	state = (ObjectState)creationData.state;

	if (creationData.hasMesh)
		transform = Transform(creationData.position, creationData.rotation, creationData.scale, mesh.extents, mesh.center); // should determine the extents and center (minmax) of all meshes not just the first one
	handle = ResourceManager::GenerateHandle();
	name = creationData.name;
	transform.position = creationData.position;
	transform.rotation = creationData.rotation;
	transform.scale = creationData.scale;

	GenerateObjectWithData(creationData); // maybe async??
	Start();
}

void Object::SetMesh(const std::vector<MeshCreationData>& creationData)
{
	if (creationData.empty())
		return;
	mesh.Create(creationData[0]);
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
	newObjPtr->mesh = oldObjPtr->mesh;
	newObjPtr->transform = oldObjPtr->transform;
	newObjPtr->name = name;
	newObjPtr->finishedLoading = true;
	newObjPtr->scriptClass = script;
	newObjPtr->handle = ResourceManager::GenerateHandle();

	if (oldObjPtr->rigid.type != RigidBody::Type::None)
		newObjPtr->SetRigidBody(oldObjPtr->rigid.type, oldObjPtr->rigid.shape);

	newObjPtr->Start();
	Console::WriteLine("Duplicated object \"" + name + "\" from object \"" + oldObjPtr->name + "\" with" + (script == nullptr ? "out a script" : " a script"), Console::Severity::Debug);
}

void Object::Update(float delta)
{
	
}

Object* Object::AddChild(const ObjectCreationData& creationData)
{
	Object* obj = Create(creationData);
	children.push_back(obj);
	return obj;
}

void Object::SetRigidBody(RigidBody::Type type, Shape shape)
{
	rigid = RigidBody(shape, type, transform.position, transform.rotation);
	rigid.SetUserData(this);

	Console::WriteLine("Created " + RigidBody::TypeToString(type) + " with " + Shape::TypeToString(shape.type) + " for object \"" + name + "\"", Console::Severity::Debug);
}

bool Object::HasFinishedLoading()
{
	if (generation.valid() && generation.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		generation.get(); // change the status of the image if it is done loading
	return finishedLoading || !generation.valid();
}

void Object::Destroy(bool del)
{
	mesh.Destroy();
	for (Object* obj : children)
		obj->Destroy();
	if (parent != nullptr)
		parent->RemoveChild(this);
	rigid.Destroy();
	rigid.type = RigidBody::Type::None;
	if (del) delete this;
}