#version 460
layout (location = 0) out vec4 color;
layout (location = 0) in vec3 texCoords;

layout (binding = 1) uniform samplerCube skybox;

void main()
{
	color = texture(skybox, texCoords); // should maybe also do HDR tonemap and gamma correct here
}