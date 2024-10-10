#version 460
layout (location = 0) out vec3 TexCoords;

layout (set = 0, binding = 0) uniform Matrices
{
	mat4 projection;
	mat4 view;
} ubo;

vec3 pos[];

void main()
{
	mat4 cView = mat4(mat3(ubo.view));
	vec4 clipPos = vec4(TexCoords, 1.0) * cView * ubo.projection;

	gl_Position = clipPos.xyww;
}