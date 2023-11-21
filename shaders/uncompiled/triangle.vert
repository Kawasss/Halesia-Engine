#version 460
#define MAX_MESHES 1000

layout(set = 0, binding = 0) uniform sceneInfo {
    vec3 camPos;
    mat4 view;
    mat4 proj;
} ubo;

struct PerModelData
{
    mat4 transformation;
    vec4 IDColor;
};

layout (binding = 1) buffer ModelData
{
    PerModelData[] data;
} modelData;

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

layout (location = 0) out      vec3 fragNormal;
layout (location = 1) out      vec2 fragTexCoord;
layout (location = 2) out      vec3 worldPos;
layout (location = 3) out      vec3 camPos;
layout (location = 4) out flat vec4 IDColorIn;
layout (location = 5) out flat int  materialIndex;

void main() {
    gl_Position = ubo.proj * ubo.view * modelData.data[gl_DrawID].transformation * vec4(inPosition, 1.0);

    fragNormal = normalize(mat3(transpose(inverse(modelData.data[gl_DrawID].transformation))) * inNormal);
    fragTexCoord = inTexCoord;
    worldPos = (modelData.data[gl_DrawID].transformation * vec4(inPosition, 1)).xyz;
    camPos = ubo.camPos;
    IDColorIn = modelData.data[gl_DrawID].IDColor;
    materialIndex = gl_DrawID;
}