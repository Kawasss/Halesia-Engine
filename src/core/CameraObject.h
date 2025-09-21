#pragma once
#include "Object.h"

#include "../glm.h"

class CameraObject : public Object
{
public:
	static CameraObject* Create(const ObjectCreationData& data);
	static CameraObject* Create();

	void Start() override;
	void Update(float delta) override;

	glm::mat4 GetViewMatrix() const;
	glm::mat4 GetProjectionMatrix() const;

	glm::mat4 GetPreviousViewMatrix() const;
	glm::mat4 GetPreviousProjectionMatrix() const;

	float zNear = 0.01f, zFar = 1000;

private:
	void CalculateView();
	void CalculateProjection();

	glm::mat4 view, prevView;
	glm::mat4 proj, prevProj;

	float aspectRatio = 0.0f;
};