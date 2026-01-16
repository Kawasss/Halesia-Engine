export module Renderer.Animator;

import Renderer.Animation;

export class Animator
{
public:
	Animator(Animation& anim);

private:
	float time = 0.0f;

	Animation& animation;
};