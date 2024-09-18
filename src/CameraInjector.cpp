#include "tools/CameraInjector.h"

#include "core/Scene.h"
#include "core/Console.h"
#include "core/Camera.h"

CameraInjector::CameraInjector(Scene* scene, bool inheritCameraTransformProperties)
{
	sceneToInjectInto = scene;
	inheritTransformProperties = inheritCameraTransformProperties;
}

void CameraInjector::Inject(Camera* newCamera)
{
	if (sceneToInjectInto == nullptr || newCamera == nullptr)
	{
		Console::WriteLine("Failed to inject a camera: the scene or camera is invalid.", Console::Severity::Error);
		return;
	}
	if (inheritTransformProperties)
	{
		newCamera->position = sceneToInjectInto->camera->position;
		newCamera->pitch = sceneToInjectInto->camera->pitch;
		newCamera->yaw = sceneToInjectInto->camera->yaw;
	}

	cameraHeldInStasis = sceneToInjectInto->camera;
	sceneToInjectInto->camera = newCamera;
}

void CameraInjector::Eject()
{
	if (cameraHeldInStasis == nullptr)
	{
		Console::WriteLine("Failed to eject the camera: no camera has been injected.", Console::Severity::Error);
		return;
	}
	sceneToInjectInto->camera = cameraHeldInStasis;
	cameraHeldInStasis = nullptr;
}

Camera* CameraInjector::GetStoredCamera()
{
	if (cameraHeldInStasis == nullptr)
	{
		Console::WriteLine("The stored camera has been requested, but can't given since no camera has been injected, returning a nullptr.", Console::Severity::Error);
		return nullptr;
	}

	return cameraHeldInStasis;
}

CameraInjector::~CameraInjector()
{
	if (cameraHeldInStasis != nullptr)
		Eject();
}