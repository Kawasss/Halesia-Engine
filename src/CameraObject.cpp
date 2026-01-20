module;

#include "glm.h"

module Core.CameraObject;

import HalesiaEngine;

import Renderer;

import Core.Object;

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

	float width  = static_cast<float>(pRenderer->GetInternalWidth());
	float height = static_cast<float>(pRenderer->GetInternalHeight());
	
	proj = glm::perspective(width / height, glm::radians(fov), zNear, zFar);
	proj[1][1] *= -1.0f;
}