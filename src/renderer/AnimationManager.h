#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <map>

#include "../glm.h"

#include "Bone.h"

struct aiAnimation;
struct aiNode;
struct MeshCreationData;
class  ComputeShader;

class Animation
{
public:
	Animation() = default;
	Animation(const aiAnimation* animation, const aiNode* root, std::map<std::string, BoneInfo> boneInfoMap);
	Bone* GetBone(std::string name);
	std::vector<glm::mat4> GetTransforms();

	void Update(float delta);
	void Reset();

	bool loop = true;

private:
	struct HierarchyNode
	{
		std::string name;
		glm::mat4 transformation = glm::mat4(1.0f);
		std::vector<HierarchyNode> children;
	};

	void ReadHierarchy(HierarchyNode& node, const aiNode* source);
	void ReadBones(const aiAnimation* animation);
	void ComputeTransform(const HierarchyNode* node, glm::mat4 parentTransform);
	
	std::map<std::string, BoneInfo> boneInfo;
	std::vector<Bone> bones;
	std::vector<glm::mat4> transforms;
	HierarchyNode root;

	float duration;
	float ticksPerSecond;
	float time;
};

class AnimationManager
{
public:
	static AnimationManager* Get();
	~AnimationManager() { Destroy(); }

	void ComputeAnimations(float delta);
	void ApplyAnimations(VkCommandBuffer commandBuffer);
	void AddAnimation(Animation* animation);
	void RemoveAnimation(Animation* animation);
	void Destroy();

	bool disable = false;

private:
	void Create();
	void CreateShader();

	std::vector<Animation*> animations;

	ComputeShader* computeShader;
	glm::mat4* mat4BufferPtr;
	VkBuffer mat4Buffer;
	VkDeviceMemory mat4Memory;
};