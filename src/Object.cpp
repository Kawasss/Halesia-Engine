#include <future>

#include "core/Object.h"
#include "core/Console.h"

#include "io/CreationData.h"

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

	if (creationData.hitBox.shapeType != Shape::Type::None && creationData.hitBox.rigidType != RigidBody::Type::None)
	{
		Shape shape = Shape::GetShapeFromType(creationData.hitBox.shapeType, creationData.hitBox.extents);
		SetRigidBody(creationData.hitBox.rigidType, shape);
		rigid.ForcePosition(transform);
	}
	finishedLoading = true; //maybe use mutex here or just find better solution

	#ifdef _DEBUG
	Console::WriteLine("Created new object \"" + name + '"', Console::Severity::Debug);
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
	handle = ResourceManager::GenerateHandle();

	name = creationData.name;
	scriptClass = customClassPointer;
	state = (ObjectState)creationData.state;

	transform.position = creationData.position;
	transform.rotation = creationData.rotation;
	transform.scale    = creationData.scale;

	if (creationData.hasMesh)
		transform = Transform(creationData.position, creationData.rotation, creationData.scale, mesh.extents, mesh.center);

	GenerateObjectWithData(creationData); // maybe async??
	Start();
}

void Object::SetMesh(const MeshCreationData& creationData)
{
	mesh.Create(creationData); // Create will automatically destroy old resources
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
	{
		obj->Destroy(false);
		delete obj;
	}

	rigid.Destroy();
	rigid.type = RigidBody::Type::None;
	if (del) delete this;
}