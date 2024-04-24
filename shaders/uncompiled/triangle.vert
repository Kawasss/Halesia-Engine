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
layout (location = 4) out flat uint ID;

layout(set = 0, binding = 0) uniform sceneInfo {
    vec3 camPos;
    mat4 view;
    mat4 proj;
} ubo;

layout(push_constant) uniform constant
{
    mat4 model;
} Constant;

struct ModelData
{
    mat4 model;
    vec4 IDColor;
};

layout (set = 0, binding = 1) buffer ModelBuffer
{
    ModelData data[];
} modelBuffer;

void main() 
{
    position = (modelBuffer.data[gl_DrawID].model * vec4(inPosition, 1.0)).xyz;
    normal = mat3(transpose(inverse(modelBuffer.data[gl_DrawID].model))) * inNormal;
    texCoords = inTexCoords;
    camPos = ubo.camPos;
    ID = gl_DrawID;

    gl_Position = ubo.proj * ubo.view * vec4(position, 1);
    position = gl_Position.xyz * 2.0 - 1.0;
}