#pragma once
#include "CommandBuffer.h"

class Cubemap;
class Texture;

class HdrConverter // converts a .hdr cubemap into a six sided cubemap
{
public:
	static void Start(); // initialize the converter
	static void End(); // deinitialize

	static void ConvertTextureIntoCubemap(const CommandBuffer& cmdBuffer, const Texture* texture, Cubemap* cubemap);

private:
};