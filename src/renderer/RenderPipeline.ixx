module;

#include <Windows.h>
#include <vulkan/vulkan.h>

#include "CommandBuffer.h"

#include "../core/MeshObject.h"

export module Renderer.RenderPipeline;

import std;

import Core.CameraObject;

import System.Window;

import Renderer.Framebuffer;

export enum class RenderMode : int // this enum is used as a suggestion
{
	DontCare = 0,
	Albedo = 1,
	Normal = 2,
	Metallic = 3,
	Roughness = 4,
	AmbientOcclusion = 5,
	Polygon = 6,
	UV = 7,
	GlobalIllumination = 8,
	ModeCount = 9, // this value is used for reflection / iteration and should never be used in code
};

export std::string_view RenderModeToString(RenderMode mode);

export class RenderPipeline
{
public:
	struct Payload
	{
		Payload(const CommandBuffer& cmdBuffer, Window* pWindow, CameraObject* pCamera, Framebuffer& framebuffer, uint32_t w, uint32_t h) : commandBuffer(cmdBuffer), window(pWindow), camera(pCamera), presentationFramebuffer(framebuffer), width(w), height(h) {}

		CommandBuffer commandBuffer;
		Window* window;
		CameraObject* camera;

		Framebuffer& presentationFramebuffer;

		uint32_t width;
		uint32_t height;
	};

	struct IntVariable
	{
		IntVariable() = default;
		IntVariable(const std::string_view& name, int* pValue) : name(name), pValue(pValue) {}

		std::string_view name;
		int* pValue = nullptr;

		bool IsValid() const { return pValue != nullptr; }
	};

	virtual void Start(const Payload& payload) = 0;
	virtual void Execute(const Payload& payload, const std::vector<MeshObject*>& objects) = 0;

	virtual ~RenderPipeline() {}

	virtual void Resize(const Payload& payload) {}

	virtual void SetRenderMode(RenderMode mode) { renderMode = mode; }

	virtual void OnRenderingBufferResize(const Payload& payload) {}

	virtual void ReloadShaders(const Payload& payload) = 0;

	virtual std::vector<IntVariable> GetIntVariables() { return {}; }

	template<typename T> T* GetChild() { return reinterpret_cast<T*>(this); }

	VkRenderPass renderPass3D = VK_NULL_HANDLE;
	VkRenderPass renderPass2D = VK_NULL_HANDLE;

	bool active = true;

private:

protected:
	RenderMode renderMode = RenderMode::DontCare;
};