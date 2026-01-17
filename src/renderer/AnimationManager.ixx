module;

#include "../glm.h"

#include "Buffer.h"
#include "CommandBuffer.h"

#include "../system/CriticalSection.h"

export module Renderer.AnimationManager;

import std;

import Renderer.Bone;
import Renderer.Animation;
import Renderer.ComputePipeline;

export class AnimationManager
{
public:
	static AnimationManager* Get();

	void ComputeAnimations(float delta);
	void ApplyAnimations(const CommandBuffer& commandBuffer);
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