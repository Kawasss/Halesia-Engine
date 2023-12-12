#version 460
#define MAX_MESHES 1000

layout(set = 0, binding = 0) uniform sceneInfo {
    vec3 camPos;
    mat4 view;
    mat4 proj;
} ubo;

layout(push_constant) uniform constant
{
    mat4 model;
} Constant;


vec3 coordinates[36] = vec3[](
	vec3(-1, -1, -1), vec3(1, 1, -1), vec3(1, -1, -1),
    vec3(1, 1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
	
	vec3(-1, -1, 1), vec3(1, -1, 1), vec3(1, 1, 1),
    vec3(1, 1, 1), vec3(-1, 1, 1), vec3(-1, -1, 1),
	
	vec3(-1, 1, 1), vec3(-1, 1, -1), vec3(-1, -1, -1),
    vec3(-1, -1, -1), vec3(-1, -1, 1), vec3(-1, 1, 1),

	vec3(1, 1, 1), vec3(1, -1, -1), vec3(1, 1, -1),
    vec3(1, -1, -1), vec3(1, 1, 1), vec3(1, -1, 1),

	vec3(-1, -1, -1), vec3(1, -1, -1), vec3(1, -1, 1),
    vec3(1, -1, 1), vec3(-1, -1, 1), vec3(-1, -1, -1),

	vec3(-1, 1, -1), vec3(1, 1, 1), vec3(1, 1, -1),
    vec3(1, 1, 1), vec3(-1, 1, -1), vec3(-1, 1, 1)
);

void main() {
    gl_Position = ubo.proj * ubo.view * Constant.model * vec4(coordinates[gl_VertexIndex], 1.0);
}