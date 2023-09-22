#pragma once
#include "../Scene.h"
#include "../Console.h"

Camera* cameraHeldInStatis = nullptr; // it would be better to make a map to hold a scnene and camera, so that its possible to inject into multiple scenes at once
void InjectCamera(Scene* scene, Camera* camera) // if culling is added make it so that the culling is still done via the normal camera
{
	if (!scene->HasFinishedLoading())
	{
		Console::WriteLine("Failed to inject a camera, the scene hasn't fininsed loading", MESSAGE_SEVERITY_ERROR);
		return;
	}
	camera->position = scene->camera->position;
	camera->pitch = scene->camera->pitch;
	camera->yaw = scene->camera->yaw;

	cameraHeldInStatis = scene->camera;
	scene->camera = camera;
}

bool CameraIsInjected(Scene* scene)
{
	return !(scene->camera == cameraHeldInStatis);
}

void EjectCamera(Scene* scene)
{
	scene->camera = cameraHeldInStatis;
}