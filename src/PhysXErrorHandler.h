#pragma once
#include "PxPhysicsAPI.h"
#include <string>
#include <iostream>

using namespace physx;

class PhysXErrorHandler : public PxErrorCallback
{
public:
	void reportError(PxErrorCode::Enum errorCode, const char* message, const char* file, int line) override
	{
		std::string error = "PhysX error: " + (std::string)message + " in file " + (std::string)file + " at line " + std::to_string(line);
		std::cout << error << std::endl;
		throw std::runtime_error(error);
	}
};