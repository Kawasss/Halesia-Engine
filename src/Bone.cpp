#include <stdexcept>

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "renderer/Bone.h"

inline glm::vec3 GetVec3(aiVector3D vec) { return { vec.x, vec.y, vec.z }; }
inline glm::quat GetQuat(aiQuaternion quat) { return { quat.w, quat.x, quat.y, quat.z }; } // if error check this


Bone::Bone(const aiNodeAnim* animNode) : time(0), transform(glm::mat4(1.0f)), name(animNode->mNodeName.C_Str())
{
	for (int i = 0; i < animNode->mNumPositionKeys; i++)
	{
		aiVectorKey& key = animNode->mPositionKeys[i];
		positions.push_back(KeyPosition{ GetVec3(key.mValue), (float)key.mTime });
	}
	for (int i = 0; i < animNode->mNumRotationKeys; i++)
	{
		aiQuatKey& key = animNode->mRotationKeys[i];
		rotations.push_back(KeyRotation{ GetQuat(key.mValue), (float)key.mTime });
	}
	for (int i = 0; i < animNode->mNumScalingKeys; i++)
	{
		aiVectorKey& key = animNode->mScalingKeys[i];
		scales.push_back(KeyScale{ GetVec3(key.mValue), (float)key.mTime });
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
	for (int i = 0; i < positions.size(); i++)
		if (time < positions[i + 1].timeStamp)
			return i;
	throw std::runtime_error("Failed to fetch the position index of a bone");
}

int Bone::GetRotationIndex()
{
	for (int i = 0; i < rotations.size(); i++)
		if (time < rotations[i + 1].timeStamp)
			return i;
	throw std::runtime_error("Failed to fetch the rotation index of a bone");
}

int Bone::GetScaleIndex()
{
	for (int i = 0; i < scales.size(); i++)
		if (time < scales[i + 1].timeStamp)
			return i;
	throw std::runtime_error("Failed to fetch the scale index of a bone");
}

float Bone::GetFactor(float lastTime, float nextTime)
{
	return (time - lastTime) / (nextTime - lastTime); // halfway point divided by the difference between times
}

glm::mat4 Bone::InterpolatePosition()
{
	if (positions.size() == 1)
		return glm::translate(glm::mat4(1.0f), positions[0].position);

	int currentIndex = GetPositionIndex();
	int nextIndex = currentIndex + 1;

	float scaleFactor = GetFactor(positions[currentIndex].timeStamp, positions[nextIndex].timeStamp);
	glm::vec3 pos = glm::mix(positions[currentIndex].position, positions[nextIndex].position, scaleFactor);
	return glm::translate(glm::mat4(1.0f), pos);
}

glm::mat4 Bone::InterpolateRotation()
{
	if (rotations.size() == 1)
		return glm::toMat4(glm::normalize(rotations[0].orientation));

	int currentIndex = GetRotationIndex();
	int nextIndex = currentIndex + 1;

	float scaleFactor = GetFactor(rotations[currentIndex].timeStamp, rotations[nextIndex].timeStamp);
	glm::quat rotation = glm::mix(rotations[currentIndex].orientation, rotations[nextIndex].orientation, scaleFactor);
	rotation = glm::normalize(rotation);
	return glm::toMat4(rotation);
}

glm::mat4 Bone::InterpolateScale()
{
	if (scales.size() == 1)
		return glm::scale(glm::mat4(1.0f), scales[0].scale);

	int currentIndex = GetScaleIndex();

	if (currentIndex + 1 >= scales.size())
		currentIndex = 0;

	int nextIndex = currentIndex + 1;

	float scaleFactor = GetFactor(scales[currentIndex].timeStamp, scales[nextIndex].timeStamp);
	glm::vec3 scale = glm::mix(scales[currentIndex].scale, scales[nextIndex].scale, scaleFactor);
	return glm::scale(glm::mat4(1.0f), scale);
}