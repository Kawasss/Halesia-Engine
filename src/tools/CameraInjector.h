#pragma once
#include "../core/Scene.h"
#include "../core/Console.h"
#include "../core/Camera.h"

class CameraInjector
{
public:
	CameraInjector(Scene* scene = nullptr, bool inheritCameraTransformProperties = true)
	{
		sceneToInjectInto = scene;
		inheritTransformProperties = inheritCameraTransformProperties;
	}

	void Inject(Camera* newCamera = Scene::defaultCamera)
	{
		if (sceneToInjectInto == nullptr || newCamera == nullptr)
		{
			Console::WriteLine("Failed to inject a camera: the scene or camera is invalid.", MESSAGE_SEVERITY_ERROR);
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

	void Eject()
	{
		if (cameraHeldInStasis == nullptr)
		{
			Console::WriteLine("Failed to eject the camera: no camera has been injected.", MESSAGE_SEVERITY_ERROR);
			return;
		}
		sceneToInjectInto->camera = cameraHeldInStasis;
		cameraHeldInStasis = nullptr;
	}

	bool IsInjected()
	{
		return cameraHeldInStasis != nullptr;
	}

	Camera* GetStoredCamera()
	{
		if (cameraHeldInStasis == nullptr)
		{
			Console::WriteLine("The stored camera has been requested, but can't given since no camera has been injected, returning a nullptr.", MESSAGE_SEVERITY_ERROR);
			return nullptr;
		}

		return cameraHeldInStasis;
	}

	~CameraInjector()
	{
		Eject();
	}

private:
	Scene* sceneToInjectInto = nullptr;
	Camera* cameraHeldInStasis = nullptr;
	bool inheritTransformProperties = true;
};


