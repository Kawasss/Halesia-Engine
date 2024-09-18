#pragma once

class Scene;
class Camera;

class CameraInjector
{
public:
	CameraInjector() = default;
	CameraInjector(Scene* scene, bool inheritCameraTransformProperties = true);
	~CameraInjector();

	void Inject(Camera* newCamera);
	void Eject();
	
	void SetScene(Scene* scene) { sceneToInjectInto = scene; }

	bool IsInjected() { return cameraHeldInStasis != nullptr; }

	Camera* GetStoredCamera();

private:
	Scene* sceneToInjectInto = nullptr;
	Camera* cameraHeldInStasis = nullptr;
	bool inheritTransformProperties = true;
};