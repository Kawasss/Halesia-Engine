#include <vector>
#include <algorithm>

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "renderer/AnimationManager.h"
#include "renderer/Renderer.h"
#include "renderer/ComputeShader.h"

AnimationManager* AnimationManager::Get()
{
	static AnimationManager* singleton = nullptr;
	if (singleton == nullptr)
	{
		singleton = new AnimationManager();
		singleton->Create();
	}
	return singleton;
}

void Animation::Update(float delta)
{
	time += delta / 1000 * ticksPerSecond;
	if (!loop && fmod(time, duration) < time)
		return;
	time = fmod(time, duration);
	ComputeTransform(&root, glm::mat4(1.0f));
}

void Animation::Reset()
{
	time = 0;
}

std::vector<glm::mat4>& Animation::GetTransforms()
{
	return transforms;
}

void AnimationManager::Create()
{
	CreateShader();
}

void AnimationManager::AddAnimation(const Animation& animation)
{
	win32::CriticalLockGuard guard(section);
	animations.push_back(animation);
}

//void AnimationManager::RemoveAnimation(Animation* animation)
//{
//	std::vector<Animation*>::iterator iter = std::find_if
//	(
//		animations.begin(), animations.end(),
//		[&](Animation* ptr)
//		{
//			return animation == ptr;
//		}
//	);
//	if (iter != animations.end()) animations.erase(iter);
//}

void AnimationManager::ComputeAnimations(float delta)
{
	win32::CriticalLockGuard guard(section);

	size_t offset = 0;
	for (int i = 1; i < animations.size() && i < 2; i++) // should be multithreaded
	{
		animations[i].Update(delta);
		std::vector<glm::mat4>& mats = animations[i].GetTransforms();
		memcpy(mat4BufferPtr + offset, mats.data(), sizeof(glm::mat4) * mats.size());
		offset += mats.size();
	}
}

void AnimationManager::ApplyAnimations(VkCommandBuffer commandBuffer)
{
	win32::CriticalLockGuard guard(section);
	if (disable)
	{
		VkBufferCopy copy{};
		copy.size = Renderer::g_defaultVertexBuffer.GetSize() * sizeof(Vertex);

		vkCmdCopyBuffer(commandBuffer, Renderer::g_defaultVertexBuffer.GetBufferHandle(), Renderer::g_vertexBuffer.GetBufferHandle(), 1, &copy);
		return;
	}

	computeShader->Execute(commandBuffer, static_cast<uint32_t>(Renderer::g_defaultVertexBuffer.GetSize() / 16) + 1, 1, 1);

	VkBufferMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.buffer = Renderer::g_vertexBuffer.GetBufferHandle();
	barrier.size = VK_WHOLE_SIZE;
	
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void AnimationManager::CreateShader()
{
	win32::CriticalLockGuard guard(section);
	mat4Buffer.Init(500 * sizeof(glm::mat4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	mat4BufferPtr = mat4Buffer.Map<glm::mat4>(); // should not permanently map this large of a buffer

	std::vector<glm::mat4> defaultValue(500, glm::mat4(1.0f));
	memcpy(mat4BufferPtr, defaultValue.data(), 500 * sizeof(glm::mat4));

	computeShader = std::make_unique<ComputeShader>("shaders/uncompiled/anim.comp");

	computeShader->BindBufferToName("indexBuffer", Renderer::g_indexBuffer.GetBufferHandle());
	computeShader->BindBufferToName("vertexBuffer", Renderer::g_vertexBuffer.GetBufferHandle());
	computeShader->BindBufferToName("sourceBuffer", Renderer::g_defaultVertexBuffer.GetBufferHandle());
	computeShader->BindBufferToName("animMatrices", mat4Buffer.Get());
}

void AnimationManager::AcquireLock()
{
	section.Lock();
}

void AnimationManager::ReleaseLock()
{
	section.Unlock();
}