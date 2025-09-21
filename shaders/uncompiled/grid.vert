#version 460

DECLARE_EXTERNAL_SET(1)

layout(set = 1, binding = scene_data_buffer_binding) uniform SceneData
{
    mat4 view;
    mat4 proj;

    mat4 prevView;
    mat4 prevProj;
    
    mat4 viewInv;
    mat4 projInv;

    vec2 viewportSize;

    float zNear;
    float zFar;

    vec3 camPosition;
    uint frameCount;
} sceneData;

layout (location = 0) out vec3 coor;
layout (location = 1) out vec3 coor3D;
layout (location = 2) out vec3 oPlayerPos;

vec3 coordinates[6] = vec3[](
	vec3(-1, 0, -1), vec3(1, 0, 1), vec3(1, 0, -1),
    vec3(1, 0, 1), vec3(-1, 0, -1), vec3(-1, 0, 1)
);

void main() 
{
	oPlayerPos = sceneData.camPosition;

	vec3 v1 = coordinates[gl_VertexIndex].xyz;
	vec4 scaled =  vec4(v1 * 1000.0, 1.0);
	coor3D = scaled.xyz;

	vec4 clip = sceneData.proj * sceneData.view * scaled;

	coor = clip.xyz;
	gl_Position = clip;
}