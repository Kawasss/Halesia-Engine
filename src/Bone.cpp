module;

#include "glm.h"

module Renderer.Bone;

import <assimp/anim.h>;

import std;

static glm::vec3 GetVec3(aiVector3D vec) 
{ 
	return glm::vec3(vec.x, vec.y, vec.z);
}

static glm::quat GetQuat(aiQuaternion quat) // if error check this
{ 
	return glm::quat(quat.w, quat.x, quat.y, quat.z);
}


Bone::Bone(const aiNodeAnim* animNode) : time(0), transform(glm::mat4(1.0f)), name(animNode->mNodeName.C_Str())
{
	positions.reserve(animNode->mNumPositionKeys);
	rotations.reserve(animNode->mNumRotationKeys);
	scales.reserve(animNode->mNumScalingKeys);

	for (unsigned int i = 0; i < animNode->mNumPositionKeys; i++)
	{
		const aiVectorKey& key = animNode->mPositionKeys[i];
		positions.emplace_back(GetVec3(key.mValue), (float)key.mTime);
	}
	for (unsigned int i = 0; i < animNode->mNumRotationKeys; i++)
	{
		aiQuatKey& key = animNode->mRotationKeys[i];
		rotations.emplace_back(GetQuat(key.mValue), (float)key.mTime);
	}
	for (unsigned int i = 0; i < animNode->mNumScalingKeys; i++)
	{
		aiVectorKey& key = animNode->mScalingKeys[i];
		scales.emplace_back(GetVec3(key.mValue), (float)key.mTime);
	}
}

glm::mat4 Bone::GetTransform() const
{
	return transform;
}

std::string Bone::GetName() const
{
	return name;
}

void Bone::Update(float time)
{
	this->time = time;
	glm::mat4 translation = InterpolatePosition();
	glm::mat4 rotation = InterpolateRotation();
	glm::mat4 scale = InterpolateScale();
	transform = translation * rotation * scale;
}

int Bone::GetPositionIndex()
{
	for (int i = 0; i < positions.size() - 1; i++)
		if (time < positions[i + 1].timeStamp)
			return i;
	return positions.size() - 2;
	//throw std::runtime_error("Failed to fetch the position index of a bone");
}

int Bone::GetRotationIndex()
{
	for (int i = 0; i < rotations.size() - 1; i++)
		if (time < rotations[i + 1].timeStamp)
			return i;
	return rotations.size() - 2;
	//throw std::runtime_error("Failed to fetch the rotation index of a bone");
}

int Bone::GetScaleIndex()
{
	for (int i = 0; i < scales.size() - 1; i++)
		if (time < scales[i + 1].timeStamp)
			return i;
	return scales.size() - 2;
	//throw std::runtime_error("Failed to fetch the scale index of a bone");
}

float Bone::GetFactor(float lastTime, float nextTime)
{
	return (time - lastTime) / (nextTime - lastTime); // halfway point divided by the difference between times
}

glm::mat4 Bone::InterpolatePosition()
{
	if (positions.empty())
		return glm::translate(glm::mat4(1.0f), positions[0].position);

	int currentIndex = GetPositionIndex();
	int nextIndex = currentIndex + 1;

	if (positions.size() == 1)
		return glm::translate(positions[0].position);

	float scaleFactor = GetFactor(positions[currentIndex].timeStamp, positions[nextIndex].timeStamp);
	glm::vec3 pos = glm::mix(positions[currentIndex].position, positions[nextIndex].position, scaleFactor);
	return glm::translate(glm::mat4(1.0f), pos);
}

glm::mat4 Bone::InterpolateRotation()
{
	if (rotations.empty())
		return glm::toMat4(glm::normalize(rotations[0].orientation));

	int currentIndex = GetRotationIndex();
	int nextIndex = currentIndex + 1;

	if (rotations.size() == 1)
		return glm::toMat4(rotations[0].orientation);

	float scaleFactor = GetFactor(rotations[currentIndex].timeStamp, rotations[nextIndex].timeStamp);
	glm::quat rotation = glm::mix(rotations[currentIndex].orientation, rotations[nextIndex].orientation, scaleFactor);
	rotation = glm::normalize(rotation);
	return glm::toMat4(rotation);
}

glm::mat4 Bone::InterpolateScale()
{
	if (scales.empty())
		return glm::scale(glm::mat4(1.0f), scales[0].scale);

	int currentIndex = GetScaleIndex();
	int nextIndex = currentIndex + 1;

	if (scales.size() == 1)
		return glm::scale(scales[0].scale);

	float scaleFactor = GetFactor(scales[currentIndex].timeStamp, scales[nextIndex].timeStamp);
	glm::vec3 scale = glm::mix(scales[currentIndex].scale, scales[nextIndex].scale, scaleFactor);
	return glm::scale(glm::mat4(1.0f), scale);
}