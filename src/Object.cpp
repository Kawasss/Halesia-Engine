#include <Windows.h>
#include <future>

#include "core/Object.h"
#include "core/Console.h"

#include "io/CreationData.h"

#include "physics/Physics.h"

#include "ResourceManager.h"

Object::Object(InheritType type) : type(type)
{
	
}

void Object::AwaitGeneration()
{
	if (generation.valid())
		generation.get();
}

Object* Object::Create(const ObjectCreationData& creationData, void* customClassPointer)
{
	Object* ptr = new Object(InheritType::Base);
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
		transform = Transform(creationData.position, creationData.rotation, creationData.scale);

	if (!creationData.children.empty())
		children.reserve(creationData.children.size());

	Start();
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
	newObjPtr->transform = oldObjPtr->transform;
	newObjPtr->name = name;
	newObjPtr->finishedLoading = true;
	newObjPtr->scriptClass = script;
	newObjPtr->handle = ResourceManager::GenerateHandle();

	newObjPtr->Start();
	Console::WriteLine("Duplicated object \"" + name + "\" from object \"" + oldObjPtr->name + "\" with" + (script == nullptr ? "out a script" : " a script"), Console::Severity::Debug);
}

Object* Object::AddChild(const ObjectCreationData& creationData)
{
	Object* obj = Create(creationData);
	
	AddChild(obj);
	return obj;
}

bool Object::HasFinishedLoading()
{
	if (generation.valid() && generation.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		generation.get(); // change the status of the image if it is done loading
	return finishedLoading || !generation.valid();
}

bool Object::IsType(InheritType cmp) const
{
	return type == cmp;
}

Object::~Object()
{
	for (Object* obj : children)
		delete obj;
}