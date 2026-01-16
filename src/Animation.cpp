module;

#include "glm.h"

module Renderer.Animation;

import <assimp/cimport.h>;
import <assimp/scene.h>;

import Renderer.Bone;

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

Animation::Animation(const aiAnimation* animation, const aiNode* root) : duration((float)animation->mDuration), ticksPerSecond((float)animation->mTicksPerSecond), time(0), transforms(animation->mNumChannels, glm::mat4(1.0f))
{
	ReadHierarchy(this->root, root);
	ReadBones(animation);
}

void Animation::ReadHierarchy(HierarchyNode& node, const aiNode* source)
{
	node.name = source->mName.C_Str();
	node.transformation = GetMat4(source->mTransformation);
	node.children.resize(source->mNumChildren);

	for (unsigned int i = 0; i < source->mNumChildren; i++)
		ReadHierarchy(node.children[i], source->mChildren[i]);
}

void Animation::ReadBones(const aiAnimation* animation)
{
	bones.resize(animation->mNumChannels);
	for (unsigned int i = 0; i < animation->mNumChannels; i++)
		bones.emplace_back(animation->mChannels[i]);
}

Bone* Animation::GetBone(std::string_view name)
{
	auto index = std::find_if
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
	//glm::mat4 rot = glm::rotate(glm::pi<float>() / 2.0f, glm::vec3(0, 1, 0));
	//if (boneInfo.find(node->name) != boneInfo.end())
	//{
	//	BoneInfo& info = boneInfo.at(node->name);
	//	glm::mat4 res = rot;//globalTrans * info.offset;

	//	if (res == glm::mat4(0.0f) || res != res)
	//		res = glm::mat4(1.0f);

	//	transforms[info.index] = res;
	//}
	//else
	//	Console::WriteLine("bone \"{}\" could not be found", Console::Severity::Warning, node->name);

	//for (int i = 0; i < node->children.size(); i++)
	//	ComputeTransform(&node->children[i], rot);
}