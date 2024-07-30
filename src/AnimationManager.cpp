#include <vector>
#include <algorithm>

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "renderer/AnimationManager.h"
#include "renderer/ShaderReflector.h"
#include "renderer/Renderer.h"
#include "renderer/Vulkan.h"
#include "renderer/ComputeShader.h"

#include "io/SceneLoader.h"

#include "tools/common.h"

inline glm::mat4 GetMat4(const aiMatrix4x4& from)
{
	glm::mat4 to{};
	//the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
	to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
	to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
	to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
	to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
	return to;
}

Animation::Animation(const aiAnimation* animation, const aiNode* root, std::map<std::string, BoneInfo> boneInfoMap) : duration(animation->mDuration), ticksPerSecond(animation->mTicksPerSecond), time(0), transforms(animation->mNumChannels, glm::mat4(1.0f)), boneInfo(boneInfoMap)
{
	ReadHierarchy(this->root, root);
	ReadBones(animation);
}

void Animation::ReadHierarchy(HierarchyNode& node, const aiNode* source)
{
	node.name = source->mName.C_Str();
	node.transformation = GetMat4(source->mTransformation);
	for (int i = 0; i < source->mNumChildren; i++)
	{
		HierarchyNode child{};
		ReadHierarchy(child, source->mChildren[i]);
		node.children.push_back(child);
	}
}

void Animation::ReadBones(const aiAnimation* animation)
{
	for (int i = 0; i < animation->mNumChannels; i++)
		bones.push_back(Bone(animation->mChannels[i]));
}

Bone* Animation::GetBone(std::string name)
{
	std::vector<Bone>::iterator index = std::find_if
	(
		bones.begin(), bones.end(),
		[&](const Bone& bone)
		{
			return bone.GetName() == name;
		}
	);
	if (index == bones.end()) return nullptr;
	return &(*index);
}

void Animation::ComputeTransform(const HierarchyNode* node, glm::mat4 parentTransform)
{
	Bone* bone = GetBone(node->name);
	glm::mat4 transform = node->transformation;
	if (bone != nullptr)
	{
		bone->Update(time);
		transform = bone->GetTransform();
	}

	glm::mat4 globalTrans = parentTransform * transform;

	if (boneInfo.find(node->name) != boneInfo.end())
	{
		BoneInfo& info = boneInfo[node->name];
		transforms[info.index] = globalTrans * info.offset;
		if (transforms[info.index] == glm::mat4(0.0f))
			transforms[info.index] = glm::mat4(1.0f);
	}

	for (int i = 0; i < node->children.size(); i++)
		ComputeTransform(&node->children[i], globalTrans);
}

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

std::vector<glm::mat4> Animation::GetTransforms()
{
	return transforms;
}

void AnimationManager::Create()
{
	CreateShader();
}

void AnimationManager::AddAnimation(Animation* animation)
{
	animations.push_back(animation);
}

void AnimationManager::RemoveAnimation(Animation* animation)
{
	std::vector<Animation*>::iterator iter = std::find_if
	(
		animations.begin(), animations.end(),
		[&](Animation* ptr)
		{
			return animation == ptr;
		}
	);
	if (iter != animations.end()) animations.erase(iter);
}

void AnimationManager::ComputeAnimations(float delta)
{
	int offset = 0;
	for (int i = 0; i < animations.size(); i++) // should be multithreaded
	{
		animations[i]->Update(delta);
		std::vector<glm::mat4> mats = animations[i]->GetTransforms();
		memcpy(mat4BufferPtr + offset, mats.data(), sizeof(glm::mat4) * mats.size());
		offset += mats.size();
	}
}

void AnimationManager::ApplyAnimations(VkCommandBuffer commandBuffer)
{
	if (disable)
	{
		VkBufferCopy copy{};
		copy.size = Renderer::g_defaultVertexBuffer.GetSize() * sizeof(Vertex);

		vkCmdCopyBuffer(commandBuffer, Renderer::g_defaultVertexBuffer.GetBufferHandle(), Renderer::g_vertexBuffer.GetBufferHandle(), 1, &copy);
		return;
	}

	computeShader->Execute(commandBuffer, Renderer::g_defaultVertexBuffer.GetSize() / 16 + 1, 1, 1);

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
	const Vulkan::Context& context = Vulkan::GetContext();

	mat4Buffer.Init(500 * sizeof(glm::mat4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	mat4BufferPtr = mat4Buffer.Map<glm::mat4>(); // should not permanently map this large of a buffer

	std::vector<glm::mat4> defaultValue(500, glm::mat4(1.0f));
	memcpy(mat4BufferPtr, defaultValue.data(), 500 * sizeof(glm::mat4));

	computeShader = new ComputeShader("shaders/spirv/anim.comp.spv");

	computeShader->WriteToDescriptorBuffer(Renderer::g_indexBuffer.GetBufferHandle(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, 0);
	computeShader->WriteToDescriptorBuffer(Renderer::g_vertexBuffer.GetBufferHandle(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, 1);
	computeShader->WriteToDescriptorBuffer(Renderer::g_defaultVertexBuffer.GetBufferHandle(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, 2);
	computeShader->WriteToDescriptorBuffer(mat4Buffer.Get(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, 3);
}

void AnimationManager::Destroy()
{
	const Vulkan::Context& context = Vulkan::GetContext();

	delete computeShader;

	mat4Buffer.Unmap();
}