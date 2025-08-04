#version 460
layout (binding = 0) uniform Constant
{
	mat4 projection;
	mat4 view;
	mat4 model;
	vec3 camPos;
} constant;

layout (location = 0) out vec3 coor;
layout (location = 1) out vec3 coor3D;
layout (location = 2) out vec3 oPlayerPos;

vec3 coordinates[6] = vec3[](
	vec3(-1, 0, -1), vec3(1, 0, 1), vec3(1, 0, -1),
    vec3(1, 0, 1), vec3(-1, 0, -1), vec3(-1, 0, 1)
);

void main() 
{
	oPlayerPos = constant.camPos;

	vec3 v1 = coordinates[gl_VertexIndex].xyz;
	vec4 scaled = constant.model * vec4(v1, 1.0);
	coor3D = scaled.xyz;

	vec4 clip = constant.projection * constant.view * scaled;

	coor = clip.xyz;
	gl_Position = clip;
}