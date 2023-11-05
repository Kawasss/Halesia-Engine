#version 460

layout (binding = 0, set = 0) uniform sampler2D image;
layout (binding = 1, set = 0) uniform Timer
{
	float completionPercentage;
} timer;

layout (location = 0) in vec2 uvCoord;
layout (location = 0) out vec4 fragColor;

void main()
{
	fragColor = vec4(mix(vec3(0), texture(image, uvCoord).xyz, timer.completionPercentage), 1);
}