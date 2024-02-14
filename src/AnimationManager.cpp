#include <vector>
#include <algorithm>

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "renderer/AnimationManager.h"
#include "renderer/ShaderReflector.h"
#include "renderer/Renderer.h"
#include "renderer/Vulkan.h"

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

Animation::Animation(const aiAnimation* animation, const aiNode* root, const MeshCreationData& mesh) : duration(animation->mDuration), ticksPerSecond(animation->mTicksPerSecond), time(0), transforms(animation->mNumChannels, glm::mat4(1.0f))
{
	ReadHierarchy(this->root, root);
	boneInfo = mesh.boneInfoMap;
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
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computeLayout, 0, 1, &descriptorSet, 0, nullptr);
	vkCmdDispatch(commandBuffer, Renderer::g_defaultVertexBuffer.GetSize() / 16 + 1, 1, 1);

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
	logicalDevice = context.logicalDevice;
	computeQueue = context.computeQueue;

	std::vector<char> code = ReadFile("shaders/spirv/anim.comp.spv");
	ShaderGroupReflector reflector({ code });

	// descriptor set

	std::vector<VkDescriptorPoolSize> descriptorPoolSizes = reflector.GetDescriptorPoolSize();

	VkDescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	poolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	poolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
	poolCreateInfo.maxSets = 1;

	VkResult result = vkCreateDescriptorPool(logicalDevice, &poolCreateInfo, nullptr, &descriptorPool);
	CheckVulkanResult("Failed to create a descriptor pool", result, vkCreateDescriptorPool);

	// set layout and pipeline

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = reflector.GetLayoutBindingsOfSet(0);

	std::vector<VkDescriptorBindingFlags> setBindingFlags(setLayoutBindings.size(), VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
	VkDescriptorSetLayoutBindingFlagsCreateInfoEXT setBindingFlagsCreateInfo{};
	setBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
	setBindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(setBindingFlags.size());
	setBindingFlagsCreateInfo.pBindingFlags = setBindingFlags.data();

	VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo{};
	setLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	setLayoutCreateInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
	setLayoutCreateInfo.pBindings = setLayoutBindings.data();
	setLayoutCreateInfo.pNext = &setBindingFlagsCreateInfo;

	result = vkCreateDescriptorSetLayout(logicalDevice, &setLayoutCreateInfo, nullptr, &computeSetLayout);
	CheckVulkanResult("Failed to create the descriptor set layout", result, vkCreateDescriptorSetLayout);

	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.pSetLayouts = &computeSetLayout;
	allocateInfo.descriptorPool = descriptorPool;
	allocateInfo.descriptorSetCount = 1;

	result = vkAllocateDescriptorSets(logicalDevice, &allocateInfo, &descriptorSet);
	CheckVulkanResult("Failed to allocate a descriptor set", result, vkAllocateDescriptorSets);
	
	VkPipelineLayoutCreateInfo layoutCreateInfo{};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.pSetLayouts = &computeSetLayout;
	layoutCreateInfo.setLayoutCount = 1;

	result = vkCreatePipelineLayout(logicalDevice, &layoutCreateInfo, nullptr, &computeLayout);
	CheckVulkanResult("Failed to create a pipeline layout", result, vkCreatePipelineLayout);

	VkShaderModule computeModule = Vulkan::CreateShaderModule(code);

	VkPipelineShaderStageCreateInfo stageCreateInfo{};
	stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageCreateInfo.module = computeModule;
	stageCreateInfo.pName = "main";

	VkComputePipelineCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	createInfo.layout = computeLayout;
	createInfo.stage = stageCreateInfo;

	result = vkCreateComputePipelines(logicalDevice, VK_NULL_HANDLE, 1, &createInfo, nullptr, &computePipeline);

	vkDestroyShaderModule(logicalDevice, computeModule, nullptr);

	Vulkan::CreateBuffer(500 * sizeof(glm::mat4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, mat4Buffer, mat4Memory);
	vkMapMemory(logicalDevice, mat4Memory, 0, VK_WHOLE_SIZE, 0, (void**)&mat4BufferPtr); // should not permanently map this large of a buffer

	reflector.WriteToDescriptorSet(logicalDevice, descriptorSet, Renderer::g_indexBuffer.GetBufferHandle(), 0, 0);
	reflector.WriteToDescriptorSet(logicalDevice, descriptorSet, Renderer::g_vertexBuffer.GetBufferHandle(), 0, 1);
	reflector.WriteToDescriptorSet(logicalDevice, descriptorSet, Renderer::g_defaultVertexBuffer.GetBufferHandle(), 0, 2);
	reflector.WriteToDescriptorSet(logicalDevice, descriptorSet, mat4Buffer, 0, 3);
}

void AnimationManager::Destroy()
{
	vkUnmapMemory(logicalDevice, mat4Memory);
	vkFreeMemory(logicalDevice, mat4Memory, nullptr);
	vkDestroyBuffer(logicalDevice, mat4Buffer, nullptr);

	vkDestroyPipelineLayout(logicalDevice, computeLayout, nullptr);
	vkDestroyPipeline(logicalDevice, computePipeline, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, computeSetLayout, nullptr);
	vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
}