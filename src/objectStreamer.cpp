#include <Windows.h>
#include <stdexcept>

#include "core/ObjectStreamer.h"
#include "core/Object.h"
#include "core/Scene.h"

#include "io/SceneLoader.h"

ObjectStreamer::ObjectStreamer(Scene* scene, std::string path) : path(path)
{
	file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	ObjectCreationData creationData = GenericLoader::LoadObjectFile(path);
	obj = scene->AddStaticObject(creationData); // only static objects!

	if (!GetFileTime(file, NULL, NULL, (FILETIME*)&time)) // dangerous cast
		throw std::runtime_error("ObjectStreamer::Poll(): Failed to get file time for file " + path + " (" + std::to_string(GetLastError()) + ')');
}

ObjectStreamer::~ObjectStreamer()
{
	if (file) CloseHandle(file);
}

void ObjectStreamer::Poll()
{
	uint64_t oldTime = time;
	if (!GetFileTime(file, NULL, NULL, (FILETIME*)&time)) // dangerous cast
		throw std::runtime_error("ObjectStreamer::Poll(): Failed to get file time for file " + path + " (" + std::to_string(GetLastError()) + ')');

	if (time == oldTime)
		return;

	WaitForFileAccess();
	ObjectCreationData data = GenericLoader::LoadObjectFile(path);
	obj->Destroy(false);
	obj->Initialize(data);
}

ObjectStreamer::ObjectStreamer(ObjectStreamer& rhs)
{
	file = rhs.file;
	path = rhs.path;
	obj = rhs.obj;

	rhs.file = 0;

	Poll();
}

void ObjectStreamer::WaitForFileAccess()
{
	HANDLE temp = 0;
	while (temp = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0)) // seems wasteful??
	{
		
		if (GetLastError() == ERROR_SHARING_VIOLATION)
			Sleep(5); // random time
		else break;
	}
	if (temp)
		CloseHandle(temp);
}