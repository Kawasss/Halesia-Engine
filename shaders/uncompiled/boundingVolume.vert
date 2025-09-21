#version 460
layout (location = 0) in  vec3 inPosition;
layout (location = 1) in  vec3 inNormal;
layout (location = 2) in  vec2 inTexCoords;
layout (location = 3) in  vec3 inTangent;
layout (location = 4) in  vec3 inBiTangent;
layout (location = 5) in ivec4 inBoneIDs;
layout (location = 6) in  vec4 inBoneWeights;

DECLARE_EXTERNAL_SET(2)

layout (binding = 0) buffer UniformData
{
	mat4 models[];
} constants;

layout(set = 2, binding = scene_data_buffer_binding) uniform SceneData
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

void main()
{
	gl_Position = sceneData.proj * sceneData.view * constants.models[gl_InstanceIndex] * vec4(inPosition, 1.0);
}