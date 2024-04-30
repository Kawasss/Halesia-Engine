#pragma once
#include <string>
#include <cstdint>

class Object;
class Scene;

typedef void* HANDLE;

class FileStreamer
{
public:
	FileStreamer(std::string path);
	~FileStreamer();

	void CheckStatus();

	virtual void OnChange() {}

protected:
	std::string path;

private:
	void GetTime();
	void WaitForAccess();

	uint64_t time = 0;
	HANDLE handle = 0;
};

// ObjectStreamer can used to stream for example object files, so that changes made to that file are also made to the data in the engine
// it will the object into the given scene, but will not take ownership of it
class ObjectStreamer : FileStreamer
{
public:
	ObjectStreamer() = default;
	ObjectStreamer(Scene* scene, std::string path);

	void Poll(); // Poll() checks the file itself to see if it has changed (and will update the object accordingly)

	Object* GetObjectPtr() { return obj; }

private:
	void OnChange() override;

	Object* obj = nullptr;
};