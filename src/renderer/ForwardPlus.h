#pragma once
#include <vector>
#include <unordered_set>
#include <memory>

#include "../glm.h"

#include "CommandBuffer.h"
#include "RenderPipeline.h"
#include "Buffer.h"

class ComputeShader;
class Camera;
class GraphicsPipeline;
struct Light;

class ForwardPlusPipeline : public RenderPipeline
{
public:
	~ForwardPlusPipeline() = default;

	void Start(const Payload& payload) override;
	void Execute(const Payload& payload, const std::vector<MeshObject*>& objects) override;

	// these functions can be used if the default render pipeline functions dont cut it (Execute( ... ) will call these functions)
	void ComputeCells(CommandBuffer commandBuffer, uint32_t lightCount, Camera* camera);
	void DrawObjects(CommandBuffer commandBuffer, const std::vector<MeshObject*>& objects, Camera* camera, uint32_t width, uint32_t height, glm::mat4 customProj = glm::mat4(0));
	void UpdateBindlessTextures();

	ComputeShader* GetShader() { return computeShader.get(); }
	VkBuffer GetCellBuffer()   { return cellBuffer.Get();    }

private:
	void Allocate();
	void CreateShader();
	void PrepareGraphicsPipeline();
	void UpdateUniformBuffer(Camera* cam, glm::mat4 proj, uint32_t width, uint32_t height);

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
		char lightIndices[(MAX_LIGHT_INDICES - 1) * (sizeof(float) * 2) + sizeof(float) * 2];
	};

	struct Matrices
	{
		glm::mat4 projection;
		glm::mat4 view;
	};

	uint32_t cellWidth = 64, cellHeight = 64;

	Buffer cellBuffer;
	Buffer matricesBuffer;

	Matrices* matrices = nullptr;
	std::unique_ptr<ComputeShader> computeShader;
	std::unique_ptr<GraphicsPipeline> graphicsPipeline;

	struct UniformBufferObject
	{
		glm::vec3 cameraPos;

		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 projection;
		uint32_t width;
		uint32_t height;
	};

	Buffer               uniformBuffer;
	UniformBufferObject* uniformBufferMapped;

	std::vector<uint64_t> processedMats;
};