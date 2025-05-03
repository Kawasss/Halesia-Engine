#include <Windows.h>
#include <stdexcept>

#include "core/ObjectStreamer.h"
#include "core/Object.h"
#include "core/Scene.h"

#include "io/SceneLoader.h"

FileStreamer::FileStreamer(std::string path) : path(path)
{
	handle = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	GetTime();
}

FileStreamer::~FileStreamer()
{
	if (handle) CloseHandle(handle);
}

void FileStreamer::CheckStatus()
{
	uint64_t oldTime = time;
	GetTime();

	if (time != oldTime)
	{
		WaitForAccess();
		OnChange();
	}
		
}

void FileStreamer::WaitForAccess()
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

void FileStreamer::GetTime()
{
	static_assert(sizeof(time) == sizeof(FILETIME));
	if (!GetFileTime(handle, NULL, NULL, (FILETIME*)&time)) // dangerous cast, hence the static_assert the line before
		throw std::runtime_error("ObjectStreamer::Poll(): Failed to get file time for file " + path + " (" + std::to_string(GetLastError()) + ')');
}

ObjectStreamer::ObjectStreamer(Scene* scene, std::string path) : FileStreamer(path)
{
	ObjectCreationData creationData = GenericLoader::LoadObjectFile(path);
	obj = scene->AddObject(creationData); // only static objects!
}

void ObjectStreamer::Poll()
{
	CheckStatus();
}

void ObjectStreamer::OnChange()
{
	ObjectCreationData data = GenericLoader::LoadObjectFile(path);
	obj->Initialize(data); // memory leak..?
}