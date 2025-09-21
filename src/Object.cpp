#include <Windows.h>
#include <future>

#include "core/Object.h"
#include "core/Console.h"
#include "core/Scene.h"

#include "io/CreationData.h"
#include "io/BinaryStream.h"

#include "ResourceManager.h"

Object::Object(InheritType type) : type(type)
{
	
}

void Object::ShallowUpdate(float delta)
{
	this->Update(delta);
}

void Object::FullUpdate(float delta)
{
	this->Update(delta);
	for (Object* pChild : children)
		pChild->FullUpdate(delta);
}

void Object::ShallowUpdateTransform()
{
	transform.CalculateModelMatrix();
}

void Object::FullUpdateTransform()
{
	this->ShallowUpdateTransform();
	for (Object* pChild : children)
		pChild->FullUpdateTransform();
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

	if (!creationData.unknownData.empty())
		DeserializeSelf(creationData.unknownData);
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

std::vector<char> Object::Serialize() const
{
	BinaryStream stream;
	SerializeIntoStream(stream);
	return stream.data;
}

void Object::SerializeIntoStream(BinaryStream& stream) const
{
	SerializeHeader(stream);
	SerializeName(stream);
	SerializeTransform(stream);

	SerializeSelf(stream);

	//SerializeChildren(stream);
}

void Object::SerializeHeader(BinaryStream& stream) const
{
	stream << static_cast<std::underlying_type_t<InheritType>>(type);
}

void Object::SerializeName(BinaryStream& stream) const
{
	stream << static_cast<uint32_t>(name.size());
	for (char ch : name)
		stream << ch;
}

void Object::SerializeTransform(BinaryStream& stream) const
{
	stream << transform.position.x << transform.position.y << transform.position.z;
	stream << transform.scale.x << transform.scale.y << transform.scale.z;
	stream << transform.rotation.w << transform.rotation.x << transform.rotation.y << transform.rotation.z;
}

void Object::SerializeChildren(BinaryStream& stream) const
{
	size_t childCount = children.size();
	stream << childCount;

	for (Object* pChild : children)
	{
		pChild->SerializeIntoStream(stream);
	}
}

void Object::Deserialize(const BinarySpan& stream)
{
	DeserializeName(stream);
	DeserializeTransform(stream);

	DeserializeSelf(stream);
}

void Object::DeserializeName(const BinarySpan& stream)
{
	uint32_t size = 0;
	stream >> size;

	name.resize(size);
	for (char ch : name)
		stream >> ch;
}

void Object::DeserializeTransform(const BinarySpan& stream)
{
	stream >> transform.position.x >> transform.position.y >> transform.position.z;
	stream >> transform.scale.x >> transform.scale.y >> transform.scale.z;
	stream >> transform.rotation.w >> transform.rotation.x >> transform.rotation.y >> transform.rotation.z;
}

bool Object::DeserializeIntoCreationData(const BinarySpan& stream, ObjectCreationData& ret)
{
	if (stream.data.size() < 4)
		return false;

	InheritType type = GetInheritTypeFromStream(stream);
	if (type == InheritType::Invalid)
		return false;

	ret.type = static_cast<ObjectCreationData::Type>(type);

	uint32_t strLen = 0;
	stream >> strLen;

	ret.name.resize(strLen);
	stream.Read(ret.name.data(), strLen);

	stream >> ret.position.x >> ret.position.y >> ret.position.z;
	stream >> ret.scale.x >> ret.scale.y >> ret.scale.z;
	stream >> ret.rotation.w >> ret.rotation.x >> ret.rotation.y >> ret.rotation.z;

	ret.unknownData = std::vector<char>(stream.data.begin() + stream.GetOffset(), stream.data.end());
	return true;
}

Object::InheritType Object::GetInheritTypeFromStream(const BinarySpan& stream)
{
	int intermediary = 0;
	stream >> intermediary;

	return intermediary >= 0 && intermediary < static_cast<int>(InheritType::TypeCount) ? static_cast<InheritType>(intermediary) : InheritType::Invalid;
}

void Object::SerializeSelf(BinaryStream& stream) const
{

}

void Object::DeserializeSelf(const BinarySpan& stream)
{

}

Object::~Object()
{
	for (Object* obj : children)
		delete obj;
}