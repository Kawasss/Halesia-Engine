#pragma comment(lib, "rpcrt4.lib")
#include <rpc.h>
#include <winerror.h>
#include <stdexcept>
#include "ResourceManager.h"

Handle ResourceManager::GenerateHandle()
{
	static UUID uuid;
	if (HRESULT_FROM_WIN32(UuidCreate(&uuid)) != S_OK)
		throw std::runtime_error("Failed to create a UUID for its parent object");

	Handle ret;
	memcpy(&ret, &uuid, sizeof(ret));
	return ret;
}