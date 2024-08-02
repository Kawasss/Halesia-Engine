#pragma once
#include <vector>
#include "../glm.h"

#include "RenderPipeline.h"
#include "Buffer.h"

class ComputeShader;
class Camera;
class GraphicsPipeline;

class ForwardPlusPipeline : public RenderPipeline
{
public:
	void Start(const Payload& payload) override;
	void Execute(const Payload& payload, const std::vector<Object*>& objects) override;
	void Destroy() override;

	~ForwardPlusPipeline() { Destroy(); }

	void AddLight(glm::vec3 pos) override;

	// these functions can be used if the default render pipeline functions dont cut it (Execute( ... ) will call these functions)
	void ComputeCells(VkCommandBuffer commandBuffer, Camera* camera);
	void DrawObjects(VkCommandBuffer commandBuffer, const std::vector<Object*>& objects, Camera* camera, uint32_t width, uint32_t height);

	ComputeShader* GetShader() { return computeShader; }
	VkBuffer GetCellBuffer()   { return cellBuffer.Get();  }
	VkBuffer GetLightBuffer()  { return lightBuffer.Get(); }

private:
	void Allocate();
	void CreateShader();
	void PrepareGraphicsPipeline();
	void UpdateUniformBuffer(Camera* cam, uint32_t width, uint32_t height);

	static constexpr int MAX_LIGHT_INDICES = 7;
	static constexpr int MAX_LIGHTS = 1024;

	// can't use floats here because GLSL / SPIRV padding for a float array is fucked up.
	//
	// GLSL pads this struct like this:
	// - lightCount:       4 bytes
	// - lightIndices[0]:  8 bytes
	// ...
	// - lightIndices[^2]: 8 bytes
	// - lightIndices[^1]: 4 bytes
	//
	// which I simply cannot achieve in normal C++, so I give up.
	// this does waste a lot of memory, because of the padding.
	struct Cell
	{
		alignas(4) float lightCount;
		char lightIndices[(MAX_LIGHT_INDICES - 1) * (sizeof(float) * 2) + sizeof(float)];
	};

	struct Matrices
	{
		glm::mat4 projection;
		glm::mat4 view;
	};

	uint32_t cellWidth = 64, cellHeight = 64;

	Buffer cellBuffer;
	Buffer lightBuffer;
	Buffer matricesBuffer;

	uint32_t lightCount = 0;

	Matrices* matrices = nullptr;
	ComputeShader* computeShader = nullptr;
	GraphicsPipeline* graphicsPipeline = nullptr;

	struct UniformBufferObject
	{
		glm::vec3 cameraPos;

		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 projection;
		uint32_t width;
		uint32_t height;
	};

	struct ModelData
	{
		glm::mat4 transformation;
		glm::vec4 IDColor;
	};

	Buffer               uniformBuffer;
	UniformBufferObject* uniformBufferMapped;

	Buffer     modelBuffer;
	ModelData* modelBufferMapped;
};