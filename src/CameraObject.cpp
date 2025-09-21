#include <Windows.h>

#include "core/CameraObject.h"

#include "HalesiaEngine.h"

void CameraObject::Start()
{

}

void CameraObject::Update(float delta)
{
	prevView = view;
	prevProj = proj;

	CalculateView();
	CalculateProjection();
}

glm::mat4 CameraObject::GetViewMatrix() const
{
	return view;
}

glm::mat4 CameraObject::GetProjectionMatrix() const
{
	return proj;
}

glm::mat4 CameraObject::GetPreviousViewMatrix() const
{
	return prevView;
}

glm::mat4 CameraObject::GetPreviousProjectionMatrix() const
{
	return prevProj;
}

void CameraObject::CalculateView()
{
	view = glm::lookAt(transform.position, transform.position + transform.GetForward(), transform.GetUp());
}

void CameraObject::CalculateProjection()
{
	Renderer* pRenderer = HalesiaEngine::GetInstance()->GetEngineCore().renderer;

	float width  = pRenderer->GetInternalWidth();
	float height = pRenderer->GetInternalHeight();

	proj = glm::perspective(width / height, aspectRatio, nearPlane, farPlane);
	proj[1][1] *= -1.0f;
}