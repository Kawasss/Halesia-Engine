#version 460

layout (binding = 2, set = 0) uniform sampler2D image;

layout (location = 0) in vec2 uvCoord;
layout (location = 0) out vec4 fragColor;

void main()
{
	vec3 color = texture(image, uvCoord).xyz;
	//color = pow(color, vec3(2.2));
	fragColor = vec4(color, 1);
}