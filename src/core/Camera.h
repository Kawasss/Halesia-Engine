#pragma once
#include "../glm.h"
#include "../system/Window.h"

class Camera
{
public:
	Camera(glm::vec3 position = glm::vec3(0), float aspectRatio = monitorWidth / (float)monitorHeight);

	float cameraSpeed = 5;
	float pitch = 0, yaw = -(glm::pi<float>() / 2), fov = glm::pi<float>() / 2;

	glm::vec3 position	= glm::vec3(0);
	glm::vec3 up		= glm::vec3(0, 1, 0);
	glm::vec3 right		= glm::vec3(1, 0, 0);
	glm::vec3 front		= glm::vec3(1, 0, 0);

	void SetNearPlane(float newNearPlane);
	void SetFarPlane(float newFarPlane);

	void SetPitch(float newPitch);
	void SetYaw(float newYaw);
	void SetFOV(float newFov);
	void SetAspectRatio(float val) { aspectRatio = val; }
		 
	virtual void Start() {}
	virtual void Destroy() {}

	virtual void Update(Window* window, float delta);
	void DefaultUpdate(Window* window, float delta);

	glm::mat4 GetProjectionMatrix() const;
	glm::mat4 GetOrthoProjectionMatrix(unsigned int width, unsigned int height) const;
	glm::mat4 GetViewMatrix() const;
	glm::vec2 GetMotionVector();

	template<typename T> T* GetScript() const;
	
	/// <summary>
	/// This function sets the script for the base class. This makes it so that GetScript can be used.
	/// </summary>
	/// <param name="script"></param>
	template<typename T> void SetScript(T* script);

private:
	glm::vec2 prev2D = glm::vec2(0);
	void* attachedScript = nullptr;
	
	float nearPlane = 0.01f, farPlane = 1000, aspectRatio = monitorWidth / (float)monitorHeight;

protected:
	void UpdateVectors();

	/// <summary>
	/// Updates the up and right vector based on the current front vector
	/// </summary>
	void UpdateUpAndRightVectors();

	const float SENSITIVITY = 0.1f;
};

class OrbitCamera : public Camera
{
public:
	glm::vec3 pivot = glm::vec3(0);
	float radius	= 2;

	OrbitCamera() { SetScript(this); }
	void Update(Window* window, float delta) override;

private:
	float sumX = 0, sumY = 0;
};

template<typename T> void Camera::SetScript(T* script) 
{ 
	static_assert(!std::is_base_of_v<T, Camera>, "Cannot set the camera script: the given typename does not have Camera as a base");
	attachedScript = script; 
}

template<typename T> T* Camera::GetScript() const
{ 
	static_assert(!std::is_base_of_v<T, Camera>, "Cannot set the camera script: the given typename does not have Camera as a base or the pointer is null");
	return static_cast<T*>(attachedScript); 
}