#pragma once
#include <string>
#include <vector>
#include <future>
#include <mutex>

#include "Transform.h"

#include "renderer/Mesh.h"

#include "physics/RigidBody.h"
#include "physics/Shapes.h"

class Scene;
struct ObjectCreationData;
typedef uint64_t Handle;

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
	/// <summary>
	/// This wont pause the program while its loading, so async loaded objects must be checked with HasFinishedLoading before calling a function.
	/// Be weary of accessing members in the constructor, since they don't have to be loaded in. AwaitGeneration awaits the async thread.
	/// </summary>
	/// <param name="customClassInstancePointer">: A pointer to the custom class, "this" will work most of the time</param>
	/// <param name="creationData"></param>
	/// <param name="creationObject">: The objects needed to create the meshes</param>
	static Object* Create(const ObjectCreationData& creationData, void* customClassInstancePointer = nullptr);

	Object() = default;
	virtual		~Object() {}
	virtual void Start()  {}
	virtual void Destroy(bool del = true);
	virtual void Update(float delta);

	virtual void OnCollisionEnter(Object* object) {}
	virtual void OnCollisionStay(Object* object)  {}
	virtual void OnCollisionExit(Object* object)  {}

	bool HasFinishedLoading();
	bool HasScript()    { return scriptClass != nullptr; }
	bool HasRigidBody() { return rigid.type != RIGID_BODY_NONE; }

	/// <summary>
	/// Awaits the async generation process of the object and meshes
	/// </summary>
	void AwaitGeneration();

	/// <summary>
	/// Gets the script attached to the object, if no script is attached it will return an invalid pointer
	/// </summary>
	/// <typeparam name="T">: The name of the script's class as a pointer</typeparam>
	/// <returns>Pointer to the given class</returns>
	template<typename T> T* GetScript();
	template<typename T> Object* AddCustomChild(const ObjectCreationData& creationData);

	void Initialize(const ObjectCreationData& creationData, void* customClassInstancePointer = nullptr);

	void SetRigidBody(RigidBodyType type, Shape shape);
	void SetMesh(const std::vector<MeshCreationData>& creationData);
	Object* AddChild(const ObjectCreationData& creationData);
	void AddChild(Object* object);
	void RemoveChild(Object* child); // this removes the child from this objects children
	void DeleteChild(Object* child); // this does the same as RemoveChild, but also deletes the object
	void TransferChild(Object* child, Object* destination); // this removes the child from this objects children and adds to the destinations children

	static void Duplicate(Object* oldObjPtr, Object* newObjPtr, std::string name, void* script);

	Scene* scene;

	Transform transform;
	RigidBody rigid;
	Mesh mesh;
	std::vector<Object*> children;
	ObjectState state = OBJECT_STATE_VISIBLE;
	std::string name;
	std::mutex mutex;
	Handle handle;
	bool finishedLoading = false;
	bool shouldBeDestroyed = false;

private:
	template<typename T> void SetScript(T* script);
	void GenerateObjectWithData(const ObjectCreationData& creationData);

	void* scriptClass = nullptr;
	std::future<void> generation;
	
protected:
	Object* parent = nullptr;

	static void Free(Object* objPtr)
	{
		objPtr->shouldBeDestroyed = true;
	}
};

template<typename T> 
void Object::SetScript(T* script)
{
	static_assert(!std::is_base_of_v<T, Object>, "Cannot set the script: the typename does not have Object as a base or the pointer is null");
	scriptClass = script;
}

template<typename T> 
T* Object::GetScript() 
{ 
	static_assert(!std::is_base_of_v<T, Object>, "Cannot get the script: the typename does not have Object as a base or the pointer is null");
	return static_cast<T*>(scriptClass); 
}

template<typename T> 
Object* Object::AddCustomChild(const ObjectCreationData& creationData)
{
	static_assert(!std::is_base_of_v<T, Object>, "Cannot Create a custom object: the typename does not have Object as a base");
	T* custom = new T();
	Object* base = custom;
	base->Initialize(creationData, custom);
	children.push_back(base);
	return base;
}