#pragma once
#include <vector>
#include <string>
#include <memory>
#include <map>

#include "../glm.h"

#include "Bone.h"
#include "Buffer.h"
#include "ComputeShader.h"
#include "Animation.h"

#include "../system/CriticalSection.h"

struct MeshCreationData;
class  ComputeShader;

class AnimationManager
{
public:
	static AnimationManager* Get();

	void ComputeAnimations(float delta);
	void ApplyAnimations(VkCommandBuffer commandBuffer);
	void AddAnimation(const Animation& animation);
	//void RemoveAnimation(Animation* animation);

	void AcquireLock();
	void ReleaseLock();

	bool disable = false;

private:
	void Create();
	void CreateShader();

	std::vector<Animation> animations;

	std::unique_ptr<ComputeShader> computeShader;
	glm::mat4* mat4BufferPtr;
	Buffer mat4Buffer;

	win32::CriticalSection section;
};