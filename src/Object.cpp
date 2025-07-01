#include <Windows.h>
#include <future>

#include "core/Object.h"
#include "core/Console.h"
#include "core/Scene.h"

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

Object* Object::Create(const ObjectCreationData& creationData)
{
	Object* ptr = new Object(InheritType::Base);
	ptr->Initialize(creationData);
	return ptr;
}

void Object::Initialize(const ObjectCreationData& creationData)
{
	handle = ResourceManager::GenerateHandle();

	name = creationData.name;
	state = (ObjectState)creationData.state;

	transform.position = creationData.position;
	transform.rotation = creationData.rotation;
	transform.scale    = creationData.scale;

	if (creationData.hasMesh)
		transform = Transform(creationData.position, creationData.rotation, creationData.scale);

	if (!creationData.children.empty())
		children.reserve(creationData.children.size());
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

Object* Object::CreateShallowCopy() const
{
	ObjectCreationData creationData{};
	creationData.type = static_cast<ObjectCreationData::Type>(type);

	Object* pCopy = scene->AddObject(creationData, parent);

	DuplicateBaseDataTo(pCopy);
	DuplicateDataTo(pCopy);

	return pCopy;
}

void Object::DuplicateDataTo(Object* pObject) const
{

}

void Object::DuplicateBaseDataTo(Object* pObject) const
{
	pObject->type = type;
	pObject->transform = transform;
	pObject->name = name + "_copy";
	pObject->state = state;
	pObject->finishedLoading = true;
	pObject->handle = ResourceManager::GenerateHandle();
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