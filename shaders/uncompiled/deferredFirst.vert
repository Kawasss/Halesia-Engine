#version 460
layout (location = 0) in  vec3 inPosition;
layout (location = 1) in  vec3 inNormal;
layout (location = 2) in  vec2 inTexCoords;
layout (location = 3) in ivec4 inBoneIDs;
layout (location = 4) in  vec4 inBoneWeights;

layout (location = 0) out vec3 position;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec2 texCoords;
layout (location = 3) out vec3 camPos;

layout(set = 0, binding = 0) uniform sceneInfo {
    vec3 camPos;
    mat4 view;
    mat4 proj;
} ubo;

layout(push_constant) uniform constant
{
    mat4 model;
    int materialID;
} Constant;

void main() 
{
    position = (Constant.model * vec4(inPosition, 1.0)).xyz;
    normal = mat3(transpose(inverse(Constant.model))) * inNormal;
    texCoords = inTexCoords;
    camPos = ubo.camPos;

    gl_Position = ubo.proj * ubo.view * vec4(position, 1);
}