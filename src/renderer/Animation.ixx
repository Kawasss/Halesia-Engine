module;

#include "../glm.h"

export module Renderer.Animation;

import <assimp/cimport.h>;
import <assimp/scene.h>;

import std;

import Renderer.Bone;

export class Animation
{
public:
	Animation() = default;
	Animation(const aiAnimation* animation, const aiNode* root);
	Bone* GetBone(std::string_view name);
	std::vector<glm::mat4>& GetTransforms();

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

	std::vector<Bone> bones;
	std::vector<glm::mat4> transforms;
	HierarchyNode root;

	float duration;
	float ticksPerSecond;
	float time;
};