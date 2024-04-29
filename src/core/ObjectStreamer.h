#pragma once
#include <string>
#include <cstdint>

class Object;
class Scene;

typedef void* HANDLE;

// ObjectStreamer can used to stream for example object files, so that changes made to that file are also made to the data in the engine
// it will the object into the given scene, but will not take ownership of it
class ObjectStreamer
{
public:
	ObjectStreamer() = default;
	ObjectStreamer(Scene* scene, std::string path);
	ObjectStreamer(ObjectStreamer& rhs);
	~ObjectStreamer();

	void Poll(); // Poll() checks the file itself to see if it has changed (and will update the object accordingly)

	Object* GetObjectPtr() { return obj; }

private:
	void WaitForFileAccess();

	HANDLE file = 0;
	uint64_t time = 0;

	Object* obj = nullptr;
	std::string path;
};