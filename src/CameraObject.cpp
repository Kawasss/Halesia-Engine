#include "core/CameraObject.h"

#include "HalesiaEngine.h"

CameraObject* CameraObject::Create()
{
	return new CameraObject();
}

CameraObject::CameraObject() : Object(InheritType::Camera)
{

}

void CameraObject::Start()
{

}

void CameraObject::UpdateMatrices()
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
	glm::vec3 position = transform.GetGlobalPosition();
	view = glm::lookAt(position, position + transform.GetForward(), transform.GetUp());
}

void CameraObject::CalculateProjection()
{
	Renderer* pRenderer = HalesiaEngine::GetInstance()->GetEngineCore().renderer;

	float width  = pRenderer->GetInternalWidth();
	float height = pRenderer->GetInternalHeight();
	
	proj = glm::perspective(width / height, glm::radians(90.0f), zNear, zFar);
	proj[1][1] *= -1.0f;
}