#pragma once
#include <string_view>
#include <string>
#include <vector>
#include <future>

#include "Transform.h"

#include "../system/CriticalSection.h"

class Scene;
class BinaryStream;
class BinarySpan;
struct ObjectCreationData;
using Handle = uint64_t;

enum ObjectState : uint8_t
{
		/// <summary>
		/// The attached script is run and the object is rendered
		/// </summary>
		OBJECT_STATE_VISIBLE,
		/// <summary>
		/// The attached script is run, but the object isn't rendered
		/// </summary>
		OBJECT_STATE_INVISIBLE,
		/// <summary>
		/// The attached script isn't run and the object isn't rendered
		/// </summary>
		OBJECT_STATE_DISABLED
	};
inline extern std::string_view ObjectStateToString(ObjectState state);
inline extern ObjectState ObjectStateFromString(const std::string_view& str);

class Object
{
public:
	enum class InheritType
	{
		Base = 0,
		Mesh = 1,
		Rigid3D = 2,
		Light = 3,
		Script = 4,
		Camera = 5,
		TypeCount = 6, // increment when a new type is added
		Invalid = TypeCount + 1,
	};
	static std::string_view InheritTypeToString(InheritType type);
	static InheritType StringToInheritType(const std::string_view& str);

	/// <summary>
	/// This wont pause the program while its loading, so async loaded objects must be checked with HasFinishedLoading before calling a function.
	/// Be wary of accessing members in the constructor, since they don't have to be loaded in. AwaitGeneration awaits the async thread.
	/// </summary>
	/// <param name="creationData"></param>
	/// <param name="creationObject">: The objects needed to create the meshes</param>
	static Object* Create(const ObjectCreationData& creationData);

	Object(InheritType type);

	// order of the destruction:
	// 1. superclass should over this destructor
	// 2. destroy children
	// 3. destroy any members
	virtual ~Object();

	virtual void Start()  {}

	/// <summary>
	/// only update this object, ignores its children
	/// </summary>
	/// <param name="delta">time passed since last update</param>
	void ShallowUpdate(float delta);

	virtual void OnCollisionEnter(Object* object) {}
	virtual void OnCollisionStay(Object* object)  {}
	virtual void OnCollisionExit(Object* object)  {}

	/// <summary>
	/// update this object and its children
	/// </summary>
	/// /// <param name="delta">time passed since last update</param>
	void FullUpdate(float delta);

	/// <summary>
	/// Update the transform matrices of this object and its children
	/// </summary>
	void FullUpdateTransform();

	/// <summary>
	/// update the transform matrices of only this object, not its children
	/// </summary>
	void ShallowUpdateTransform();

	bool HasFinishedLoading();
	bool HasScript() const { return hasScript; }

	void AwaitGeneration(); // Awaits the async generation process of the object and meshes

	void Initialize(const ObjectCreationData& creationData);

	template<typename T> 
	Object* AddChild(const ObjectCreationData& creationData);
	Object* AddChild(const ObjectCreationData& creationData);
	void    AddChild(Object* object);

	void RemoveChild(Object* child); // this removes the child from this objects children
	void DeleteChild(Object* child); // this does the same as RemoveChild, but also deletes the object

	void TransferChild(Object* child, Object* destination); // this removes the child from this objects children and adds to the destinations children

	Object* CreateShallowCopy() const; // creates a copy of the object and assigns it to the same parent, but does not copy its children (also registers it to the scene)

	void SetParentScene(Scene* parent) { scene = parent; }

	std::vector<char> Serialize() const; // when serializing, the children of an object will be serialized by the object itself.
	void Deserialize(const BinarySpan& stream); // assumes that the object is already the correct type, also assumes that the inheritType at the beginning of the stream is already read (a.k.a. 'GetInheritTypeFromStream(...)' is already called)

	static bool DeserializeIntoCreationData(const BinarySpan& stream, ObjectCreationData& ret);

	static InheritType GetInheritTypeFromStream(const BinarySpan& stream); // this function should only be called when deserializing an object and the inherit type is needed to create the correct object

	std::vector<Object*>& GetChildren() { return children; }

	const std::vector<Object*>& GetChildren() const  { return children;    }
	win32::CriticalSection&     GetCriticalSection() { return critSection; }
	
	Transform transform;
	
	ObjectState state = OBJECT_STATE_VISIBLE;
	std::string name;
	Handle handle = 0;

	bool FinishedLoading()   const { return finishedLoading;   }
	bool ShouldBeDestroyed() const { return shouldBeDestroyed; }

	bool HasChildren() const { return !children.empty(); }

	InheritType GetType() const { return type; }
	bool IsType(InheritType cmp) const;

	Object* GetParent() const { return parent; }

	static void Free(Object* objPtr)
	{
		objPtr->shouldBeDestroyed = true;
	}

private:
	template<typename T> void SetScript(T* script);

	void SerializeHeader(BinaryStream& stream) const;
	void SerializeName(BinaryStream& stream) const;
	void SerializeTransform(BinaryStream& stream) const;

	void SerializeChildren(BinaryStream& stream) const;

	void DeserializeName(const BinarySpan& stream);
	void DeserializeTransform(const BinarySpan& stream);

	void SerializeIntoStream(BinaryStream& stream) const;

	InheritType type = InheritType::Base;

	std::future<void> generation;
	
	Scene* scene = nullptr;
	win32::CriticalSection critSection;
	std::vector<Object*> children;

	bool finishedLoading = true;
	bool shouldBeDestroyed = false;

protected:
	Object* parent = nullptr;

	virtual void Update(float delta) {}

	/// <summary>
	/// instance must create a copy of all of its data into pObject. Implementations can assume that pObject is the same superclass is the instance.
	/// base class data is copied seperately.
	/// </summary>
	/// <param name="pObject"></param>
	virtual void DuplicateDataTo(Object* pObject) const;

	/// <summary>
	/// duplicate all data of the base to pObject
	/// </summary>
	/// <param name="pObject"></param>
	void DuplicateBaseDataTo(Object* pObject) const;

	// base object data is already (de)serialized before this call is made
	virtual void SerializeSelf(BinaryStream& stream) const;
	virtual void DeserializeSelf(const BinarySpan& stream);

	Scene* GetParentScene() { return scene; }

	bool hasScript = false;
};

template<typename T> 
Object* Object::AddChild(const ObjectCreationData& creationData)
{
	static_assert(!std::is_base_of_v<T, Object>, "Cannot Create a custom object: the typename does not have Object as a base");
	T* custom = new T();
	Object* base = custom;
	base->Initialize(creationData, custom);
	children.push_back(base);
	return base;
}