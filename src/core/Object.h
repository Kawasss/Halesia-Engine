#pragma once
#include <string>
#include <vector>
#include <future>

#include "Transform.h"

#include "../system/CriticalSection.h"

class Scene;
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
inline extern std::string ObjectStateToString(ObjectState state);

class Object
{
public:
	enum class InheritType
	{
		Base = 0,
		Mesh = 1,
		Rigid3D = 2,
		Light = 3,
	};

	/// <summary>
	/// This wont pause the program while its loading, so async loaded objects must be checked with HasFinishedLoading before calling a function.
	/// Be weary of accessing members in the constructor, since they don't have to be loaded in. AwaitGeneration awaits the async thread.
	/// </summary>
	/// <param name="customClassInstancePointer">: A pointer to the custom class, "this" will work most of the time</param>
	/// <param name="creationData"></param>
	/// <param name="creationObject">: The objects needed to create the meshes</param>
	static Object* Create(const ObjectCreationData& creationData, void* customClassInstancePointer = nullptr);

	Object(InheritType type);

	// order of the destruction:
	// 1. superclass should over this destructor
	// 2. destroy children
	// 3. destroy any members
	virtual ~Object();

	virtual void Start()  {}
	virtual void Update(float delta) {}

	virtual void OnCollisionEnter(Object* object) {}
	virtual void OnCollisionStay(Object* object)  {}
	virtual void OnCollisionExit(Object* object)  {}

	bool HasFinishedLoading();
	bool HasScript() const { return scriptClass != nullptr; }

	void AwaitGeneration(); // Awaits the async generation process of the object and meshes

	template<typename T> 
	T* GetScript(); // Gets the script attached to the object, if no script is attached it will return an invalid pointer

	void Initialize(const ObjectCreationData& creationData, void* customClassInstancePointer = nullptr);

	template<typename T> 
	Object* AddChild(const ObjectCreationData& creationData);
	Object* AddChild(const ObjectCreationData& creationData);
	void    AddChild(Object* object);

	void RemoveChild(Object* child); // this removes the child from this objects children
	void DeleteChild(Object* child); // this does the same as RemoveChild, but also deletes the object

	void TransferChild(Object* child, Object* destination); // this removes the child from this objects children and adds to the destinations children

	static void Duplicate(Object* oldObjPtr, Object* newObjPtr, std::string name, void* script);

	void SetParentScene(Scene* parent) { scene = parent; }

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

private:
	template<typename T> void SetScript(T* script);

	InheritType type = InheritType::Base;

	void* scriptClass = nullptr;
	std::future<void> generation;
	
	Scene* scene = nullptr;
	win32::CriticalSection critSection;
	std::vector<Object*> children;

	bool finishedLoading = false;
	bool shouldBeDestroyed = false;

protected:
	Object* parent = nullptr;

	static void Free(Object* objPtr)
	{
		objPtr->shouldBeDestroyed = true;
	}

	void Free() { shouldBeDestroyed = true; }

	Scene* GetParentScene() { return scene; }
};

template<typename T> 
void Object::SetScript(T* script)
{
	static_assert(!std::is_base_of_v<T, Object>, "Cannot set the script: the typename does not have Object as a base");
	scriptClass = script;
}

template<typename T> 
T* Object::GetScript() 
{ 
	static_assert(!std::is_base_of_v<T, Object>, "Cannot get the script: the typename does not have Object as a base");
	return static_cast<T*>(scriptClass); 
}

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