#pragma once
#include "Animation.h"

class Animator
{
public:
	Animator(Animation& anim);

private:
	float time = 0.0f;

	Animation& animation;
};